#pragma once

#include "Reflection/ReflectionFwd.h"

struct AudioListenerComponent
{
	AudioListenerComponent() = default;
	AudioListenerComponent(const AudioListenerComponent&) = default;

	bool primary = true;
	static void Describe(Reflection::Builder<AudioListenerComponent>& c);
};
