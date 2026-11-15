#include "AudioSourceComponent.h"

#include "Reflection/Describe.h"

void AudioSourceComponent::Describe(Reflection::Builder<AudioSourceComponent>& c)
{
	c.Name("AudioSource").Category("Audio")
		.Description("Plays an audio clip from the entity's position");

	c.Field<&AudioSourceComponent::audioClip>("Clip", "The audio clip to play").MainAsset();
	c.Field<&AudioSourceComponent::volume>("Volume", "Volume multiplier, 1 being the clip's own volume");
	c.Field<&AudioSourceComponent::pitch>("Pitch", "Playback speed and pitch multiplier");
	c.Field<&AudioSourceComponent::loop>("Loop", "Restart the clip when it finishes");
	c.Field<&AudioSourceComponent::minDistance>("MinDistance", "Distance from the listener within which the clip plays at full volume");
	c.Field<&AudioSourceComponent::maxDistance>("MaxDistance", "Distance from the listener beyond which the clip stops getting quieter");
	c.Field<&AudioSourceComponent::rolloff>("Rolloff", "How quickly the volume falls off between MinDistance and MaxDistance");
	c.Field<&AudioSourceComponent::stream>("Stream", "Stream the clip from disk instead of decoding it all up front");
	c.Field<&AudioSourceComponent::playOnStart>("PlayOnStart", "Start playing when the scene starts");
}
