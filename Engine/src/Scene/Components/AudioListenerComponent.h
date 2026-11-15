#include "cereal/cereal.hpp"
#include "Reflection/ReflectionFwd.h"

struct AudioListenerComponent
{
	AudioListenerComponent() = default;
	AudioListenerComponent(const AudioListenerComponent&) = default;

	bool primary = true;
	static void Describe(Reflection::Builder<AudioListenerComponent>& c);

private:
	friend cereal::access;
	template<typename Archive>
	void serialize(Archive& archive)
	{
		archive(primary);
	}
};
