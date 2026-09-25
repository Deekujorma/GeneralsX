#include "GameClient/KeyBindingRules.h"

#include <cassert>
#include <iostream>

int main()
{
	using namespace KeyBindingRules;
	assert(AreLogicalPartners("BEGIN_CAMERA_ROTATE_LEFT", "END_CAMERA_ROTATE_LEFT"));
	assert(AreLogicalPartners("END_FORCEMOVE", "BEGIN_FORCEMOVE"));
	assert(!AreLogicalPartners("BEGIN_FORCEMOVE", "END_FORCEATTACK"));
	assert(BindingsConflict(1, 0, 2, 1, 0, 2, false));
	// DOWN/UP transition is intentionally not an input to physical conflict policy.
	assert(!BindingsConflict(1, 1, 2, 1, 0, 2, false));
	assert(!BindingsConflict(1, 0, 1, 1, 0, 2, false));
	assert(!BindingsConflict(1, 0, 2, 1, 0, 2, true));
	const char *separator = nullptr;
	assert(SplitOverride("KEY_W,SHIFT", separator) && std::strcmp(separator + 1, "SHIFT") == 0);
	assert(SplitOverride("KEY_NONE,NONE", separator));
	assert(!SplitOverride("KEY_W", separator));
	assert(!SplitOverride(",SHIFT", separator));
	assert(!SplitOverride("KEY_W,SHIFT,ALT", separator));
	assert(NormalizeModifiersForKey(0, 0, 4) == 0);
	assert(NormalizeModifiersForKey(1, 0, 4) == 4);

	HeldCameraPanState pan;
	assert(!pan.any());
	pan.set(0, true);
	pan.set(2, true);
	assert(pan.vertical() == -1 && pan.horizontal() == -1); // up-left diagonal
	pan.set(1, true);
	assert(pan.vertical() == 0); // opposite directions cancel
	pan.set(0, false); // release remains independent of modifier state outside this class
	assert(pan.vertical() == 1);
	pan.clear();
	assert(!pan.any());

	ActiveCameraPanBindings activePans;
	activePans.activate(0, 17); // bare W camera pan
	assert(activePans.releasePhysicalKey(18) == 0); // unrelated key cannot release it
	assert(activePans.releasePhysicalKey(17) == 1U);
	assert(!activePans.isActive(0));
	activePans.activate(0, 17);
	activePans.activate(1, 17); // same physical key with different configured modifiers
	assert(activePans.releasePhysicalKey(17) == (1U | 2U));
	assert(!activePans.isActive(0) && !activePans.isActive(1));

	std::cout << "keybinding rules tests passed\n";
	return 0;
}
