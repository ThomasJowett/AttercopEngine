#include "AudioListenerComponent.h"

#include "Reflection/Describe.h"

void AudioListenerComponent::Describe(Reflection::Builder<AudioListenerComponent>& c)
{
	c.Name("AudioListener").Category("Audio")
		.Description("Where sounds are heard from");

	c.Field<&AudioListenerComponent::primary>("Primary", "Whether this is the listener sounds are heard from at runtime");
}
