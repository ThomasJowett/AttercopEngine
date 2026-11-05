#pragma once

#include "math/Vector3f.h"
#include "math/Matrix.h"

#include "cereal/cereal.hpp"
#include "Scripting/Lua/LuaBindings.h"

struct TransformComponent
{
	TransformComponent() = default;
	TransformComponent(const Vector3f & position)
		:m_Position(position) {}
	TransformComponent(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale)
		:m_Position(position), m_Rotation(rotation), m_Scale(scale) {}

	// A copy may belong to a different entity, so its world matrix is recomputed
	TransformComponent(const TransformComponent& other)
		:m_Position(other.m_Position), m_Rotation(other.m_Rotation), m_Scale(other.m_Scale),
		m_WorldMatrix(other.m_WorldMatrix), m_ParentMatrix(other.m_ParentMatrix) {}

	TransformComponent& operator=(const TransformComponent& other)
	{
		m_Position = other.m_Position;
		m_Rotation = other.m_Rotation;
		m_Scale = other.m_Scale;
		m_WorldMatrix = other.m_WorldMatrix;
		m_ParentMatrix = other.m_ParentMatrix;
		m_Dirty = true;
		return *this;
	}

	const Vector3f& GetPosition() const { return m_Position; }
	const Vector3f& GetRotation() const { return m_Rotation; }
	const Vector3f& GetScale() const { return m_Scale; }

	void SetPosition(const Vector3f& position) { if (!(position == m_Position)) { m_Position = position; m_Dirty = true; } }
	void SetRotation(const Vector3f& rotation) { if (!(rotation == m_Rotation)) { m_Rotation = rotation; m_Dirty = true; } }
	void SetScale(const Vector3f& scale) { if (!(scale == m_Scale)) { m_Scale = scale; m_Dirty = true; } }

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

	// Euler angles of the world matrix with scale removed; a mirrored matrix is treated as a negative x scale
	const Vector3f GetWorldRotation() const
	{
		Vector3f basis[3];
		for (int column = 0; column < 3; column++)
		{
			basis[column] = Vector3f(m_WorldMatrix(0, column), m_WorldMatrix(1, column), m_WorldMatrix(2, column));
			if (basis[column].Magnitude() < 1e-6f)
				return m_Rotation;
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
		return Matrix4x4::Translate(m_Position) * Matrix4x4::Rotate(Quaternion(m_Rotation)) * Matrix4x4::Scale(m_Scale);
	}

	const Matrix4x4& GetParentMatrix() const { return m_ParentMatrix; }

	// Replaces the world matrix until the next scene graph update, without changing the local transform
	void OverrideWorldMatrix(const Matrix4x4& worldMatrix) { m_WorldMatrix = worldMatrix; m_Dirty = true; }

	REFLECT_LUA_BEGIN(TransformComponent)
		REFLECT_LUA_PROPERTY_CUSTOM("Position", "Position relative to the parent entity, or to the world if it has no parent", "Vec3",
			([](Self& c) { return c.GetPosition(); }),
			([](Self& c, const Vector3f& v) { c.SetPosition(v); }))
		REFLECT_LUA_PROPERTY_CUSTOM("Rotation", "Rotation as Euler angles in radians, relative to the parent", "Vec3",
			([](Self& c) { return c.GetRotation(); }),
			([](Self& c, const Vector3f& v) { c.SetRotation(v); }))
		REFLECT_LUA_PROPERTY_CUSTOM("Scale", "Scale relative to the parent", "Vec3",
			([](Self& c) { return c.GetScale(); }),
			([](Self& c, const Vector3f& v) { c.SetScale(v); }))
		type.set_function("GetWorldPosition", [](const Lua::ComponentHandle<Self>& handle)
			{
				Lua::UpdateWorldTransform(handle.scene, handle.entity);
				return handle.Get().GetWorldPosition();
			});
		RegisterLuaApiEntry({ "GetWorldPosition", "Position in world space, after applying every parent's transform", LuaApiEntry::Kind::Function, s_ReflectComponentName, "", true });
	REFLECT_LUA_END()

private:
	Vector3f m_Position = Vector3f(0.0f, 0.0f, 0.0f);
	Vector3f m_Rotation = Vector3f(0.0f, 0.0f, 0.0f);
	Vector3f m_Scale = Vector3f(1.0f, 1.0f, 1.0f);

	Matrix4x4 m_WorldMatrix;
	Matrix4x4 m_ParentMatrix;
	bool m_Dirty = true;

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

	friend cereal::access;
	template<typename Archive>
	void serialize(Archive& archive)
	{
		archive(m_Position, m_Rotation, m_Scale);
	}
};
