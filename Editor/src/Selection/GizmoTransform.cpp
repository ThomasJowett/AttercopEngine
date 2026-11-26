#include "GizmoTransform.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"

namespace GizmoTransform
{
	namespace
	{
		Vector3f Multiply(const Vector3f& a, const Vector3f& b)
		{
			return Vector3f(a.x * b.x, a.y * b.y, a.z * b.z);
		}

		Vector3f Column(const Matrix4x4& matrix, int column)
		{
			return Vector3f(matrix.m[0][column], matrix.m[1][column], matrix.m[2][column]);
		}

		float Ratio(float after, float before)
		{
			return before != 0.0f ? after / before : 1.0f;
		}
	}

	std::vector<Entity> GetTargets(Scene& scene, const EntitySelection& selection)
	{
		std::vector<Entity> targets;
		for (Entity entity : selection.GetTopLevel(scene))
		{
			if (entity.HasComponent<TransformComponent>())
				targets.push_back(entity);
		}
		return targets;
	}

	Item ItemOf(Entity entity)
	{
		const TransformComponent& transformComp = entity.GetComponent<TransformComponent>();
		Item item;
		item.parentWorld = transformComp.GetParentMatrix();
		item.world = item.parentWorld * transformComp.GetLocalMatrix();
		item.local = { transformComp.GetPosition(), transformComp.GetRotation(), transformComp.GetScale() };
		return item;
	}

	Vector3f PivotPosition(const std::vector<Item>& targets, const Item* active, Pivot pivot)
	{
		if (pivot == Pivot::Active && active)
			return active->world.ExtractTranslation();

		Vector3f sum;
		for (const Item& target : targets)
			sum += target.world.ExtractTranslation();
		return targets.empty() ? sum : sum / (float)targets.size();
	}

	Matrix4x4 PivotMatrix(const Vector3f& position, const Quaternion& orientation)
	{
		return Matrix4x4::Translate(position) * Matrix4x4::Rotate(orientation.GetNormalized());
	}

	Transform Apply(const Matrix4x4& oldPivot, const Matrix4x4& newPivot, const Item& item, bool scalePositions)
	{
		Vector3f oldPosition, oldScale, newPosition, newScale;
		Quaternion oldRotation, newRotation;
		oldPivot.Decompose(oldPosition, oldRotation, oldScale);
		newPivot.Decompose(newPosition, newRotation, newScale);

		// Axes that only grew, shrank or flipped are a scale, with its sign; any axis that turned is a rotation
		Vector3f factor(1.0f, 1.0f, 1.0f);
		bool rotated = false;
		for (int axis = 0; axis < 3; ++axis)
		{
			const Vector3f before = Column(oldPivot, axis);
			const Vector3f after = Column(newPivot, axis);
			const float length = Vector3f::Dot(before, before);
			if (length <= 0.0f)
				continue;
			factor[axis] = Vector3f::Dot(after, before) / length;
			// Less than this, in radians, is float noise and would make the Euler angles drift
			if (Vector3f::Cross(after.GetNormalized(), before.GetNormalized()).Magnitude() >= 1e-5f)
				rotated = true;
		}
		if (rotated)
			factor = Vector3f(Ratio(newScale.x, oldScale.x), Ratio(newScale.y, oldScale.y), Ratio(newScale.z, oldScale.z));
		const Quaternion rotationChange = newRotation * oldRotation.Conjugate();

		// Scaled positions follow the pivot's whole change, flips included; otherwise the offset only turns with it
		const Vector3f oldWorldPosition = item.world.ExtractTranslation();
		const Vector3f worldPosition = scalePositions
			? (newPivot * Matrix4x4::Inverse(oldPivot)) * oldWorldPosition
			: newPosition + (rotated ? rotationChange : Quaternion()) * (oldWorldPosition - oldPosition);

		// Rotated with the pivot, and scaled on the entity's own axes
		Matrix4x4 linear = item.world;
		linear.m[0][3] = linear.m[1][3] = linear.m[2][3] = 0.0f;
		Matrix4x4 rotation = rotated ? Matrix4x4::Rotate(rotationChange.GetNormalized()) : Matrix4x4();
		const Matrix4x4 world = Matrix4x4::Translate(worldPosition) * rotation * linear * Matrix4x4::Scale(factor);

		Transform result = item.local;
		Vector3f localPosition, localScale;
		Quaternion localRotation;
		const bool hasRotation = (Matrix4x4::Inverse(item.parentWorld) * world).Decompose(localPosition, localRotation, localScale, item.local.scale);
		result.position = localPosition;
		result.scale = Multiply(item.local.scale, factor);
		if (rotated && hasRotation)
			result.rotation = localRotation;
		return result;
	}
}
