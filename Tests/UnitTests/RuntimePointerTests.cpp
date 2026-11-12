#include "vendor/doctest.h"

#include "Physics/RuntimePointer.h"
#include "Scene/Components/RigidBody2DComponent.h"
#include "Scene/Components/BoxCollider2DComponent.h"
#include "Scene/Components/WeldJoint2DComponent.h"

#include <utility>

// Never dereferenced, only compared
static b2Body* FakeBody(uintptr_t value = 0x1000) { return reinterpret_cast<b2Body*>(value); }

TEST_CASE("RuntimePointer copy construction gives null")
{
	RuntimePointer<b2Body> source;
	source = FakeBody();

	RuntimePointer<b2Body> copy(source);
	CHECK(copy == nullptr);
	CHECK(source == FakeBody());
}

TEST_CASE("RuntimePointer copy assignment keeps its own pointer")
{
	RuntimePointer<b2Body> source;
	source = FakeBody(0x1000);
	RuntimePointer<b2Body> target;
	target = FakeBody(0x2000);

	target = source;
	CHECK(target == FakeBody(0x2000));
	CHECK(source == FakeBody(0x1000));
}

TEST_CASE("RuntimePointer moves transfer the pointer")
{
	RuntimePointer<b2Body> source;
	source = FakeBody();

	RuntimePointer<b2Body> moved(std::move(source));
	CHECK(moved == FakeBody());
	CHECK(source == nullptr);

	RuntimePointer<b2Body> assigned;
	assigned = std::move(moved);
	CHECK(assigned == FakeBody());
	CHECK(moved == nullptr);
}

// EnTT moves components when others are removed, so a move must keep the body
TEST_CASE("Physics components keep their body when moved and drop it when copied")
{
	RigidBody2DComponent rigidBody;
	rigidBody.gravityScale = 2.0f;
	rigidBody.runtimeBody = FakeBody();

	RigidBody2DComponent copy(rigidBody);
	CHECK(copy.gravityScale == 2.0f);
	CHECK(copy.runtimeBody == nullptr);

	RigidBody2DComponent moved(std::move(rigidBody));
	CHECK(moved.runtimeBody == FakeBody());

	BoxCollider2DComponent box;
	box.runtimeBody = FakeBody();
	BoxCollider2DComponent movedBox;
	movedBox = std::move(box);
	CHECK(movedBox.runtimeBody == FakeBody());
}

TEST_CASE("Assigning a stored copy onto a live component keeps the live body")
{
	RigidBody2DComponent live;
	live.runtimeBody = FakeBody();

	RigidBody2DComponent stored;
	stored = live;
	stored.gravityScale = 0.5f;
	CHECK(stored.runtimeBody == nullptr);

	live = stored;
	CHECK(live.gravityScale == 0.5f);
	CHECK(live.runtimeBody == FakeBody());
}

TEST_CASE("Weld joint copies don't carry the joint")
{
	WeldJoint2DComponent weld;
	CHECK(weld.joint == nullptr);

	weld.joint = reinterpret_cast<b2WeldJoint*>(FakeBody());
	WeldJoint2DComponent copy(weld);
	CHECK(copy.joint == nullptr);
}
