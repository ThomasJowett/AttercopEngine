#include "vendor/doctest.h"

#include "Scene/SceneGraph.h"

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
