#pragma once

#include "core/Core.h"

namespace AG::Skee
{
	// RaceMenu (SKEE) interfaces, fetched once after all plugins loaded.
	bool Init();
	bool Ready();
	std::string_view Status();  // human readable, for the menu and the log

	// overlay slots per area from skee64.ini [Overlays/<Area>] iNumOverlays
	int NumSlots(Area a_area);

	std::string NodeName(Area a_area, int a_slot);  // "Body [Ovl7]"

	// Must run on the main thread (inside an SKSE task).
	void EnsureOverlays(RE::Actor* a_actor);  // NPCs get SKEE overlay nodes now, not on a deferred task
	// texture of a slot: the stored override, else what the loaded node shows; "" = nothing
	std::string SlotTexture(RE::Actor* a_actor, bool a_female, Area a_area, int a_slot);
	// stores the override (survives armor swaps, 3D rebuilds and save/load) and shows it at once
	void Paint(RE::Actor* a_actor, bool a_female, Area a_area, int a_slot, const std::string& a_diffuse, const std::string& a_normal, float a_alpha);
	// marks a loaded slot NoDynamicNormalMap=2 (Mu Dynamic NormalMap's opt-out); true when the mark was missing.
	// The mark lives on the 3D, so it is lost whenever SKEE rebuilds the overlay nodes - callers re-check.
	bool ShieldFromMu(RE::Actor* a_actor, Area a_area, int a_slot);
	// sets our normal map on the loaded node again (after Mu wrote its own there)
	void ShowNormal(RE::Actor* a_actor, Area a_area, int a_slot, const std::string& a_normal);
	// live alpha only (fade animation), nothing stored
	void SetLiveAlpha(RE::Actor* a_actor, Area a_area, int a_slot, float a_alpha);
	// removes our overrides from the slot and blanks the loaded node
	void Erase(RE::Actor* a_actor, bool a_female, Area a_area, int a_slot);
}
