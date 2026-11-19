#pragma once

#include "Asset/AudioClip.h"
#include "Utilities/SerializationUtils.h"
#include "Utilities/BundleAudioStream.h"
#include "Reflection/ReflectionFwd.h"

struct ma_sound;

struct AudioSourceComponent
{
	AudioSourceComponent() = default;
	AudioSourceComponent(const AudioSourceComponent&) = default;

	Ref<AudioClip> audioClip;

	float volume = 1.0f;
	float pitch = 1.0f;
	bool loop = false;

	float minDistance = 1.0f;
	float maxDistance = 10.0f;

	float rolloff = 1.0f;

	bool stream = false;

	bool playOnStart = false;

	Ref<ma_sound> sound;
	Ref<BundleAudioStream> bundleStream;

	void Play() { play = true; }
	void Pause() { pause = true; }
	void Stop() { stop = true; }

	static void Describe(Reflection::Builder<AudioSourceComponent>& c);

private:
	friend class Scene;
	bool play = false;
	bool pause = false;
	bool stop = false;
};