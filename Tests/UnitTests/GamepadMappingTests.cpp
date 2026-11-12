#include "vendor/doctest.h"

#include "Input/GamepadMapping.h"

#include <set>

TEST_CASE("Standard gamepad mapping covers every engine button once")
{
	std::set<int> standardButtons;
	for (int button = 0; button <= GAMEPAD_BUTTON_LAST; button++)
	{
		int index = GamepadMapping::StandardButton(button);
		CHECK(index >= 0);
		CHECK(index <= 16);
		standardButtons.insert(index);
	}
	CHECK(standardButtons.size() == GAMEPAD_BUTTON_LAST + 1);

	// The triggers are buttons 6 and 7 in the standard layout, read as axes instead
	CHECK(standardButtons.count(6) == 0);
	CHECK(standardButtons.count(7) == 0);
}

TEST_CASE("Standard gamepad mapping puts face buttons and the d-pad in the right places")
{
	CHECK(GamepadMapping::StandardButton(GAMEPAD_BUTTON_A) == 0);
	CHECK(GamepadMapping::StandardButton(GAMEPAD_BUTTON_Y) == 3);
	CHECK(GamepadMapping::StandardButton(GAMEPAD_BUTTON_BACK) == 8);
	CHECK(GamepadMapping::StandardButton(GAMEPAD_BUTTON_START) == 9);
	CHECK(GamepadMapping::StandardButton(GAMEPAD_BUTTON_DPAD_UP) == 12);
	CHECK(GamepadMapping::StandardButton(GAMEPAD_BUTTON_DPAD_DOWN) == 13);
	CHECK(GamepadMapping::StandardButton(GAMEPAD_BUTTON_DPAD_LEFT) == 14);
	CHECK(GamepadMapping::StandardButton(GAMEPAD_BUTTON_DPAD_RIGHT) == 15);
	CHECK(GamepadMapping::StandardButton(GAMEPAD_BUTTON_GUIDE) == 16);
	CHECK(GamepadMapping::StandardButton(GAMEPAD_BUTTON_LAST + 1) == -1);
}

TEST_CASE("Standard gamepad mapping reads sticks from axes and triggers from buttons")
{
	CHECK(GamepadMapping::StandardAxis(GAMEPAD_AXIS_LEFT_X) == 0);
	CHECK(GamepadMapping::StandardAxis(GAMEPAD_AXIS_LEFT_Y) == 1);
	CHECK(GamepadMapping::StandardAxis(GAMEPAD_AXIS_RIGHT_X) == 2);
	CHECK(GamepadMapping::StandardAxis(GAMEPAD_AXIS_RIGHT_Y) == 3);
	CHECK(GamepadMapping::StandardAxis(GAMEPAD_AXIS_LEFT_TRIGGER) == -1);
	CHECK(GamepadMapping::StandardAxis(GAMEPAD_AXIS_RIGHT_TRIGGER) == -1);

	CHECK(GamepadMapping::StandardTriggerButton(GAMEPAD_AXIS_LEFT_TRIGGER) == 6);
	CHECK(GamepadMapping::StandardTriggerButton(GAMEPAD_AXIS_RIGHT_TRIGGER) == 7);
	CHECK(GamepadMapping::StandardTriggerButton(GAMEPAD_AXIS_LEFT_X) == -1);
}
