#include "Skee.h"

#include "Settings.h"

#include "SKEE_IPluginInterface.h"
#include "SkeeNatives.h"

namespace AG::Skee
{
	namespace
	{
		using namespace AG_ext::skee;

		IOverrideInterface* g_override{ nullptr };
		IOverlayInterface*  g_overlay{ nullptr };
		std::array<int, kAreaCount> g_slots{ 3, 3, 3, 3 };
		std::string g_status{ "RaceMenu not checked yet" };

		// NiOverride keys (same numbers as the Papyrus API)
		constexpr std::uint16_t kKeyGlossiness = 2;
		constexpr std::uint16_t kKeySpecular = 3;
		constexpr std::uint16_t kKeyAlpha = 8;
		constexpr std::uint16_t kKeyTexture = 9;
		constexpr auto          kDefaultTexture = "actors\\character\\overlays\\default.dds";

		class SetString : public IOverrideInterface::SetVariant
		{
		public:
			explicit SetString(const std::string& a_value) :
				_value(a_value) {}
			Type        GetType() override { return Type::String; }
			const char* String() override { return _value.c_str(); }

		private:
			const std::string& _value;
		};

		class SetFloat : public IOverrideInterface::SetVariant
		{
		public:
			explicit SetFloat(float a_value) :
				_value(a_value) {}
			Type  GetType() override { return Type::Float; }
			float Float() override { return _value; }

		private:
			float _value;
		};

		class GetString : public IOverrideInterface::GetVariant
		{
		public:
			void Int(const skee_i32) override {}
			void Float(const float) override {}
			void String(const char* a_str) override { value = a_str ? a_str : ""; }
			void Bool(const bool) override {}
			void TextureSet(const RE::BGSTextureSet*) override {}
			std::string value;
		};

		int ReadIniInt(const char* a_section, int a_default)
		{
			return static_cast<int>(GetPrivateProfileIntA(a_section, "iNumOverlays", a_default, "Data\\SKSE\\Plugins\\skee64.ini"));
		}
	}

	namespace
	{
		void LoadSlots()
		{
			g_slots[static_cast<std::size_t>(Area::Body)] = ReadIniInt("Overlays/Body", 6);
			g_slots[static_cast<std::size_t>(Area::Hands)] = ReadIniInt("Overlays/Hands", 3);
			g_slots[static_cast<std::size_t>(Area::Feet)] = ReadIniInt("Overlays/Feet", 3);
			g_slots[static_cast<std::size_t>(Area::Face)] = ReadIniInt("Overlays/Face", 3);
		}
	}

	bool Init()
	{
		InterfaceExchangeMessage msg;
		SKSE::GetMessagingInterface()->Dispatch(InterfaceExchangeMessage::kMessage_ExchangeInterface, &msg, sizeof(InterfaceExchangeMessage*), "SKEE");
		if (msg.interfaceMap) {
			auto*          ov = static_cast<IOverrideInterface*>(msg.interfaceMap->QueryInterface("Override"));
			auto*          ol = static_cast<IOverlayInterface*>(msg.interfaceMap->QueryInterface("Overlay"));
			const skee_u32 ovVer = ov ? ov->GetVersion() : 0;
			const skee_u32 olVer = ol ? ol->GetVersion() : 0;
			if (ov && ol && ovVer >= IOverrideInterface::kPluginVersion2) {
				g_override = ov;
				g_overlay = ol;
				LoadSlots();
				g_status = std::format("RaceMenu OK (override v{}, overlay v{}); slots body {} face {} hands {} feet {}", ovVer, olVer,
					g_slots[0], g_slots[1], g_slots[2], g_slots[3]);
				logger::info("{}", g_status);
				return true;
			}
			// RaceMenu 0.4.16 (Skyrim 1.5.97): the interfaces skee64 hands out are not the 0.4.19 ones this plugin
			// is written against, so they are never called. The NiOverride natives take over (ConnectLegacy).
			logger::info("skee64 offers no Override interface v2 (override {} v{}, overlay {} v{}); will use its NiOverride natives after data load",
				static_cast<void*>(ov), ovVer, static_cast<void*>(ol), olVer);
		} else {
			logger::info("skee64 did not answer the interface exchange; will try its NiOverride natives after data load");
		}
		g_status = "RaceMenu interface v2 not found - trying the RaceMenu 0.4.16 NiOverride natives after data load";
		return false;
	}

	bool ConnectLegacy()
	{
		if (g_override) {
			return true;
		}
		IOverrideInterface* ov = nullptr;
		IOverlayInterface*  ol = nullptr;
		if (!ConnectNatives(ov, ol)) {
			g_status = "RaceMenu (skee64.dll) NiOverride natives unavailable - Afterglow cannot draw overlays (see Afterglow.log)";
			return false;
		}
		g_override = ov;
		g_overlay = ol;
		LoadSlots();
		g_status = std::format("RaceMenu 0.4.16 NiOverride natives OK; slots body {} face {} hands {} feet {}", g_slots[0], g_slots[1], g_slots[2], g_slots[3]);
		logger::info("{}", g_status);
		return true;
	}

	bool             Ready() { return g_override != nullptr; }
	std::string_view Status() { return g_status; }
	int              NumSlots(Area a_area) { return g_slots[static_cast<std::size_t>(a_area)]; }

	std::string NodeName(Area a_area, int a_slot) { return std::format("{} [Ovl{}]", AreaName(a_area), a_slot); }

	void EnsureOverlays(RE::Actor* a_actor)
	{
		if (!g_overlay || !a_actor || a_actor->IsPlayerRef()) {
			return;  // the player always has overlay nodes
		}
		if (!g_overlay->HasOverlays(a_actor)) {
			// NB: SKEE's implementation reads this flag as "immediate" (OverlayInterface::AddOverlays):
			// true builds the nodes inline, so the paint below lands on the first orgasm.
			g_overlay->AddOverlays(a_actor, true);
		}
	}

	std::string SlotTexture(RE::Actor* a_actor, bool a_female, Area a_area, int a_slot)
	{
		if (!g_override) {
			return {};
		}
		const auto node = NodeName(a_area, a_slot);
		GetString  stored;
		if (g_override->GetNodeOverride(a_actor, a_female, node.c_str(), kKeyTexture, 0, stored) && !stored.value.empty()) {
			return stored.value;
		}
		GetString live;
		if (a_actor->Is3DLoaded() && g_override->GetNodeProperty(a_actor, false, node.c_str(), kKeyTexture, 0, live)) {
			return live.value;
		}
		return {};
	}

	namespace
	{
		// Mu Dynamic NormalMap (ApplyOverlay = true) puts its generated skin normal map on every RaceMenu overlay,
		// ours included, so the drops lose their relief. Its documented opt-out is this extra data on a mesh:
		// 2 = neither used to build nor given the generated map. Only our own slots get it.
		constexpr const char* kNoDynamicNormalMap = "NoDynamicNormalMap";

		RE::NiAVObject* OverlayNode(RE::Actor* a_actor, const std::string& a_node)
		{
			auto* root = a_actor && a_actor->Is3DLoaded() ? a_actor->Get3D(false) : nullptr;
			return root ? root->GetObjectByName(RE::BSFixedString(a_node.c_str())) : nullptr;
		}
	}

	bool ShieldFromMu(RE::Actor* a_actor, Area a_area, int a_slot)
	{
		auto* obj = OverlayNode(a_actor, NodeName(a_area, a_slot));
		if (!obj) {
			return false;
		}
		const RE::BSFixedString key(kNoDynamicNormalMap);
		if (auto* ed = obj->GetExtraData<RE::NiIntegerExtraData>(key); ed && ed->value == 2) {
			return false;
		}
		obj->RemoveExtraData(key);
		obj->AddExtraData(RE::NiIntegerExtraData::Create(key, 2));
		return true;
	}

	void ShowNormal(RE::Actor* a_actor, Area a_area, int a_slot, const std::string& a_normal)
	{
		if (!g_override || a_normal.empty() || !a_actor->Is3DLoaded()) {
			return;
		}
		const auto node = NodeName(a_area, a_slot);
		SetString  normal(a_normal);
		g_override->SetNodeProperty(a_actor, false, node.c_str(), kKeyTexture, 1, normal, true);
	}

	void Paint(RE::Actor* a_actor, bool a_female, Area a_area, int a_slot, const std::string& a_diffuse, const std::string& a_normal, float a_alpha)
	{
		if (!g_override) {
			return;
		}
		const auto node = NodeName(a_area, a_slot);
		ShieldFromMu(a_actor, a_area, a_slot);  // before the normal is set, so Mu never takes the slot over
		SetString  diffuse(a_diffuse);
		SetFloat   alpha(a_alpha);
		g_override->AddNodeOverride(a_actor, a_female, node.c_str(), kKeyTexture, 0, diffuse);
		if (!a_normal.empty()) {
			SetString normal(a_normal);
			g_override->AddNodeOverride(a_actor, a_female, node.c_str(), kKeyTexture, 1, normal);
		} else {
			g_override->RemoveNodeOverride(a_actor, a_female, node.c_str(), kKeyTexture, 1);
		}
		g_override->AddNodeOverride(a_actor, a_female, node.c_str(), kKeyAlpha, 0xFF, alpha);
		const auto& s = Settings::Get();
		SetFloat    gloss(s.glossiness);
		SetFloat    spec(s.specular);
		if (s.glossiness > 0.0f) {
			g_override->AddNodeOverride(a_actor, a_female, node.c_str(), kKeyGlossiness, 0xFF, gloss);
		} else {
			g_override->RemoveNodeOverride(a_actor, a_female, node.c_str(), kKeyGlossiness, 0xFF);
		}
		if (s.specular > 0.0f) {
			g_override->AddNodeOverride(a_actor, a_female, node.c_str(), kKeySpecular, 0xFF, spec);
		} else {
			g_override->RemoveNodeOverride(a_actor, a_female, node.c_str(), kKeySpecular, 0xFF);
		}
		if (a_actor->Is3DLoaded()) {
			if (s.glossiness > 0.0f) {
				g_override->SetNodeProperty(a_actor, false, node.c_str(), kKeyGlossiness, 0xFF, gloss, true);
			}
			if (s.specular > 0.0f) {
				g_override->SetNodeProperty(a_actor, false, node.c_str(), kKeySpecular, 0xFF, spec, true);
			}
			g_override->SetNodeProperty(a_actor, false, node.c_str(), kKeyTexture, 0, diffuse, true);
			if (!a_normal.empty()) {
				SetString normal(a_normal);
				g_override->SetNodeProperty(a_actor, false, node.c_str(), kKeyTexture, 1, normal, true);
			}
			g_override->SetNodeProperty(a_actor, false, node.c_str(), kKeyAlpha, 0xFF, alpha, true);
		}
	}

	void SetLiveAlpha(RE::Actor* a_actor, Area a_area, int a_slot, float a_alpha)
	{
		if (!g_override || !a_actor->Is3DLoaded()) {
			return;
		}
		const auto node = NodeName(a_area, a_slot);
		SetFloat   alpha(a_alpha);
		g_override->SetNodeProperty(a_actor, false, node.c_str(), kKeyAlpha, 0xFF, alpha, true);
	}

	void Erase(RE::Actor* a_actor, bool a_female, Area a_area, int a_slot)
	{
		if (!g_override) {
			return;
		}
		const auto node = NodeName(a_area, a_slot);
		g_override->RemoveNodeOverride(a_actor, a_female, node.c_str(), kKeyTexture, 0);
		g_override->RemoveNodeOverride(a_actor, a_female, node.c_str(), kKeyTexture, 1);
		g_override->RemoveNodeOverride(a_actor, a_female, node.c_str(), kKeyAlpha, 0xFF);
		g_override->RemoveNodeOverride(a_actor, a_female, node.c_str(), kKeyGlossiness, 0xFF);
		g_override->RemoveNodeOverride(a_actor, a_female, node.c_str(), kKeySpecular, 0xFF);
		if (auto* obj = OverlayNode(a_actor, node)) {
			obj->RemoveExtraData(RE::BSFixedString(kNoDynamicNormalMap));  // a free slot is Mu's again (tattoos etc.)
		}
		if (a_actor->Is3DLoaded()) {
			const std::string blank = kDefaultTexture;
			SetString         diffuse(blank);
			SetFloat          alpha(1.0f);
			g_override->SetNodeProperty(a_actor, false, node.c_str(), kKeyTexture, 0, diffuse, true);
			g_override->SetNodeProperty(a_actor, false, node.c_str(), kKeyAlpha, 0xFF, alpha, true);
		}
	}
}
