#pragma once

namespace AG::Body
{
	// UBE UV layout or the CBBE/3BA one. Decided by the body the actor really wears: the skin's
	// body-slot addon model path contains "!UBE\" (the same rule OBody NG 5.2 uses), or the race
	// EditorID contains "UBE". Read natively, so RaceCompatibilityCondition is not needed.
	bool IsUBE(RE::Actor* a_actor);
	std::string Describe(RE::Actor* a_actor);  // for the menu / log: race EDID + body model path
	void ClearCache();
}
