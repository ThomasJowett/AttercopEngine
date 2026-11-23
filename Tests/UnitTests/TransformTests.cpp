#include "vendor/doctest.h"
#include "TestHelpers.h"

#include "History/HistoryCommands.h"
#include "History/PropertyCommands.h"
#include "History/SceneHistory.h"
#include "Reflection/Reflection.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneGraph.h"
#include "Scripting/Lua/LuaManager.h"

#include <cmath>
#include <vector>

namespace
{
	const float k_Tolerance = 1e-4f;

	const std::vector<Vector3f> k_EulerAngles = {
		{ 0.0f, 0.0f, 0.0f },
		{ 0.5f, 0.0f, 0.0f },
		{ 0.0f, 0.5f, 0.0f },
		{ 0.0f, 0.0f, 0.5f },
		{ 0.3f, -0.7f, 1.2f },
		{ -2.5f, 1.1f, 3.0f },
		{ 1.5f, -1.4f, -0.2f },
	};

	// Rotation only, with the scale removed from each basis column
	Matrix4x4 RotationPart(const Matrix4x4& matrix)
	{
		Matrix4x4 rotation;
		for (int column = 0; column < 3; column++)
		{
			Vector3f basis(matrix(0, column), matrix(1, column), matrix(2, column));
			basis.Normalize();
			rotation.m[0][column] = basis.x;
			rotation.m[1][column] = basis.y;
			rotation.m[2][column] = basis.z;
		}
		return rotation;
	}

	// Euler angles aren't unique, so compare the rotations they produce
	void CheckSameRotation(const Matrix4x4& a, const Matrix4x4& b)
	{
		for (int row = 0; row < 3; row++)
			for (int column = 0; column < 3; column++)
				CHECK(a(row, column) == doctest::Approx(b(row, column)).epsilon(k_Tolerance));
	}

	Matrix4x4 FromEuler(const Vector3f& euler)
	{
		return Matrix4x4::Rotate(Quaternion(euler));
	}
}

TEST_CASE("Euler angles survive a round trip through a rotation matrix")
{
	for (const Vector3f& euler : k_EulerAngles)
	{
		CAPTURE(euler.x); CAPTURE(euler.y); CAPTURE(euler.z);
		Matrix4x4 matrix = FromEuler(euler);
		CheckSameRotation(FromEuler(matrix.ExtractRotation().EulerAngles()), matrix);
	}
}

TEST_CASE("Rotation is extracted from a matrix that also has translation and non-uniform scale")
{
	for (const Vector3f& euler : k_EulerAngles)
	{
		CAPTURE(euler.x); CAPTURE(euler.y); CAPTURE(euler.z);
		Matrix4x4 matrix = Matrix4x4::Translate(Vector3f(3.0f, -2.0f, 5.0f)) * FromEuler(euler) * Matrix4x4::Scale(Vector3f(2.0f, 0.5f, 3.0f));
		CheckSameRotation(FromEuler(RotationPart(matrix).ExtractRotation().EulerAngles()), FromEuler(euler));
	}
}

TEST_CASE("World rotation combines a 3D parent rotation with the child's")
{
	entt::registry registry;
	entt::entity parent = registry.create();
	// Uniform scale, since non-uniform parent scale on a rotated child skews it and has no single rotation
	registry.emplace<TransformComponent>(parent, Vector3f(1.0f, 2.0f, 3.0f), Vector3f(0.0f, 1.2f, 0.4f), Vector3f(2.0f, 2.0f, 2.0f));
	HierarchyComponent& parentHierarchy = registry.emplace<HierarchyComponent>(parent);

	entt::entity child = registry.create();
	registry.emplace<TransformComponent>(child, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.9f, 0.0f, -0.3f), Vector3f(1.0f, 1.0f, 1.0f));
	registry.emplace<HierarchyComponent>(child).parent = parent;
	parentHierarchy.firstChild = child;

	SceneGraph::Traverse(registry);

	const TransformComponent& childTransform = registry.get<TransformComponent>(child);
	CheckSameRotation(FromEuler(childTransform.GetWorldEulerAngles()), RotationPart(childTransform.GetWorldMatrix()));
	CheckSameRotation(Matrix4x4::Rotate(childTransform.GetWorldRotation()), RotationPart(childTransform.GetWorldMatrix()));
}

TEST_CASE("World rotation of a mirrored 2D parent")
{
	entt::registry registry;
	entt::entity parent = registry.create();
	registry.emplace<TransformComponent>(parent, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 0.0f, 0.6f), Vector3f(-1.0f, 1.0f, 1.0f));
	HierarchyComponent& parentHierarchy = registry.emplace<HierarchyComponent>(parent);

	entt::entity child = registry.create();
	registry.emplace<TransformComponent>(child, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 0.0f, 0.2f), Vector3f(1.0f, 1.0f, 1.0f));
	registry.emplace<HierarchyComponent>(child).parent = parent;
	parentHierarchy.firstChild = child;

	SceneGraph::Traverse(registry);

	// Rotation then the mirrored scale must rebuild the world matrix
	const TransformComponent& childTransform = registry.get<TransformComponent>(child);
	Matrix4x4 rebuilt = FromEuler(childTransform.GetWorldEulerAngles()) * Matrix4x4::Scale(Vector3f(-1.0f, 1.0f, 1.0f));
	CheckSameRotation(rebuilt, childTransform.GetWorldMatrix());
	rebuilt = Matrix4x4::Rotate(childTransform.GetWorldRotation()) * Matrix4x4::Scale(Vector3f(-1.0f, 1.0f, 1.0f));
	CheckSameRotation(rebuilt, childTransform.GetWorldMatrix());
}

TEST_CASE("Scale is extracted from a rotated, translated matrix")
{
	for (const Vector3f& euler : k_EulerAngles)
	{
		CAPTURE(euler.x); CAPTURE(euler.y); CAPTURE(euler.z);
		Matrix4x4 matrix = Matrix4x4::Translate(Vector3f(3.0f, -2.0f, 5.0f)) * FromEuler(euler) * Matrix4x4::Scale(Vector3f(2.0f, 0.5f, 3.0f));
		Vector3f scale = matrix.ExtractScale();
		CHECK(scale.x == doctest::Approx(2.0f).epsilon(k_Tolerance));
		CHECK(scale.y == doctest::Approx(0.5f).epsilon(k_Tolerance));
		CHECK(scale.z == doctest::Approx(3.0f).epsilon(k_Tolerance));
	}
}

TEST_CASE("SetEulerAngles stores the angles exactly as given, past a whole turn included")
{
	TransformComponent transform;
	Vector3f angles((float)DegToRad(7.0), (float)DegToRad(-400.0), (float)DegToRad(370.0));
	transform.SetEulerAngles(angles);
	CHECK(transform.GetEulerAngles() == angles);
}

TEST_CASE("The 2D angle is the Z Euler angle, and setting it keeps X and Y")
{
	TransformComponent transform;
	transform.SetEulerAngles(Vector3f(0.25f, -0.5f, 0.75f));
	CHECK(transform.GetAngle2D() == 0.75f);

	// Box2D's angle isn't wrapped, so neither is the stored one
	const float threeTurns = (float)(PI * 6.0) + 0.1f;
	transform.SetAngle2D(threeTurns);
	CHECK(transform.GetAngle2D() == threeTurns);
	CHECK(transform.GetEulerAngles() == Vector3f(0.25f, -0.5f, threeTurns));

	transform.SetAngle2D(-1.0f);
	CHECK(transform.GetAngle2D() == -1.0f);
}

TEST_CASE("SetAngle2D marks the transform dirty only when the angle changes")
{
	entt::registry registry;
	entt::entity entity = registry.create();
	registry.emplace<TransformComponent>(entity, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 0.0f, 0.5f), Vector3f(1.0f, 1.0f, 1.0f));
	registry.emplace<HierarchyComponent>(entity);
	SceneGraph::Traverse(registry);

	TransformComponent& transform = registry.get<TransformComponent>(entity);
	REQUIRE_FALSE(transform.IsDirty());
	transform.SetAngle2D(0.5f);
	CHECK_FALSE(transform.IsDirty());
	transform.SetAngle2D(1.0f);
	CHECK(transform.IsDirty());

	SceneGraph::Traverse(registry);
	CheckSameRotation(transform.GetWorldMatrix(), FromEuler(Vector3f(0.0f, 0.0f, 1.0f)));
}

namespace TransformTestHelpers
{
	bool SameQuaternion(const Quaternion& a, const Quaternion& b)
	{
		return a.r == b.r && a.i == b.i && a.j == b.j && a.k == b.k;
	}

	// q and -q are the same rotation
	void CheckSameQuaternionRotation(const Quaternion& a, const Quaternion& b)
	{
		CHECK(std::abs(Quaternion::Dot(a, b)) == doctest::Approx(1.0f).epsilon(1e-6));
	}

	// Clean after a traversal, so the next change can be seen
	TransformComponent& CleanTransform(entt::registry& registry, const Vector3f& eulerAngles)
	{
		entt::entity entity = registry.create();
		registry.emplace<TransformComponent>(entity, Vector3f(0.0f, 0.0f, 0.0f), eulerAngles, Vector3f(1.0f, 1.0f, 1.0f));
		registry.emplace<HierarchyComponent>(entity);
		SceneGraph::Traverse(registry);
		return registry.get<TransformComponent>(entity);
	}

	// What the gizmo does each frame: decompose its local matrix, keeping the scale's signs, and set the rotation
	void GizmoDrag(TransformComponent& transform, const Quaternion& rotation)
	{
		Matrix4x4 local = Matrix4x4::Translate(transform.GetPosition()) * Matrix4x4::Rotate(rotation) * Matrix4x4::Scale(transform.GetScale());
		Vector3f translation, scale;
		Quaternion decomposed;
		REQUIRE(local.Decompose(translation, decomposed, scale, transform.GetScale()));
		transform.SetRotation(decomposed);
	}
}

using namespace TransformTestHelpers;

TEST_CASE("The local matrix is built from the stored quaternion")
{
	TransformComponent transform;
	transform.SetEulerAngles(AtRunTime(Vector3f(0.3f, -0.7f, 1.2f)));
	CHECK(SameQuaternion(transform.GetRotation(), Quaternion(AtRunTime(transform.GetEulerAngles()))));
	CheckSameRotation(RotationPart(transform.GetLocalMatrix()), FromEuler(Vector3f(0.3f, -0.7f, 1.2f)));
}

TEST_CASE("Setting a rotation keeps the quaternion unit length and the Euler angles the same rotation")
{
	TransformComponent transform;
	Quaternion rotation(Vector3f(1.0f, 2.0f, -0.5f), 0.8f);
	transform.SetRotation(rotation.Scale(3.0f));
	CHECK(transform.GetRotation().GetMagnitude() == doctest::Approx(1.0f));
	CheckSameQuaternionRotation(transform.GetRotation(), rotation);
	CheckSameRotation(FromEuler(transform.GetEulerAngles()), Matrix4x4::Rotate(rotation));
}

TEST_CASE("SetRotation around Z, a degree at a time, turns the Z angle steadily past 180 degrees")
{
	TransformComponent transform;
	const float degree = 0.017453292f;
	float previous = 0.0f;
	for (int step = 1; step <= 720; step++)
	{
		CAPTURE(step);
		transform.SetRotation(Quaternion(Vector3f(0.0f, 0.0f, 1.0f), step * degree));
		const Vector3f& angles = transform.GetEulerAngles();
		CHECK(angles.x == doctest::Approx(0.0f).scale(1.0f));
		CHECK(angles.y == doctest::Approx(0.0f).scale(1.0f));
		CHECK(angles.z - previous == doctest::Approx(degree).epsilon(1e-3));
		previous = angles.z;
	}
	CHECK(previous == doctest::Approx(720 * degree).epsilon(1e-4));
}

TEST_CASE("At a pitch of 90 degrees a small SetRotation keeps X as it was")
{
	TransformComponent transform;
	transform.SetEulerAngles(Vector3f(0.3f, 1.5707963f, 0.2f));
	transform.SetRotation(Quaternion(Vector3f(0.0f, 0.0f, 1.0f), 0.05f) * transform.GetRotation());
	CHECK(transform.GetEulerAngles().x == 0.3f);
	CHECK(transform.GetEulerAngles().z == doctest::Approx(0.25f).epsilon(1e-4));
	CheckSameRotation(FromEuler(transform.GetEulerAngles()), Matrix4x4::Rotate(transform.GetRotation()));
}

TEST_CASE("The 2D fast path gives the same quaternion as the general path")
{
	TransformComponent transform;
	for (float angle : { 0.4f, -2.0f, 7.5f })
	{
		CAPTURE(angle);
		const float z = AtRunTime(angle);
		transform.SetEulerAngles(Vector3f(0.0f, 0.0f, z));
		CHECK(SameQuaternion(transform.GetRotation(), Quaternion(Vector3f(0.0f, 0.0f, AtRunTime(z)))));
		transform.SetAngle2D(z + 1.0f);
		CHECK(SameQuaternion(transform.GetRotation(), Quaternion(Vector3f(0.0f, 0.0f, AtRunTime(z + 1.0f)))));
	}

	// With X or Y set, SetAngle2D keeps them in the quaternion
	transform.SetEulerAngles(AtRunTime(Vector3f(0.2f, 0.3f, 0.0f)));
	transform.SetAngle2D(AtRunTime(0.4f));
	CHECK(transform.GetEulerAngles() == Vector3f(0.2f, 0.3f, 0.4f));
	CHECK(SameQuaternion(transform.GetRotation(), Quaternion(AtRunTime(transform.GetEulerAngles()))));
}

TEST_CASE("Rotation setters mark the transform dirty only when the rotation changes")
{
	entt::registry registry;
	TransformComponent& transform = CleanTransform(registry, Vector3f(0.1f, 0.2f, 0.3f));
	REQUIRE_FALSE(transform.IsDirty());

	transform.SetEulerAngles(Vector3f(0.1f, 0.2f, 0.3f));
	CHECK_FALSE(transform.IsDirty());
	transform.SetRotation(transform.GetRotation());
	CHECK_FALSE(transform.IsDirty());

	transform.SetRotation(Quaternion(Vector3f(0.0f, 1.0f, 0.0f), 0.5f));
	CHECK(transform.IsDirty());
	SceneGraph::Traverse(registry);
	CheckSameRotation(RotationPart(transform.GetWorldMatrix()), Matrix4x4::Rotate(Quaternion(Vector3f(0.0f, 1.0f, 0.0f), 0.5f)));

	transform.SetEulerAngles(Vector3f(0.0f, 0.0f, 1.0f));
	CHECK(transform.IsDirty());
}

TEST_CASE("Copying a transform keeps both forms of its rotation")
{
	TransformComponent source;
	source.SetEulerAngles(Vector3f(0.0f, 0.0f, 7.0f));
	source.SetRotation(Quaternion(Vector3f(0.0f, 0.0f, 1.0f), 0.1f) * source.GetRotation());

	TransformComponent copy(source);
	CHECK(copy.GetEulerAngles() == source.GetEulerAngles());
	CHECK(SameQuaternion(copy.GetRotation(), source.GetRotation()));

	TransformComponent assigned;
	assigned = source;
	CHECK(assigned.GetEulerAngles() == source.GetEulerAngles());
	CHECK(SameQuaternion(assigned.GetRotation(), source.GetRotation()));
	CHECK(source.GetEulerAngles().z == doctest::Approx(7.1f));
}

TEST_CASE("Undo brings back the Euler angles exactly and a matching quaternion")
{
	RegisterComponents();
	Scene scene("");
	Entity entity = scene.CreateEntity("Turned");
	TransformComponent& transform = entity.AddComponent<TransformComponent>();

	// Set by quaternion, so the angles were worked out, and past a whole turn
	transform.SetEulerAngles(Vector3f(0.4f, -0.3f, 7.0f));
	transform.SetRotation(Quaternion(Vector3f(1.0f, 1.0f, 0.0f), 0.2f) * transform.GetRotation());
	const Vector3f angles = transform.GetEulerAngles();
	const Quaternion rotation = transform.GetRotation();

	SceneHistory history;
	Scope<SetPropertyCommand> command = CreateScope<SetPropertyCommand>(std::vector<Entity>{ entity }, *Reflection::PropertyPath::Parse("Transform.Rotation"));
	transform.SetEulerAngles(Vector3f(0.0f, 0.0f, 0.0f));
	history.Push(scene, std::move(command));
	history.Undo(scene);

	const TransformComponent& undone = entity.GetComponent<TransformComponent>();
	CHECK(undone.GetEulerAngles() == angles);
	CheckSameQuaternionRotation(undone.GetRotation(), rotation);
	CHECK(SameQuaternion(undone.GetRotation(), Quaternion(AtRunTime(angles))));
}

TEST_CASE("Stop brings back the Euler angles exactly and a matching quaternion")
{
	LuaManager::Init();
	Scene scene("");
	Entity entity = scene.CreateEntity("Turned");
	TransformComponent& transform = entity.AddComponent<TransformComponent>();
	transform.SetEulerAngles(Vector3f(0.0f, 0.0f, -7.0f));
	transform.SetRotation(Quaternion(Vector3f(0.0f, 1.0f, 0.0f), 0.3f) * transform.GetRotation());
	const Vector3f angles = transform.GetEulerAngles();
	const Quaternion rotation = transform.GetRotation();

	scene.MakeClean();
	scene.OnRuntimeStart();
	entity.GetComponent<TransformComponent>().SetAngle2D(1.0f);
	scene.OnRuntimeStop();

	REQUIRE(entity.HasComponent<TransformComponent>());
	const TransformComponent& restored = entity.GetComponent<TransformComponent>();
	CHECK(restored.GetEulerAngles() == angles);
	CheckSameQuaternionRotation(restored.GetRotation(), rotation);
}

TEST_CASE("Rotating with the gizmo around Z keeps counting past 180 degrees")
{
	TransformComponent transform;
	for (int degrees = 0; degrees <= 720; degrees++)
	{
		CAPTURE(degrees);
		const float angle = (float)DegToRad((double)degrees);
		GizmoDrag(transform, Quaternion(Vector3f(0.0f, 0.0f, 1.0f), angle));
		CHECK(transform.GetAngle2D() == doctest::Approx(angle).epsilon(1e-4));
		CHECK(transform.GetEulerAngles().x == 0.0f);
		CHECK(transform.GetEulerAngles().y == doctest::Approx(0.0f).scale(1.0f).epsilon(1e-5));
	}
}

TEST_CASE("Rotating with the gizmo near pitch 90 leaves X where it was")
{
	const float halfPi = 1.5707963f;
	for (float pitch : { halfPi, halfPi - 0.01f, -halfPi })
	{
		CAPTURE(pitch);
		TransformComponent transform(Vector3f(), Vector3f(0.3f, pitch, 0.2f), Vector3f(1.0f, 1.0f, 1.0f));

		// The parent's Z ring, in small steps
		for (int step = 1; step <= 10; step++)
		{
			GizmoDrag(transform, Quaternion(Vector3f(0.0f, 0.0f, 1.0f), 0.05f) * transform.GetRotation());
			CHECK(transform.GetEulerAngles().x == doctest::Approx(0.3f).epsilon(1e-4));
			CHECK(transform.GetEulerAngles().y == doctest::Approx(pitch).epsilon(1e-4));
		}
		CHECK(transform.GetEulerAngles().z == doctest::Approx(0.7f).epsilon(1e-3));
	}
}
