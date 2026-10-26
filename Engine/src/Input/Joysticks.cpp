#include "Joysticks.h"

Joysticks::Joystick Joysticks::s_Joysticks[MAX_JOYSTICKS];
int Joysticks::s_JoystickCount = 0;

double Joysticks::s_DeadZone = 0.1f;

int Joysticks::GetFirstGamepadSlot()
{
	for (int i = 0; i < s_JoystickCount; i++)
	{
		if (s_Joysticks[i].isMapped)
			return i;
	}
	return -1;
}

Joysticks::Joystick Joysticks::GetJoystick(int joystickSlot)
{
	if (joystickSlot < s_JoystickCount && joystickSlot >= 0)
	{
		return s_Joysticks[joystickSlot];
	}
	return Joystick();
}
