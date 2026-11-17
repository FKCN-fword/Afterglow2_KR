#include "Body.h"

namespace AG::Body
{
	namespace
	{
		std::mutex                                g_lock;
		std::unordered_map<std::uint64_t, bool>   g_cache;  // (race, skin, sex) -> UBE

		bool ContainsNoCase(std::string_view a_text, std::string_view a_needle)
		{
			if (a_needle.empty() || a_text.size() < a_needle.size()) {
				return false;
			}
			for (std::size_t i = 0; i + a_needle.size() <= a_text.size(); ++i) {
				bool ok = true;
				for (std::size_t j = 0; j < a_needle.size() && ok; ++j) {
					auto a = a_text[i + j];
					auto b = a_needle[j];
					if (a == '/') {
						a = '\\';
					}
					ok = std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
				}
				if (ok) {
					return true;
				}
			}
			return false;
		}

		const char* BodyModel(RE::Actor* a_actor, RE::TESObjectARMO* a_skin, RE::TESRace* a_race, bool a_female)
		{
			if (!a_skin) {
				return nullptr;
			}
			for (auto* addon : a_skin->armorAddons) {
				if (!addon || !addon->IsValidRace(a_race)) {
					continue;
				}
				if (!addon->GetSlotMask().any(RE::BGSBipedObjectForm::BipedObjectSlot::kBody)) {
					continue;
				}
				const auto* model = addon->bipedModels[a_female ? RE::SEXES::kFemale : RE::SEXES::kMale].GetModel();
				if (model && *model) {
					return model;
				}
			}
			(void)a_actor;
			return nullptr;
		}
	}

	bool IsUBE(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		auto*      race = a_actor->GetRace();
		auto*      skin = a_actor->GetSkin();
		const auto* base = a_actor->GetActorBase();
		const bool female = base && base->GetSex() == RE::SEXES::kFemale;
		const std::uint64_t key = (std::uint64_t(race ? race->GetFormID() : 0) << 32) ^ (std::uint64_t(skin ? skin->GetFormID() : 0) << 1) ^ std::uint64_t(female);
		{
			std::scoped_lock l(g_lock);
			if (auto it = g_cache.find(key); it != g_cache.end()) {
				return it->second;
			}
		}
		bool ube = race && ContainsNoCase(race->GetFormEditorID(), "UBE");
		if (!ube) {
			const char* model = BodyModel(a_actor, skin, race, female);
			ube = model && ContainsNoCase(model, "!UBE\\");
		}
		std::scoped_lock l(g_lock);
		g_cache[key] = ube;
		return ube;
	}

	std::string Describe(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return "none";
		}
		auto*       race = a_actor->GetRace();
		const auto* base = a_actor->GetActorBase();
		const bool  female = base && base->GetSex() == RE::SEXES::kFemale;
		const char* model = BodyModel(a_actor, a_actor->GetSkin(), race, female);
		return std::format("race '{}', body '{}' -> {}", race ? race->GetFormEditorID() : "?", model ? model : "?", IsUBE(a_actor) ? "UBE" : "CBBE/3BA");
	}

	void ClearCache()
	{
		std::scoped_lock l(g_lock);
		g_cache.clear();
	}
}
