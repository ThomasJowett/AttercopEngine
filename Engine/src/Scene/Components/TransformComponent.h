#pragma once

#include "math/Vector3f.h"
#include "math/Matrix.h"

#include "cereal/cereal.hpp"
#include "Scripting/Lua/LuaBindings.h"

struct TransformComponent
{
	Vector3f position = Vector3f(0.0f, 0.0f, 0.0f);
	Vector3f rotation = Vector3f(0.0f, 0.0f, 0.0f);
	Vector3f scale = Vector3f(1.0f, 1.0f, 1.0f);

	TransformComponent() = default;
	TransformComponent(const TransformComponent&) = default;
	TransformComponent(const Vector3f & position)
		:position(position) {}
	TransformComponent(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale)
		:position(position), rotation(rotation), scale(scale) {}

	const Matrix4x4& GetWorldMatrix() const
	{
		return m_WorldMatrix;
	}

	const Vector3f GetWorldPosition() const
	{
		return m_ParentMatrix * position;
	}

	// Euler angles of the world matrix with scale removed; a mirrored matrix is treated as a negative x scale
	const Vector3f GetWorldRotation() const
	{
		Vector3f basis[3];
		for (int column = 0; column < 3; column++)
		{
			basis[column] = Vector3f(m_WorldMatrix(0, column), m_WorldMatrix(1, column), m_WorldMatrix(2, column));
			if (basis[column].Magnitude() < 1e-6f)
				return rotation;
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
		return rotationMatrix.ExtractRotation().EulerAngles();
	}

	Matrix4x4 GetLocalMatrix() const
	{
		return Matrix4x4::Translate(position) * Matrix4x4::Rotate(Quaternion(rotation)) * Matrix4x4::Scale(scale);
	}

	void SetWorldMatrix(Matrix4x4 parentMatrix)
	{ 
		m_ParentMatrix = parentMatrix;
		m_WorldMatrix = parentMatrix * GetLocalMatrix();
	}

	const Matrix4x4& GetParentMatrix() const { return m_ParentMatrix; }

	// Replaces the world matrix until the next scene graph walk, without changing the local transform
	void OverrideWorldMatrix(const Matrix4x4& worldMatrix) { m_WorldMatrix = worldMatrix; }

	REFLECT_LUA_BEGIN(TransformComponent)
		REFLECT_LUA_PROPERTY_CUSTOM("Position", "Position relative to the parent entity, or to the world if it has no parent", "Vec3",
			([](Self& c) -> Vector3f& { return c.position; }),
			([](Self& c, const Vector3f& v) { c.position = v; }))
		REFLECT_LUA_PROPERTY_CUSTOM("Rotation", "Rotation as Euler angles in radians, relative to the parent", "Vec3",
			([](Self& c) -> Vector3f& { return c.rotation; }),
			([](Self& c, const Vector3f& v) { c.rotation = v; }))
		REFLECT_LUA_PROPERTY_CUSTOM("Scale", "Scale relative to the parent", "Vec3",
			([](Self& c) -> Vector3f& { return c.scale; }),
			([](Self& c, const Vector3f& v) { c.scale = v; }))
		REFLECT_LUA_FUNCTION(GetWorldPosition, "Position in world space, after applying every parent's transform")
	REFLECT_LUA_END()

private:
	Matrix4x4 m_WorldMatrix;
	Matrix4x4 m_ParentMatrix;

	friend cereal::access;
	template<typename Archive>
	void serialize(Archive& archive)
	{
		archive(position, rotation, scale);
	}
};