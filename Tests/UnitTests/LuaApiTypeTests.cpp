#include "vendor/doctest.h"

#include "Scripting/Lua/LuaManager.h"

#include <string>

namespace
{
	const LuaApiEntry* Find(const std::string& owner, const std::string& name)
	{
		for (const LuaApiEntry& entry : LuaManager::GetIdentifiers())
			if (entry.component == owner && entry.name == name)
				return &entry;
		return nullptr;
	}

	std::string TypeOf(const std::string& owner, const std::string& name)
	{
		const LuaApiEntry* entry = Find(owner, name);
		return entry ? entry->type : "<missing>";
	}

	std::string ParametersOf(const std::string& owner, const std::string& name)
	{
		const LuaApiEntry* entry = Find(owner, name);
		return entry ? entry->FormatParameters() : "<missing>";
	}
}

TEST_CASE("Lua API entries record the type each function returns")
{
	LuaManager::Init();

	CHECK(TypeOf("", "CurrentScene") == "Scene");
	CHECK(TypeOf("", "CurrentEntity") == "Entity");

	// SetFunction with a member function pointer, a static_cast overload and a lambda
	CHECK(TypeOf("Scene", "FindEntity") == "Entity");
	CHECK(TypeOf("Scene", "CreateEntity") == "Entity");
	CHECK(TypeOf("Scene", "RayCast2D") == "HitResult2D");
	CHECK(TypeOf("Scene", "ScreenToWorldPoint") == "Vec3");
	CHECK(TypeOf("BehaviourTree", "GetBlackboard") == "Blackboard");

	// Component accessors return the component, or nothing for Remove
	CHECK(TypeOf("RigidBody2DComponent", "GetRigidBody2DComponent") == "RigidBody2DComponent");
	CHECK(TypeOf("RigidBody2DComponent", "AddRigidBody2DComponent") == "RigidBody2DComponent");
	CHECK(TypeOf("RigidBody2DComponent", "HasRigidBody2DComponent") == "boolean");
	CHECK(TypeOf("RigidBody2DComponent", "RemoveRigidBody2DComponent") == "");

	LuaManager::Shutdown();
}

TEST_CASE("Lua API entries record the argument types each function takes")
{
	LuaManager::Init();

	// The object a method is called on isn't an argument, nor is sol::this_state
	CHECK(ParametersOf("Scene", "FindEntity") == "(string)");
	CHECK(ParametersOf("Scene", "InstantiateEntity") == "(Entity, Vec3)");
	CHECK(ParametersOf("Scene", "ScreenToWorldPoint") == "(Vec2, number?)");
	CHECK(ParametersOf("RigidBody2DComponent", "GetRigidBody2DComponent") == "()");

	// Static functions keep every argument
	CHECK(ParametersOf("Vec2", "Dot") == "(a: Vec2, b: Vec2)");

	// Variadic arguments can't be listed
	CHECK(ParametersOf("PostProcess", "CreateEffect") == "(...)");

	LuaManager::Shutdown();
}

TEST_CASE("Lua API parameter names match the arguments they name")
{
	LuaManager::Init();

	for (const LuaApiEntry& entry : LuaManager::GetIdentifiers())
	{
		if (!entry.hasSignature || entry.parameterNames.empty())
			continue;
		INFO(entry.component << "." << entry.name);
		CHECK(entry.parameterNames.size() == entry.parameters.size());
	}

	CHECK(ParametersOf("Vec2", "Lerp") == "(a: Vec2, b: Vec2, alpha: number)");
	CHECK(ParametersOf("PrimitiveComponent", "SetCube") == "(width: number, depth: number, height: number)");

	LuaManager::Shutdown();
}
