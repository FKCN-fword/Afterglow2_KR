#include "SkeeNatives.h"

namespace AG::Skee
{
	namespace
	{
		using namespace AG_ext::skee;

		// Mirror of SKSE 2.0.x BSFixedString (StringCache::Ref), which skee64 0.4.16 was compiled against: one
		// pointer, user-declared constructors, trivial copy and destructor. On x64 MSVC that shape is passed by value
		// in a register and returned through a hidden pointer. RE::BSFixedString has a non-trivial copy/destructor
		// and is passed by reference instead, so it must never appear in the signatures below.
		struct SkseString
		{
			SkseString() noexcept {}
			explicit SkseString(const char* a_data) noexcept :
				data(a_data) {}

			const char* data = nullptr;
		};
		static_assert(sizeof(SkseString) == 8);
		static_assert(std::is_trivially_copyable_v<SkseString> && std::is_trivially_destructible_v<SkseString>);

		using Refr = RE::TESObjectREFR*;
		using U32 = std::uint32_t;

		// papyrusNiOverride::* in skee64 0.4.16; the first parameter is SKSE's StaticFunctionTag*, always unused
		using AddNodeString = void (*)(void*, Refr, bool, SkseString, U32, U32, SkseString, bool);
		using AddNodeFloat = void (*)(void*, Refr, bool, SkseString, U32, U32, float, bool);
		using GetNodeString = SkseString (*)(void*, Refr, bool, SkseString, U32, U32);
		using HasNode = bool (*)(void*, Refr, bool, SkseString, U32, U32);
		using RemoveNode = void (*)(void*, Refr, bool, SkseString, U32, U32);
		using ApplyRefr = void (*)(void*, Refr);
		using HasRefr = bool (*)(void*, Refr);

		struct Natives
		{
			AddNodeString addNodeString = nullptr;  // AddNodeOverrideString
			AddNodeFloat  addNodeFloat = nullptr;   // AddNodeOverrideFloat
			GetNodeString getNodeString = nullptr;  // GetNodeOverrideString (the stored override)
			GetNodeString getPropString = nullptr;  // GetNodePropertyString (what the loaded node shows)
			HasNode       hasNode = nullptr;        // HasNodeOverride
			RemoveNode    removeNode = nullptr;     // RemoveNodeOverride
			ApplyRefr     applyNode = nullptr;      // ApplyNodeOverrides
			HasRefr       hasOverlays = nullptr;    // HasOverlays
			ApplyRefr     addOverlays = nullptr;    // AddOverlays
		};

		// signature letters: v none, o object, b bool, i int, f float, s string (the Papyrus side of each native)
		struct Spec
		{
			std::string_view name;
			char             ret;
			std::string_view params;
			void**           slot;
		};

		// SKSE 2.0.x NativeFunction: NativeFunctionBase is 0x50 bytes, the C++ callback pointer follows it
		constexpr std::uintptr_t kCallbackOffset = 0x50;

		// the one string key Afterglow reads back (shader texture); alpha, glossiness and specular are only written
		constexpr std::uint16_t kTextureKey = 9;

		struct Image
		{
			std::uintptr_t begin = 0;
			std::uintptr_t end = 0;

			[[nodiscard]] bool Contains(std::uintptr_t a_address) const { return a_address >= begin && a_address < end; }
		};

		std::optional<Image> SkeeImage()
		{
			const auto module = ::GetModuleHandleW(L"skee64.dll");
			if (!module) {
				return std::nullopt;
			}
			// SizeOfImage straight from the PE header: e_lfanew at 0x3C, OptionalHeader at NT+0x18, SizeOfImage at +0x38
			const auto base = reinterpret_cast<std::uintptr_t>(module);
			const auto nt = base + *reinterpret_cast<const std::uint32_t*>(base + 0x3C);
			const auto size = *reinterpret_cast<const std::uint32_t*>(nt + 0x18 + 0x38);
			return Image{ base, base + size };
		}

		char Letter(const RE::BSScript::TypeInfo& a_type)
		{
			if (a_type.IsBool()) {
				return 'b';
			}
			if (a_type.IsInt()) {
				return 'i';
			}
			if (a_type.IsFloat()) {
				return 'f';
			}
			if (a_type.IsString()) {
				return 's';
			}
			if (a_type.IsObject()) {
				return 'o';
			}
			if (a_type.GetRawType() == RE::BSScript::TypeInfo::RawType::kNone) {
				return 'v';
			}
			return '?';
		}

		// Finds a_spec in NiOverride's global function table and stores its C++ callback in a_spec.slot. Every check
		// must pass: the Papyrus signature matches the function pointer type we will call through, and both the
		// object's vtable and the callback live inside skee64.dll (so it is an SKSE NativeFunction skee registered).
		bool Resolve(const RE::BSScript::ObjectTypeInfo& a_type, const Image& a_skee, const Spec& a_spec)
		{
			const auto* funcs = a_type.GetGlobalFuncIter();
			for (std::uint32_t i = 0; funcs && i < a_type.GetNumGlobalFuncs(); ++i) {
				auto* fn = funcs[i].func.get();
				if (!fn || fn->GetName() != a_spec.name) {
					continue;
				}
				if (!fn->GetIsNative() || !fn->GetIsStatic()) {
					logger::error("NiOverride.{} is not a global native", a_spec.name);
					return false;
				}
				std::string signature(1, Letter(fn->GetReturnType()));
				signature += ':';
				for (std::uint32_t p = 0; p < fn->GetParamCount(); ++p) {
					RE::BSFixedString      name;
					RE::BSScript::TypeInfo type;
					fn->GetParam(p, name, type);
					signature += Letter(type);
				}
				const auto expected = std::string(1, a_spec.ret) + ':' + std::string(a_spec.params);
				if (signature != expected) {
					logger::error("NiOverride.{} has signature {}, expected {}", a_spec.name, signature, expected);
					return false;
				}
				const auto object = reinterpret_cast<std::uintptr_t>(fn);
				const auto vtable = *reinterpret_cast<const std::uintptr_t*>(object);
				const auto callback = *reinterpret_cast<const std::uintptr_t*>(object + kCallbackOffset);
				if (!a_skee.Contains(vtable) || !a_skee.Contains(callback)) {
					logger::error("NiOverride.{} is not skee64's own native (vtable {:X}, callback {:X}, skee64 {:X}-{:X})",
						a_spec.name, vtable, callback, a_skee.begin, a_skee.end);
					return false;
				}
				*a_spec.slot = reinterpret_cast<void*>(callback);
				logger::info("  NiOverride.{} -> skee64+{:X}", a_spec.name, callback - a_skee.begin);
				return true;
			}
			logger::error("NiOverride.{} not found", a_spec.name);
			return false;
		}

		bool Unsupported(std::string_view a_what)
		{
			static std::mutex                      lock;
			static std::unordered_set<std::string> reported;
			std::scoped_lock                       guard(lock);
			if (reported.emplace(a_what).second) {
				logger::warn("{} is not available through RaceMenu 0.4.16's NiOverride natives; ignored", a_what);
			}
			return false;
		}

		class NativeOverrides final : public IOverrideInterface
		{
		public:
			explicit NativeOverrides(const Natives& a_natives) :
				_n(a_natives) {}

			skee_u32 GetVersion() override { return kPluginVersion2; }
			void     Revert() override {}

			// --- node overrides: what Afterglow uses (overlay nodes "Body [Ovl7]" ...)

			bool HasNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index) override
			{
				if (!refr || !nodeName) {
					return false;
				}
				const RE::BSFixedString node(nodeName);
				return _n.hasNode(nullptr, refr, isFemale, SkseString(node.data()), key, index);
			}

			// stored (persist = true): saved to the co-save, re-applied after armor swaps and 3D rebuilds
			void AddNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value) override
			{
				Add(refr, isFemale, nodeName, key, index, value, true);
			}

			bool GetNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor) override
			{
				// the native returns "" for a missing override, so existence comes from Has first
				if (!HasNodeOverride(refr, isFemale, nodeName, key, index)) {
					return false;
				}
				if (key != kTextureKey) {
					return Unsupported("GetNodeOverride of a non-string key");
				}
				const RE::BSFixedString node(nodeName);
				const auto              result = _n.getNodeString(nullptr, refr, isFemale, SkseString(node.data()), key, index);
				visitor.String(result.data ? result.data : "");
				return true;
			}

			void RemoveNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index) override
			{
				if (!refr || !nodeName) {
					return;
				}
				const RE::BSFixedString node(nodeName);
				_n.removeNode(nullptr, refr, isFemale, SkseString(node.data()), key, index);
			}

			void SetNodeProperties(RE::TESObjectREFR* refr, bool) override
			{
				if (refr) {
					_n.applyNode(nullptr, refr);
				}
			}

			// live only (persist = false): per NiOverride.psc "apply the change visually until the armor is
			// re-equipped or the game is reloaded (Equivalent to SetPropertyX)". 0.4.16 has no SetNodeProperty native.
			void SetNodeProperty(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool) override
			{
				Add(refr, isFemale, nodeName, key, index, value, false);
			}

			// what the loaded node shows right now (GetNodePropertyString)
			bool GetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value) override
			{
				if (!refr || !nodeName) {
					return false;
				}
				if (key != kTextureKey) {
					return Unsupported("GetNodeProperty of a non-string key");
				}
				const RE::BSFixedString node(nodeName);
				const auto              result = _n.getPropString(nullptr, refr, firstPerson, SkseString(node.data()), key, index);
				value.String(result.data ? result.data : "");
				return true;
			}

			// --- not used by Afterglow; not wired to RaceMenu 0.4.16

			bool HasArmorAddonNode(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, bool) override { return Unsupported("HasArmorAddonNode"); }
			bool HasArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, skee_u16, skee_u8) override { return Unsupported("HasArmorOverride"); }
			void AddArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, skee_u16, skee_u8, SetVariant&) override { Unsupported("AddArmorOverride"); }
			bool GetArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, skee_u16, skee_u8, GetVariant&) override { return Unsupported("GetArmorOverride"); }
			void RemoveArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, skee_u16, skee_u8) override { Unsupported("RemoveArmorOverride"); }
			void SetArmorProperties(RE::TESObjectREFR*, bool) override { Unsupported("SetArmorProperties"); }
			void SetArmorProperty(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, skee_u16, skee_u8, SetVariant&, bool) override { Unsupported("SetArmorProperty"); }
			bool GetArmorProperty(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, skee_u16, skee_u8, GetVariant&) override { return Unsupported("GetArmorProperty"); }
			void ApplyArmorOverrides(RE::TESObjectREFR*, RE::TESObjectARMO*, RE::TESObjectARMA*, RE::NiAVObject*, bool) override { Unsupported("ApplyArmorOverrides"); }
			void RemoveAllArmorOverrides() override { Unsupported("RemoveAllArmorOverrides"); }
			void RemoveAllArmorOverridesByReference(RE::TESObjectREFR*) override { Unsupported("RemoveAllArmorOverridesByReference"); }
			void RemoveAllArmorOverridesByArmor(RE::TESObjectREFR*, bool, RE::TESObjectARMO*) override { Unsupported("RemoveAllArmorOverridesByArmor"); }
			void RemoveAllArmorOverridesByAddon(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*) override { Unsupported("RemoveAllArmorOverridesByAddon"); }
			void RemoveAllArmorOverridesByNode(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*) override { Unsupported("RemoveAllArmorOverridesByNode"); }

			void ApplyNodeOverrides(RE::TESObjectREFR*, RE::NiAVObject*, bool) override { Unsupported("ApplyNodeOverrides(object)"); }
			void RemoveAllNodeOverrides() override { Unsupported("RemoveAllNodeOverrides"); }
			void RemoveAllNodeOverridesByReference(RE::TESObjectREFR*) override { Unsupported("RemoveAllNodeOverridesByReference"); }
			void RemoveAllNodeOverridesByNode(RE::TESObjectREFR*, bool, const char*) override { Unsupported("RemoveAllNodeOverridesByNode"); }

			bool HasSkinOverride(RE::TESObjectREFR*, bool, bool, skee_u32, skee_u16, skee_u8) override { return Unsupported("HasSkinOverride"); }
			void AddSkinOverride(RE::TESObjectREFR*, bool, bool, skee_u32, skee_u16, skee_u8, SetVariant&) override { Unsupported("AddSkinOverride"); }
			bool GetSkinOverride(RE::TESObjectREFR*, bool, bool, skee_u32, skee_u16, skee_u8, GetVariant&) override { return Unsupported("GetSkinOverride"); }
			void RemoveSkinOverride(RE::TESObjectREFR*, bool, bool, skee_u32, skee_u16, skee_u8) override { Unsupported("RemoveSkinOverride"); }
			void SetSkinProperties(RE::TESObjectREFR*, bool) override { Unsupported("SetSkinProperties"); }
			void SetSkinProperty(RE::TESObjectREFR*, bool, skee_u32, skee_u16, skee_u8, SetVariant&, bool) override { Unsupported("SetSkinProperty"); }
			bool GetSkinProperty(RE::TESObjectREFR*, bool, skee_u32, skee_u16, skee_u8, GetVariant&) override { return Unsupported("GetSkinProperty"); }
			void ApplySkinOverrides(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, skee_u32, RE::NiAVObject*, bool) override { Unsupported("ApplySkinOverrides(object)"); }
			void RemoveAllSkinOverrides() override { Unsupported("RemoveAllSkinOverrides"); }
			void RemoveAllSkinOverridesByReference(RE::TESObjectREFR*) override { Unsupported("RemoveAllSkinOverridesByReference"); }
			void RemoveAllSkinOverridesBySlot(RE::TESObjectREFR*, bool, bool, skee_u32) override { Unsupported("RemoveAllSkinOverridesBySlot"); }

		private:
			void Add(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool persist)
			{
				if (!refr || !nodeName) {
					return;
				}
				const RE::BSFixedString node(nodeName);
				switch (value.GetType()) {
				case SetVariant::Type::String:
					{
						const RE::BSFixedString interned(value.String() ? value.String() : "");
						_n.addNodeString(nullptr, refr, isFemale, SkseString(node.data()), key, index, SkseString(interned.data()), persist);
						break;
					}
				case SetVariant::Type::Float:
					_n.addNodeFloat(nullptr, refr, isFemale, SkseString(node.data()), key, index, value.Float(), persist);
					break;
				default:
					Unsupported("a node override with a non string/float value");
					break;
				}
			}

			Natives _n;
		};

		class NativeOverlay final : public IOverlayInterface
		{
		public:
			explicit NativeOverlay(const Natives& a_natives) :
				_n(a_natives) {}

			skee_u32 GetVersion() override { return kPluginVersion2; }
			void     Revert() override {}

			bool HasOverlays(RE::TESObjectREFR* reference) override
			{
				return reference && _n.hasOverlays(nullptr, reference);
			}

			// The native has no "immediate" flag (SKEE's Papyrus AddOverlays always uses its default), so the overlay
			// nodes of an NPC may only exist a moment after this call; the stored overrides are applied once they do.
			void AddOverlays(RE::TESObjectREFR* reference, bool) override
			{
				if (reference) {
					_n.addOverlays(nullptr, reference);
				}
			}

			void     RemoveOverlays(RE::TESObjectREFR*, bool) override { Unsupported("RemoveOverlays"); }
			void     RevertOverlays(RE::TESObjectREFR*, bool, bool) override { Unsupported("RevertOverlays"); }
			void     RevertOverlay(RE::TESObjectREFR*, const char*, skee_u32, skee_u32, bool, bool) override { Unsupported("RevertOverlay"); }
			void     EraseOverlays(RE::TESObjectREFR*, bool) override { Unsupported("EraseOverlays"); }
			void     RevertHeadOverlays(RE::TESObjectREFR*, bool, bool) override { Unsupported("RevertHeadOverlays"); }
			void     RevertHeadOverlay(RE::TESObjectREFR*, const char*, skee_u32, skee_u32, bool, bool) override { Unsupported("RevertHeadOverlay"); }
			skee_u32 GetOverlayCount(OverlayType, OverlayLocation) override
			{
				Unsupported("GetOverlayCount");
				return 0;
			}
			const char* GetOverlayFormat(OverlayType, OverlayLocation) override
			{
				Unsupported("GetOverlayFormat");
				return nullptr;
			}
			bool RegisterInstallCallback(const char*, OverlayInstallCallback) override { return Unsupported("RegisterInstallCallback"); }
			bool UnregisterInstallCallback(const char*) override { return Unsupported("UnregisterInstallCallback"); }

		private:
			Natives _n;
		};
	}

	bool ConnectNatives(IOverrideInterface*& a_override, IOverlayInterface*& a_overlay)
	{
		const auto skee = SkeeImage();
		if (!skee) {
			logger::error("skee64.dll is not loaded; Afterglow cannot draw overlays");
			return false;
		}

		auto*                                             vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
		if (!vm || !vm->GetScriptObjectType(RE::BSFixedString("NiOverride"), type) || !type) {
			logger::error("Papyrus has no NiOverride script type (RaceMenu scripts missing?); Afterglow cannot draw overlays");
			return false;
		}

		Natives    n;
		const Spec specs[] = {
			{ "AddNodeOverrideString", 'v', "obsiisb", reinterpret_cast<void**>(&n.addNodeString) },
			{ "AddNodeOverrideFloat", 'v', "obsiifb", reinterpret_cast<void**>(&n.addNodeFloat) },
			{ "GetNodeOverrideString", 's', "obsii", reinterpret_cast<void**>(&n.getNodeString) },
			{ "GetNodePropertyString", 's', "obsii", reinterpret_cast<void**>(&n.getPropString) },
			{ "HasNodeOverride", 'b', "obsii", reinterpret_cast<void**>(&n.hasNode) },
			{ "RemoveNodeOverride", 'v', "obsii", reinterpret_cast<void**>(&n.removeNode) },
			{ "ApplyNodeOverrides", 'v', "o", reinterpret_cast<void**>(&n.applyNode) },
			{ "HasOverlays", 'b', "o", reinterpret_cast<void**>(&n.hasOverlays) },
			{ "AddOverlays", 'v', "o", reinterpret_cast<void**>(&n.addOverlays) },
		};

		logger::info("Resolving RaceMenu NiOverride natives (skee64 {:X}-{:X})", skee->begin, skee->end);
		bool ok = true;
		for (const auto& spec : specs) {
			ok = Resolve(*type, *skee, spec) && ok;
		}
		if (!ok) {
			logger::error("Some NiOverride natives did not validate; Afterglow cannot draw overlays");
			return false;
		}

		static NativeOverrides overrides(n);
		static NativeOverlay   overlay(n);
		a_override = &overrides;
		a_overlay = &overlay;
		logger::info("Connected to RaceMenu NiOverride natives ({} of {})", std::size(specs), std::size(specs));
		return true;
	}
}
