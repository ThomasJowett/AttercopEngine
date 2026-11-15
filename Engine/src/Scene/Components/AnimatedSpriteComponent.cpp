#include "AnimatedSpriteComponent.h"

#include "Reflection/Describe.h"

std::vector<FiredAnimationEvent> AnimatedSpriteComponent::Animate(float deltaTime)
{
	std::vector<FiredAnimationEvent> fired;
	if (!spriteSheet) return fired;
	if (animation.empty()) {
		currentFrame = 0;
		m_PlayingAnimation.clear();
		return fired;
	}
	Animation* animationRef = spriteSheet->GetAnimation(animation);
	if (!animationRef)
		return fired;
	if (animation != m_PlayingAnimation
		|| currentFrame < animationRef->GetStartFrame()
		|| currentFrame >= animationRef->GetEndFrame()) {
		m_PlayingAnimation = animation;
		currentFrame = animationRef->GetStartFrame();
		m_CurrentFrameTime = 0.0f;
		EnterFrame(*animationRef, fired);
	}
	m_CurrentFrameTime += deltaTime;
	while (m_CurrentFrameTime >= animationRef->GetHoldTime() && animationRef->GetHoldTime() > 0.0f)
	{
		if (++currentFrame >= animationRef->GetEndFrame()) {
			fired.push_back({ FiredAnimationEvent::Type::Finished, {} });
			currentFrame = animationRef->GetStartFrame();
		}
		m_CurrentFrameTime -= animationRef->GetHoldTime();
		EnterFrame(*animationRef, fired);
	}
	return fired;
}

void AnimatedSpriteComponent::EnterFrame(const Animation& animationRef, std::vector<FiredAnimationEvent>& fired) const
{
	uint32_t localFrame = currentFrame - animationRef.GetStartFrame();
	for (const AnimationEvent& event : animationRef.GetEvents())
	{
		if (event.frame == localFrame)
			fired.push_back({ FiredAnimationEvent::Type::Event, event.name });
		else if (event.frame > localFrame)
			break;
	}
}

void AnimatedSpriteComponent::Describe(Reflection::Builder<AnimatedSpriteComponent>& c)
{
	c.Name("AnimatedSprite").Category("Rendering 2D")
		.Description("Plays animations from a sprite sheet");

	c.Field<&AnimatedSpriteComponent::spriteSheet>("SpriteSheet", "The sprite sheet the animation frames come from");
	c.Field<&AnimatedSpriteComponent::tint>("Tint", "Colour multiplied over the sprite sheet");
	c.Field<&AnimatedSpriteComponent::animation>("Animation", "Name of the sprite sheet animation to play");
}
