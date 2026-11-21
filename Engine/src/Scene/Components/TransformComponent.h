#pragma once

#include "math/Vector3f.h"
#include "math/Matrix.h"
#include "math/Quaternion.h"

#include "Reflection/ReflectionFwd.h"

struct TransformComponent
{
	TransformComponent() = default;
	TransformComponent(const Vector3f & position)
		:m_Position(position) {}
	// Rotation as Euler angles in radians
	TransformComponent(const Vector3f& position, const Vector3f& eulerAngles, const Vector3f& scale)
		:m_Position(position), m_Rotation(eulerAngles), m_EulerAngles(eulerAngles), m_Scale(scale) {}

	// A copy may belong to a different entity, so its world matrix is recomputed
	TransformComponent(const TransformComponent& other)
		:m_Position(other.m_Position), m_Rotation(other.m_Rotation), m_EulerAngles(other.m_EulerAngles), m_Scale(other.m_Scale),
		m_WorldMatrix(other.m_WorldMatrix), m_ParentMatrix(other.m_ParentMatrix) {}

	TransformComponent& operator=(const TransformComponent& other)
	{
		m_Position = other.m_Position;
		m_Rotation = other.m_Rotation;
		m_EulerAngles = other.m_EulerAngles;
		m_Scale = other.m_Scale;
		m_WorldMatrix = other.m_WorldMatrix;
		m_ParentMatrix = other.m_ParentMatrix;
		m_Dirty = true;
		return *this;
	}

	const Vector3f& GetPosition() const { return m_Position; }
	const Vector3f& GetScale() const { return m_Scale; }

	void SetPosition(const Vector3f& position) { if (!(position == m_Position)) { m_Position = position; m_Dirty = true; } }
	void SetScale(const Vector3f& scale) { if (!(scale == m_Scale)) { m_Scale = scale; m_Dirty = true; } }

	// The rotation relative to the parent, unit length
	const Quaternion& GetRotation() const { return m_Rotation; }
	// Normalises; the Euler angles become the ones for it nearest the old ones
	void SetRotation(const Quaternion& rotation)
	{
		// Checked before normalising too, so setting the stored value changes nothing
		if (rotation == m_Rotation)
			return;
		Quaternion unit = rotation.GetNormalized();
		if (unit == m_Rotation)
			return;
		m_Rotation = unit;
		m_EulerAngles = unit.EulerAnglesNear(m_EulerAngles);
		m_Dirty = true;
	}

	// Radians: around the parent's X, then Y, then Z. Values outside ±pi are kept as given
	const Vector3f& GetEulerAngles() const { return m_EulerAngles; }
	void SetEulerAngles(const Vector3f& angles)
	{
		if (angles == m_EulerAngles)
			return;
		m_EulerAngles = angles;
		m_Rotation = Quaternion(angles);
		m_Dirty = true;
	}

	// 2D: the angle around Z in radians, the same as GetEulerAngles().z
	float GetAngle2D() const { return m_EulerAngles.z; }
	// Keeps X and Y
	void SetAngle2D(float angle)
	{
		if (angle == m_EulerAngles.z)
			return;
		m_EulerAngles.z = angle;
		m_Rotation = Quaternion(m_EulerAngles);
		m_Dirty = true;
	}

	// Whether the world matrix is waiting for SceneGraph to recompute it
	bool IsDirty() const { return m_Dirty; }

	// Up to date after SceneGraph::UpdateWorldTransform or the once-a-frame SceneGraph::Traverse
	const Matrix4x4& GetWorldMatrix() const
	{
		return m_WorldMatrix;
	}

	const Vector3f GetWorldPosition() const
	{
		return m_ParentMatrix * m_Position;
	}

	// The world matrix's rotation with scale removed; a mirrored matrix is treated as a negative x scale
	Quaternion GetWorldRotation() const
	{
		Quaternion rotation;
		return ExtractWorldRotation(rotation) ? rotation : m_Rotation;
	}

	// Worked out from the world rotation each call
	Vector3f GetWorldEulerAngles() const
	{
		Quaternion rotation;
		return ExtractWorldRotation(rotation) ? rotation.EulerAngles() : m_EulerAngles;
	}

	Matrix4x4 GetLocalMatrix() const
	{
		return Matrix4x4::Translate(m_Position) * Matrix4x4::Rotate(m_Rotation) * Matrix4x4::Scale(m_Scale);
	}

	const Matrix4x4& GetParentMatrix() const { return m_ParentMatrix; }

	// Replaces the world matrix until the next scene graph update, without changing the local transform
	void OverrideWorldMatrix(const Matrix4x4& worldMatrix) { m_WorldMatrix = worldMatrix; m_Dirty = true; }

	static void Describe(Reflection::Builder<TransformComponent>& c);

private:
	Vector3f m_Position = Vector3f(0.0f, 0.0f, 0.0f);
	Quaternion m_Rotation;									// the rotation every matrix is built from
	Vector3f m_EulerAngles = Vector3f(0.0f, 0.0f, 0.0f);	// the same rotation, as last set
	Vector3f m_Scale = Vector3f(1.0f, 1.0f, 1.0f);

	Matrix4x4 m_WorldMatrix;
	Matrix4x4 m_ParentMatrix;
	bool m_Dirty = true;

	// False when a scale of zero leaves no rotation in the world matrix
	bool ExtractWorldRotation(Quaternion& rotation) const
	{
		Vector3f basis[3];
		for (int column = 0; column < 3; column++)
		{
			basis[column] = Vector3f(m_WorldMatrix(0, column), m_WorldMatrix(1, column), m_WorldMatrix(2, column));
			if (basis[column].Magnitude() < 1e-6f)
				return false;
			basis[column].Normalize();
		}

		if (Vector3f::Dot(Vector3f::Cross(basis[0], basis[1]), basis[2]) < 0.0f)
			basis[0] = basis[0] * -1.0f;

		Matrix4x4 rotationMatrix;
		for (int column = 0; column < 3; column++)
		{
			rotationMatrix.m[0][column] = basis[column].x;
			rotationMatrix.m[1][column] = basis[column].y;
			rotationMatrix.m[2][column] = basis[column].z;
		}
		rotation = rotationMatrix.ExtractRotation().GetNormalized();
		return true;
	}

	friend class SceneGraph;
	void UpdateWorldMatrix()
	{
		m_ParentMatrix = Matrix4x4();
		m_WorldMatrix = GetLocalMatrix();
		m_Dirty = false;
	}

	void UpdateWorldMatrix(const Matrix4x4& parentMatrix)
	{
		m_ParentMatrix = parentMatrix;
		m_WorldMatrix = parentMatrix * GetLocalMatrix();
		m_Dirty = false;
	}
};
