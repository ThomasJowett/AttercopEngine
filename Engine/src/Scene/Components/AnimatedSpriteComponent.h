#pragma once

#include "Asset/SpriteSheet.h"
#include "Core/Colour.h"

#include "Core/Application.h"
#include "Utilities/FileUtils.h"
#include "Utilities/SerializationUtils.h"
#include "Scripting/Lua/LuaBindings.h"
#include "Reflection/ReflectionFwd.h"

struct FiredAnimationEvent
{
	enum class Type { Event, Finished };
	Type type = Type::Event;
	std::string name; // empty for Finished
};

struct AnimatedSpriteComponent
{
	Colour tint{ 1.0f, 1.0f,1.0f,1.0f };
	Ref<SpriteSheet> spriteSheet;
	std::string animation;
	uint32_t currentFrame = 0;

	AnimatedSpriteComponent() = default;
	AnimatedSpriteComponent(const AnimatedSpriteComponent& other) = default;

	// Returns the events crossed this call in frame order, plus Finished each time the last frame ends
	std::vector<FiredAnimationEvent> Animate(float deltaTime);
	static void Describe(Reflection::Builder<AnimatedSpriteComponent>& c);

	REFLECT_LUA_BEGIN(AnimatedSpriteComponent)
		REFLECT_LUA_PROPERTY_CUSTOM("Tint", "Colour multiplied over the sprite sheet", "Colour",
			([](Self& c) -> Colour& { return c.tint; }),
			([](Self& c, const Colour& v) { c.tint = v; }))
		REFLECT_LUA_PROPERTY_CUSTOM("SpriteSheet", "The sprite sheet the animation frames come from", "SpriteSheet",
			([](Self& c) { return c.spriteSheet; }),
			([](Self& c, const Ref<SpriteSheet>& v) { c.spriteSheet = v; }))
		REFLECT_LUA_PROPERTY_CUSTOM("Animation", "Name of the sprite sheet animation to play", "string",
			([](Self& c) { return c.animation; }),
			([](Self& c, const std::string& v) { c.animation = v; }))
	REFLECT_LUA_END()

private:
	float m_CurrentFrameTime = 0.0f;
	// Clip Animate last played, so a change of clip restarts it at frame 0
	std::string m_PlayingAnimation;

	void EnterFrame(const Animation& animationRef, std::vector<FiredAnimationEvent>& fired) const;
};
