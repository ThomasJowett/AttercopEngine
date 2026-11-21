#include "vendor/doctest.h"

#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneGraph.h"
#include "Scripting/Lua/LuaManager.h"

#include <string>

namespace LuaQuaternionTests
{
	// A parent turned around Y with a child turned around X, as globals parent and child
	struct LuaQuaternionFixture
	{
		Scene scene{ "" };
		Entity parent;
		Entity child;

		LuaQuaternionFixture()
		{
			LuaManager::Init();
			parent = scene.CreateEntity("Parent");
			parent.AddComponent<TransformComponent>(Vector3f(1.0f, 2.0f, 3.0f), Vector3f(0.0f, 0.7f, 0.0f), Vector3f(2.0f, 2.0f, 2.0f));
			child = scene.CreateEntity("Child");
			child.AddComponent<TransformComponent>(Vector3f(0.0f, 1.0f, 0.0f), Vector3f(0.4f, 0.0f, 0.0f), Vector3f(1.0f, 1.0f, 1.0f));
			SceneGraph::Reparent(child, parent);
			LuaManager::GetState()["parent"] = parent;
			LuaManager::GetState()["child"] = child;
		}
		~LuaQuaternionFixture() { LuaManager::Shutdown(); }

		bool Run(const std::string& code)
		{
			const std::string helpers = R"(
				local function check(condition, message) if not condition then error(message, 2) end end
				local function near(a, b) return math.abs(a - b) < 1e-5 end
				local function sameQuaternion(a, b) return near(a.w, b.w) and near(a.x, b.x) and near(a.y, b.y) and near(a.z, b.z) end
				local function sameVector(a, b) return near(a.x, b.x) and near(a.y, b.y) and near(a.z, b.z) end
			)";
			sol::protected_function_result result = LuaManager::GetState().safe_script(helpers + code, sol::script_pass_on_error);
			if (!result.valid())
			{
				sol::error error = result;
				FAIL_CHECK(error.what());
			}
			return result.valid();
		}
	};
}

using namespace LuaQuaternionTests;

TEST_CASE("Quaternion.FromAxisAngle turns around the axis by the angle")
{
	LuaQuaternionFixture lua;
	CHECK(lua.Run(R"(
		local q = Quaternion.FromAxisAngle(Vec3.new(0, 0, 2), math.pi / 2)
		check(near(q.w, math.cos(math.pi / 4)) and near(q.x, 0) and near(q.y, 0) and near(q.z, math.sin(math.pi / 4)), "FromAxisAngle")
		check(sameVector(q * Vec3.new(1, 0, 0), Vec3.new(0, 1, 0)), "q * v")
	)"));
}

TEST_CASE("Quaternion.FromEuler gives the same quaternion as setting a transform's Rotation")
{
	LuaQuaternionFixture lua;
	CHECK(lua.Run(R"(
		local transform = parent:GetTransformComponent()
		local angles = Vec3.new(0.3, -0.7, 1.2)
		transform.Rotation = angles
		local q = Quaternion.FromEuler(angles)
		local stored = transform.Orientation
		check(q.w == stored.w and q.x == stored.x and q.y == stored.y and q.z == stored.z, "FromEuler matches Rotation")
		local fromNew = Quaternion.new(0.3, -0.7, 1.2)
		check(q.w == fromNew.w and q.x == fromNew.x and q.y == fromNew.y and q.z == fromNew.z, "FromEuler matches Quaternion.new")
	)"));
}

TEST_CASE("Quaternion.Slerp gives the ends and the rotation halfway")
{
	LuaQuaternionFixture lua;
	CHECK(lua.Run(R"(
		local axis = Vec3.new(0, 1, 0)
		local a = Quaternion.FromAxisAngle(axis, 0.2)
		local b = Quaternion.FromAxisAngle(axis, 1.4)
		check(sameQuaternion(Quaternion.Slerp(a, b, 0), a), "Slerp at 0")
		check(sameQuaternion(Quaternion.Slerp(a, b, 1), b), "Slerp at 1")
		check(sameQuaternion(Quaternion.Slerp(a, b, 0.5), Quaternion.FromAxisAngle(axis, 0.8)), "Slerp at 0.5")
	)"));
}

TEST_CASE("Multiplying quaternions in Lua applies the right-hand one first")
{
	LuaQuaternionFixture lua;
	CHECK(lua.Run(R"(
		local aboutZ = Quaternion.FromAxisAngle(Vec3.new(0, 0, 1), math.pi / 2)
		local aboutX = Quaternion.FromAxisAngle(Vec3.new(1, 0, 0), math.pi / 2)
		local v = Vec3.new(0, 1, 0)
		check(sameVector((aboutZ * aboutX) * v, aboutZ * (aboutX * v)), "q * r then v")
		check(sameVector((aboutZ * aboutX) * v, Vec3.new(0, 0, 1)), "aboutX first")
	)"));
}

TEST_CASE("Setting Orientation from Lua sets the rotation and its Euler angles")
{
	LuaQuaternionFixture lua;
	CHECK(lua.Run(R"(
		local transform = parent:GetTransformComponent()
		transform.Orientation = Quaternion.FromAxisAngle(Vec3.new(0, 0, 1), 0.4)
		local rotation = transform.Rotation
		check(near(rotation.x, 0) and near(rotation.y, 0) and near(rotation.z, 0.4), "Rotation follows Orientation")
	)"));
	CHECK(lua.parent.GetComponent<TransformComponent>().GetAngle2D() == doctest::Approx(0.4f));
}

TEST_CASE("GetWorldRotation in Lua combines the parent's rotation with the child's")
{
	LuaQuaternionFixture lua;
	CHECK(lua.Run(R"(
		local expected = parent:GetTransformComponent().Orientation * child:GetTransformComponent().Orientation
		local world = child:GetTransformComponent():GetWorldRotation()
		local dot = world.w * expected.w + world.x * expected.x + world.y * expected.y + world.z * expected.z
		check(near(math.abs(dot), 1), "world rotation")
		check(sameVector(world * Vec3.new(0, 0, 1), expected * Vec3.new(0, 0, 1)), "world rotation turns vectors the same way")
	)"));
}
