#include "Input.h"
#include "Renderer/RendererAPI.h"

#include <GLFW/glfw3.h>

#include <cmath>

#include "Core/Joysticks.h"
#include "Core/Application.h"

Scope<Input> Input::s_Instance = nullptr;

std::pair<double, double> Input::GetMousePos()
{
	if (Application::HasGameViewportOverride())
		return Application::GetGameViewportMousePos();
	return s_Instance->GetMousePosImpl();
}

Input::Input(GLFWwindow* windowHandle)
	:m_Window(windowHandle)
{
}

void Input::Init(GLFWwindow* windowHandle)
{
	s_Instance = CreateScope<Input>(windowHandle);
}

void Input::SetMouseWheel(double X, double Y)
{
	s_Instance->m_MouseWheelX += X;
	s_Instance->m_MouseWheelY += Y;
}

void Input::SetMousePressed(int button)
{
	s_Instance->m_MouseButtonsPressed.at(button) = true;
}

void Input::SetMouseReleased(int button)
{
	s_Instance->m_MouseButtonsReleased.at(button) = true;
}

void Input::AccumulateMouseMotion(double xPos, double yPos)
{
	if (s_Instance->m_HasLastMousePos)
	{
		s_Instance->m_MouseDeltaAccumulator.x += (float)(xPos - s_Instance->m_LastMouseX);
		s_Instance->m_MouseDeltaAccumulator.y += (float)(yPos - s_Instance->m_LastMouseY);
	}
	s_Instance->m_LastMouseX = xPos;
	s_Instance->m_LastMouseY = yPos;
	s_Instance->m_HasLastMousePos = true;
}

void Input::ClearInputData()
{
	s_Instance->m_MouseWheelX = 0.0f;
	s_Instance->m_MouseWheelY = 0.0f;

	s_Instance->m_MouseDeltaAccumulator = Vector2f();

	s_Instance->m_MouseButtonsPressed = {};
	s_Instance->m_MouseButtonsReleased = {};
}

bool Input::IsKeyPressedImpl(int keycode)
{
	if (!m_Window)
		return false;
	try
	{
		int state = glfwGetKey(m_Window, keycode);
		return state == GLFW_PRESS;
	}
	catch (const std::bad_any_cast& e)
	{
		ENGINE_ERROR(e.what());
		return false;
	}
}

bool Input::IsMouseButtonPressedImpl(int button)
{
	if (!m_Window)
		return false;
	try
	{
		int state = glfwGetMouseButton(m_Window, button);
		return state == GLFW_PRESS;
	}
	catch (const std::bad_any_cast& e)
	{
		ENGINE_ERROR(e.what());
		return false;
	}
}

std::pair<double, double> Input::GetMousePosImpl()
{
	if (!m_Window)
		return { 0.0, 0.0 };
	try
	{
		double x, y;
		glfwGetCursorPos(m_Window, &x, &y);
		return { x, y };
	}
	catch (const std::bad_any_cast& e)
	{
		ENGINE_ERROR(e.what());
		return { 0.0f, 0.0f };
	}
}

double Input::GetMouseYImpl()
{
	auto [x, y] = GetMousePosImpl();
	return y;
}

double Input::GetMouseXImpl()
{
	auto [x, y] = GetMousePosImpl();
	return x;
}

bool Input::IsJoystickButtonPressedImpl(int joystickSlot, int button)
{
	if (!m_Window)
		return false;

#ifdef __EMSCRIPTEN__
	int button_count;
	const unsigned char* buttons = glfwGetJoystickButtons(joystickSlot, &button_count);

	if (button < button_count)
		return buttons[button];
	return false;
#else
	if (joystickSlot < 0 || joystickSlot >= Joysticks::GetJoystickCount())
		return false;

	Joysticks::Joystick joystick = Joysticks::GetJoystick(joystickSlot);
	if (joystick.isMapped)
	{
		GLFWgamepadstate state;
		return button >= 0 && button <= GLFW_GAMEPAD_BUTTON_LAST
			&& glfwGetGamepadState(joystick.id, &state) && state.buttons[button] == GLFW_PRESS;
	}

	int buttonCount = 0;
	const unsigned char* buttons = glfwGetJoystickButtons(joystick.id, &buttonCount);
	return buttons && button >= 0 && button < buttonCount && buttons[button] == GLFW_PRESS;
#endif
}

double Input::GetJoystickAxisImpl(int joystickSlot, int axis)
{
	if (!m_Window)
		return 0.0;

#ifdef __EMSCRIPTEN__
	int axes_count;
	const float* axes = glfwGetJoystickAxes(joystickSlot, &axes_count);

	if (axes && axis >= 0 && axis < axes_count)
		return axes[axis];

	return 0.0f;
#else
	if (joystickSlot < 0 || joystickSlot >= Joysticks::GetJoystickCount())
		return 0.0;

	Joysticks::Joystick joystick = Joysticks::GetJoystick(joystickSlot);
	if (!joystick.isMapped)
	{
		// Raw axes, e.g. wheels and pedals, are returned as-is with no dead zone
		int axisCount = 0;
		const float* axes = glfwGetJoystickAxes(joystick.id, &axisCount);
		return (axes && axis >= 0 && axis < axisCount) ? axes[axis] : 0.0;
	}

	GLFWgamepadstate state;
	if (axis < 0 || axis > GLFW_GAMEPAD_AXIS_LAST || !glfwGetGamepadState(joystick.id, &state))
		return 0.0;

	float value = state.axes[axis];
	// GLFW's triggers rest at -1, the engine's run from 0 to 1
	if (axis == GAMEPAD_AXIS_LEFT_TRIGGER || axis == GAMEPAD_AXIS_RIGHT_TRIGGER)
		value = (value + 1.0f) * 0.5f;

	return std::fabs(value) >= Joysticks::GetDeadZone() ? value : 0.0;
#endif
}
