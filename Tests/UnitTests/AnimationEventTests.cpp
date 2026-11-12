#include "vendor/doctest.h"

#include "Animation/Animation.h"
#include "Asset/SpriteSheet.h"
#include "Scene/Components/AnimatedSpriteComponent.h"

#include <string>
#include <vector>

static std::vector<std::string> Describe(const std::vector<FiredAnimationEvent>& fired)
{
	std::vector<std::string> names;
	for (const FiredAnimationEvent& event : fired)
		names.push_back(event.type == FiredAnimationEvent::Type::Finished ? "<Finished>" : event.name);
	return names;
}

using Names = std::vector<std::string>;

// 4 frames starting at sheet frame 10, 0.1s each
static AnimatedSpriteComponent MakeSprite(Ref<SpriteSheet> sheet, const std::string& animation)
{
	AnimatedSpriteComponent sprite;
	sprite.spriteSheet = sheet;
	sprite.animation = animation;
	return sprite;
}

TEST_CASE("Animation events reject empty names and duplicates on one frame")
{
	Animation animation(0, 4, 0.1f);
	CHECK_FALSE(animation.AddEvent(1, ""));
	CHECK(animation.AddEvent(1, "Hit"));
	CHECK_FALSE(animation.AddEvent(1, "Hit"));
	CHECK(animation.AddEvent(2, "Hit"));
	CHECK(animation.AddEvent(1, "Sound"));
	CHECK(animation.GetEvents().size() == 3);
}

TEST_CASE("Animation events are kept in frame order, then insertion order")
{
	Animation animation(0, 4, 0.1f);
	animation.AddEvent(3, "C");
	animation.AddEvent(1, "A");
	animation.AddEvent(1, "B");
	animation.AddEvent(0, "Z");

	const std::vector<AnimationEvent>& events = animation.GetEvents();
	REQUIRE(events.size() == 4);
	CHECK(events[0].name == "Z");
	CHECK(events[1].name == "A");
	CHECK(events[2].name == "B");
	CHECK(events[3].name == "C");
}

TEST_CASE("Animation events rename and remove only the matching frame")
{
	Animation animation(0, 4, 0.1f);
	animation.AddEvent(1, "Hit");
	animation.AddEvent(1, "Sound");
	animation.AddEvent(2, "Hit");

	CHECK_FALSE(animation.RenameEvent(1, "Hit", "Sound"));
	CHECK_FALSE(animation.RenameEvent(1, "Hit", ""));
	CHECK_FALSE(animation.RenameEvent(3, "Hit", "Other"));
	CHECK(animation.RenameEvent(1, "Hit", "Swing"));
	CHECK(animation.HasEvent(1, "Swing"));
	CHECK(animation.HasEvent(2, "Hit"));

	CHECK(animation.RemoveEvent(2, "Hit"));
	CHECK_FALSE(animation.RemoveEvent(2, "Hit"));
	CHECK(animation.HasEvent(1, "Swing"));
	CHECK(animation.GetEvents().size() == 2);
}

TEST_CASE("Animate fires frame 0 events when a clip starts and every loop")
{
	Ref<SpriteSheet> sheet = CreateRef<SpriteSheet>();
	sheet->AddAnimation("Run", 10, 4, 0.1f).AddEvent(0, "Start");
	AnimatedSpriteComponent sprite = MakeSprite(sheet, "Run");

	CHECK(Describe(sprite.Animate(0.0f)) == Names{ "Start" });
	CHECK(sprite.currentFrame == 10);
	CHECK(Describe(sprite.Animate(0.05f)).empty());
	// Frames 1-3, then back to 0
	CHECK(Describe(sprite.Animate(0.1f)).empty());
	CHECK(Describe(sprite.Animate(0.1f)).empty());
	CHECK(Describe(sprite.Animate(0.1f)).empty());
	// The last frame's hold ends, then the clip loops
	CHECK(Describe(sprite.Animate(0.1f)) == Names{ "<Finished>", "Start" });
	CHECK(sprite.currentFrame == 10);
}

TEST_CASE("Animate reports events on frames skipped by a long frame, in order")
{
	Ref<SpriteSheet> sheet = CreateRef<SpriteSheet>();
	Animation& run = sheet->AddAnimation("Run", 10, 4, 0.1f);
	run.AddEvent(1, "Footstep");
	run.AddEvent(3, "Footstep");
	run.AddEvent(2, "Swing");
	AnimatedSpriteComponent sprite = MakeSprite(sheet, "Run");

	sprite.Animate(0.0f);
	// 0.35s crosses frames 1, 2 and 3
	CHECK(Describe(sprite.Animate(0.35f)) == Names{ "Footstep", "Swing", "Footstep" });
	CHECK(sprite.currentFrame == 13);
}

TEST_CASE("Animate fires several events on one frame, all of them")
{
	Ref<SpriteSheet> sheet = CreateRef<SpriteSheet>();
	Animation& attack = sheet->AddAnimation("Attack", 0, 3, 0.1f);
	attack.AddEvent(1, "Active");
	attack.AddEvent(1, "Sound");
	AnimatedSpriteComponent sprite = MakeSprite(sheet, "Attack");

	sprite.Animate(0.0f);
	CHECK(Describe(sprite.Animate(0.1f)) == Names{ "Active", "Sound" });
}

TEST_CASE("Animate reports finished once per loop of a looping clip")
{
	Ref<SpriteSheet> sheet = CreateRef<SpriteSheet>();
	sheet->AddAnimation("Idle", 0, 2, 0.1f);
	AnimatedSpriteComponent sprite = MakeSprite(sheet, "Idle");

	sprite.Animate(0.0f);
	// Frames 1, 0, 1, 0, 1
	CHECK(Describe(sprite.Animate(0.55f)) == Names{ "<Finished>", "<Finished>" });
}

TEST_CASE("Animate restarts at frame 0 when the clip changes")
{
	Ref<SpriteSheet> sheet = CreateRef<SpriteSheet>();
	sheet->AddAnimation("A", 0, 4, 0.1f).AddEvent(0, "AStart");
	// Overlapping frame range, so only the name change shows it is a new clip
	sheet->AddAnimation("B", 0, 4, 0.1f).AddEvent(0, "BStart");
	AnimatedSpriteComponent sprite = MakeSprite(sheet, "A");

	CHECK(Describe(sprite.Animate(0.0f)) == Names{ "AStart" });
	sprite.Animate(0.15f);
	CHECK(sprite.currentFrame == 1);

	sprite.animation = "B";
	CHECK(Describe(sprite.Animate(0.0f)) == Names{ "BStart" });
	CHECK(sprite.currentFrame == 0);
	// The time left over from A doesn't carry into B
	CHECK(Describe(sprite.Animate(0.09f)).empty());
	CHECK(sprite.currentFrame == 0);
}

TEST_CASE("Animate doesn't report finished until the last frame's hold ends")
{
	Ref<SpriteSheet> sheet = CreateRef<SpriteSheet>();
	sheet->AddAnimation("Attack", 0, 3, 0.1f);
	AnimatedSpriteComponent sprite = MakeSprite(sheet, "Attack");

	sprite.Animate(0.0f);
	sprite.Animate(0.25f);
	CHECK(sprite.currentFrame == 2);
	CHECK(Describe(sprite.Animate(0.04f)).empty());
	CHECK(Describe(sprite.Animate(0.02f)) == Names{ "<Finished>" });
	CHECK(sprite.currentFrame == 0);
}

TEST_CASE("Animate on a one frame clip finishes every hold")
{
	Ref<SpriteSheet> sheet = CreateRef<SpriteSheet>();
	sheet->AddAnimation("Pose", 5, 1, 0.1f).AddEvent(0, "Pose");
	AnimatedSpriteComponent sprite = MakeSprite(sheet, "Pose");

	CHECK(Describe(sprite.Animate(0.0f)) == Names{ "Pose" });
	CHECK(Describe(sprite.Animate(0.1f)) == Names{ "<Finished>", "Pose" });
}

TEST_CASE("Animate ignores events past the last frame")
{
	Ref<SpriteSheet> sheet = CreateRef<SpriteSheet>();
	sheet->AddAnimation("Short", 0, 2, 0.1f).AddEvent(5, "Never");
	AnimatedSpriteComponent sprite = MakeSprite(sheet, "Short");

	sprite.Animate(0.0f);
	CHECK(Describe(sprite.Animate(1.05f)) == Names(5, "<Finished>"));
}

TEST_CASE("Sprite sheets skip empty and same-frame duplicate event names on load")
{
	const std::string xml = R"(<SpriteSheet>
	<Animation Name="Attack" StartFrame="4" FrameCount="4" HoldTime="0.1">
		<Event Frame="2" Name="Active"/>
		<Event Frame="2" Name="Active"/>
		<Event Frame="2" Name=""/>
		<Event Frame="3"/>
		<Event Frame="3" Name="Active"/>
		<Event Frame="1" Name="Windup"/>
	</Animation>
</SpriteSheet>)";

	SpriteSheet sheet;
	sheet.Load("Memory.spritesheet", std::vector<uint8_t>(xml.begin(), xml.end()));

	Animation* attack = sheet.GetAnimation("Attack");
	REQUIRE(attack != nullptr);
	const std::vector<AnimationEvent>& events = attack->GetEvents();
	REQUIRE(events.size() == 3);
	CHECK(events[0].frame == 1);
	CHECK(events[0].name == "Windup");
	CHECK(events[1].frame == 2);
	CHECK(events[1].name == "Active");
	CHECK(events[2].frame == 3);
	CHECK(events[2].name == "Active");
}
