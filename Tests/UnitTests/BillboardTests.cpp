#include "vendor/doctest.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components/BillboardComponent.h"
#include "Scene/Components/TransformComponent.h"

namespace BillboardTests
{
	// The direction the billboard's front (+Z) faces in the world
	Vector3f Facing(Entity entity)
	{
		const Matrix4x4& world = entity.GetComponent<TransformComponent>().GetWorldMatrix();
		return Vector3f(world(0, 2), world(1, 2), world(2, 2)).GetNormalized();
	}

	void CheckSameDirection(const Vector3f& a, const Vector3f& b)
	{
		CHECK(a.x == doctest::Approx(b.x).scale(1.0f));
		CHECK(a.y == doctest::Approx(b.y).scale(1.0f));
		CHECK(a.z == doctest::Approx(b.z).scale(1.0f));
	}

	Entity CreateBillboard(Scene& scene, const Vector3f& position, BillboardComponent::Orientation orientation)
	{
		Entity entity = scene.CreateEntity("Billboard");
		entity.AddComponent<TransformComponent>(position);
		entity.AddComponent<BillboardComponent>().orientation = orientation;
		return entity;
	}

	const Matrix4x4 k_Orthographic = Matrix4x4::OrthographicRH(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
	const Matrix4x4 k_Perspective = Matrix4x4::PerspectiveRH(1.0f, 1.0f, 0.1f, 100.0f);
}

using namespace BillboardTests;

TEST_CASE("Billboards off to the side of an orthographic camera face back along its view")
{
	Scene scene("");
	Entity facingCamera = CreateBillboard(scene, Vector3f(6.0f, 4.0f, 0.0f), BillboardComponent::Orientation::Camera);
	Entity upright = CreateBillboard(scene, Vector3f(-6.0f, 4.0f, 0.0f), BillboardComponent::Orientation::WorldUp);

	// Turned 30 degrees around Y, 10 units back
	Matrix4x4 camera = Matrix4x4::Rotate(Quaternion(Vector3f(0.0f, 1.0f, 0.0f), 0.5235988f)) * Matrix4x4::Translate(Vector3f(0.0f, 0.0f, 10.0f));
	scene.Render(camera, k_Orthographic);

	Vector3f back(camera(0, 2), camera(1, 2), camera(2, 2));
	CheckSameDirection(Facing(facingCamera), back.GetNormalized());
	CheckSameDirection(Facing(upright), back.GetNormalized());
}

TEST_CASE("Billboards turn to point at a perspective camera")
{
	Scene scene("");
	Entity billboard = CreateBillboard(scene, Vector3f(6.0f, 0.0f, 0.0f), BillboardComponent::Orientation::Camera);

	Matrix4x4 camera = Matrix4x4::Translate(Vector3f(0.0f, 0.0f, 10.0f));
	scene.Render(camera, k_Perspective);

	CheckSameDirection(Facing(billboard), Vector3f(-6.0f, 0.0f, 10.0f).GetNormalized());
}
