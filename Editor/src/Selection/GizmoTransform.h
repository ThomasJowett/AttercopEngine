#pragma once

#include "Scene/Entity.h"
#include "Selection/EntitySelection.h"
#include "math/Matrix.h"
#include "math/Quaternion.h"
#include "math/Vector3f.h"

#include <vector>

class Scene;

// Moving, rotating and scaling many entities with one gizmo, through a pivot matrix
namespace GizmoTransform
{
	enum class Pivot { Centre, Active };

	struct Transform
	{
		Vector3f position;
		Quaternion rotation;
		Vector3f scale = Vector3f(1.0f, 1.0f, 1.0f);
	};

	// An entity the gizmo moves
	struct Item
	{
		// The world matrix from its local transform, not one overridden for drawing
		Matrix4x4 world;
		Matrix4x4 parentWorld;
		Transform local;
	};

	// The top-level selected entities with a transform, which the gizmo moves
	std::vector<Entity> GetTargets(Scene& scene, const EntitySelection& selection);
	// Parent matrices must be up to date
	Item ItemOf(Entity entity);

	// The centre of the targets' world positions, or the active entity's; the centre if the active one has no transform
	Vector3f PivotPosition(const std::vector<Item>& targets, const Item* active, Pivot pivot);
	// Unit scale, at the position, with the axes
	Matrix4x4 PivotMatrix(const Vector3f& position, const Quaternion& orientation);

	// The item's local transform after the pivot moved from oldPivot to newPivot. Its scale is multiplied by the pivot's
	// scale change on its own axes; with scalePositions its offset from the pivot is also scaled, on the pivot's axes.
	Transform Apply(const Matrix4x4& oldPivot, const Matrix4x4& newPivot, const Item& item, bool scalePositions);
}
