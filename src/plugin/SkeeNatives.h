#pragma once

#include "SKEE_IPluginInterface.h"

// RaceMenu SE 0.4.16 (Skyrim 1.5.97) does not hand out a usable "Override"/"Overlay" interface: what it answers
// with is skee's own implementation (different vtables, private string table), not the 0.4.19 header this plugin
// is written against. Its NiOverride Papyrus natives, however, are plain C++ functions of skee64.dll that call the
// same override code. This resolves them through the Papyrus VM's function table and calls them directly (no
// script, no VM queue), behind the same IOverrideInterface / IOverlayInterface the rest of the plugin uses.
//
// Approach and ABI handling are taken from WetFunctionNG's SkeeNatives (contributed there by GSVJinx for 1.5.97).

namespace AG::Skee
{
	// Resolves and validates every NiOverride native Afterglow needs. On success the two out parameters point at
	// adapters and true is returned; on any mismatch nothing is set, a line is logged and false is returned.
	// Call once the Papyrus VM exists (kDataLoaded or later).
	bool ConnectNatives(AG_ext::skee::IOverrideInterface*& a_override, AG_ext::skee::IOverlayInterface*& a_overlay);
}
