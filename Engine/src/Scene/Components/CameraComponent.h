#pragma once

#include "Scene/SceneCamera.h"

#include "Reflection/ReflectionFwd.h"

struct CameraComponent
{
	SceneCamera camera;
	bool primary = true;
	bool fixedAspectRatio = false;

	CameraComponent() = default;
	CameraComponent(const CameraComponent&) = default;

	operator SceneCamera& () { return camera; }
	operator const SceneCamera& () const { return camera; }
	static void Describe(Reflection::Builder<CameraComponent>& c);
};