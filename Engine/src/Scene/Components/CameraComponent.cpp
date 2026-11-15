#include "CameraComponent.h"

#include "Reflection/Describe.h"

namespace CameraComponentReflection
{
	SceneCamera::ProjectionType GetProjection(const CameraComponent& c) { return c.camera.GetProjectionType(); }
	void SetProjection(CameraComponent& c, SceneCamera::ProjectionType projection) { c.camera.SetProjection(projection); }
	float GetOrthoSize(const CameraComponent& c) { return c.camera.GetOrthoSize(); }
	void SetOrthoSize(CameraComponent& c, float size) { c.camera.SetOrthoSize(size); }
	float GetOrthoNear(const CameraComponent& c) { return c.camera.GetOrthoNear(); }
	void SetOrthoNear(CameraComponent& c, float nearClip) { c.camera.SetOrthoNear(nearClip); }
	float GetOrthoFar(const CameraComponent& c) { return c.camera.GetOrthoFar(); }
	void SetOrthoFar(CameraComponent& c, float farClip) { c.camera.SetOrthoFar(farClip); }
	float GetPerspectiveNear(const CameraComponent& c) { return c.camera.GetPerspectiveNear(); }
	void SetPerspectiveNear(CameraComponent& c, float nearClip) { c.camera.SetPerspectiveNear(nearClip); }
	float GetPerspectiveFar(const CameraComponent& c) { return c.camera.GetPerspectiveFar(); }
	void SetPerspectiveFar(CameraComponent& c, float farClip) { c.camera.SetPerspectiveFar(farClip); }
	float GetFov(const CameraComponent& c) { return c.camera.GetVerticalFov(); }
	void SetFov(CameraComponent& c, float fov) { c.camera.SetVerticalFov(fov); }
	float GetAspectRatio(const CameraComponent& c) { return c.camera.GetAspectRatio(); }
	void SetAspectRatio(CameraComponent& c, float aspectRatio) { c.camera.SetAspectRatio(aspectRatio); }

	bool IsOrthographic(const CameraComponent& c) { return c.camera.GetProjectionType() == SceneCamera::ProjectionType::orthographic; }
	bool IsPerspective(const CameraComponent& c) { return !IsOrthographic(c); }
	bool HasFixedAspectRatio(const CameraComponent& c) { return c.fixedAspectRatio; }

	void Recalculate(Entity, CameraComponent& c) { c.camera.RecalculateProjection(); }
}

void CameraComponent::Describe(Reflection::Builder<CameraComponent>& c)
{
	using namespace CameraComponentReflection;

	Reflection::Enum<SceneCamera::ProjectionType>("ProjectionType", "Camera projections, for CameraComponent.Projection", {
		{ "Perspective", SceneCamera::ProjectionType::perspective },
		{ "Orthographic", SceneCamera::ProjectionType::orthographic },
	});

	c.Name("Camera").Category("Rendering")
		.Description("A camera the scene can be drawn from");

	c.Field<&CameraComponent::primary>("Primary", "Whether this is the camera the scene is rendered from at runtime");
	c.Field<&CameraComponent::fixedAspectRatio>("FixedAspectRatio", "Keep the camera's own aspect ratio instead of matching the viewport");
	c.Property<&GetProjection, &SetProjection>("Projection", "ProjectionType.Perspective or ProjectionType.Orthographic");
	c.Property<&GetOrthoSize, &SetOrthoSize>("OrthoSize", "Height of the orthographic view, in world units")
		.Label("Size").ShowIf(&IsOrthographic);
	c.Property<&GetOrthoNear, &SetOrthoNear>("OrthoNear", "Near clip plane of the orthographic view")
		.Label("Near").ShowIf(&IsOrthographic);
	c.Property<&GetOrthoFar, &SetOrthoFar>("OrthoFar", "Far clip plane of the orthographic view")
		.Label("Far").ShowIf(&IsOrthographic);
	c.Property<&GetPerspectiveNear, &SetPerspectiveNear>("PerspectiveNear", "Near clip plane of the perspective view")
		.Label("Near").ShowIf(&IsPerspective);
	c.Property<&GetPerspectiveFar, &SetPerspectiveFar>("PerspectiveFar", "Far clip plane of the perspective view")
		.Label("Far").ShowIf(&IsPerspective);
	c.Property<&GetFov, &SetFov>("Fov", "Vertical field of view of the perspective view, in radians")
		.Label("Field of View").ShowIf(&IsPerspective);
	c.Property<&GetAspectRatio, &SetAspectRatio>("AspectRatio", "Width divided by height, when FixedAspectRatio is set")
		.ShowIf(&HasFixedAspectRatio);

	c.OnChanged(&Recalculate);
}
