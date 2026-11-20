#include "PrimitiveComponent.h"

#include "Reflection/Describe.h"
#include "Scripting/Lua/LuaBindings.h"

namespace PrimitiveComponentReflection
{
	using Shape = PrimitiveComponent::Shape;

	bool IsCube(const PrimitiveComponent& c) { return c.type == Shape::Cube; }
	bool IsSphere(const PrimitiveComponent& c) { return c.type == Shape::Sphere; }
	bool IsPlane(const PrimitiveComponent& c) { return c.type == Shape::Plane; }
	bool IsCylinder(const PrimitiveComponent& c) { return c.type == Shape::Cylinder; }
	bool IsCone(const PrimitiveComponent& c) { return c.type == Shape::Cone; }
	bool IsTorus(const PrimitiveComponent& c) { return c.type == Shape::Torus; }

	// The scene regenerates the mesh before drawing it
	void MarkForUpdate(Entity, PrimitiveComponent& c) { c.needsUpdating = true; }
}

namespace PrimitiveComponentReflection
{
	REFLECT_LUA_BEGIN(PrimitiveComponent)
		REFLECT_LUA_HAND_WRITTEN_PROPERTY("Mesh", "The generated mesh", "Mesh",
			([](Self& c) { return c.mesh; }),
			([](Self& c, const Ref<Mesh>& v) { c.mesh = v; }))
		REFLECT_LUA_FUNCTION_PARAMS(SetCube, "width, depth, height", "Rebuild as a cube")
		REFLECT_LUA_FUNCTION_PARAMS(SetSphere, "radius, longitudeLines, latitudeLines", "Rebuild as a sphere")
		REFLECT_LUA_FUNCTION_PARAMS(SetPlane, "width, length, widthLines, lengthLines, tileU, tileV", "Rebuild as a plane")
		REFLECT_LUA_FUNCTION_PARAMS(SetCylinder, "bottomRadius, topRadius, height, sliceCount, stackCount", "Rebuild as a cylinder")
		REFLECT_LUA_FUNCTION_PARAMS(SetCone, "bottomRadius, height, sliceCount, stackCount", "Rebuild as a cone")
		REFLECT_LUA_FUNCTION_PARAMS(SetTorus, "outerRadius, innerRadius, sliceCount", "Rebuild as a torus")
		REFLECT_LUA_FUNCTION(SetType, "Rebuild as the given shape type, using its current settings")
	REFLECT_LUA_END()
}

void PrimitiveComponent::Describe(Reflection::Builder<PrimitiveComponent>& c)
{
	c.LuaFunctions(&PrimitiveComponentReflection::RegisterLuaFunctions);
	using namespace PrimitiveComponentReflection;
	using P = PrimitiveComponent;

	Reflection::Enum<Shape>("PrimitiveShape", "Primitive shapes, for PrimitiveComponent.Type", {
		{ "Cube", Shape::Cube },
		{ "Sphere", Shape::Sphere },
		{ "Plane", Shape::Plane },
		{ "Cylinder", Shape::Cylinder },
		{ "Cone", Shape::Cone },
		{ "Torus", Shape::Torus },
	});

	c.Name("Primitive").Category("Rendering 3D")
		.Description("Draws a generated shape: a cube, sphere, plane, cylinder, cone or torus");

	c.Field<&P::type>("Type", "The shape to generate").Label("Shape");
	c.Field<&P::material>("Material", "The material the shape is drawn with");

	c.Field<&P::cubeWidth>("Width", "Size of the cube along X").Group("Cube", &IsCube);
	c.Field<&P::cubeHeight>("Height", "Size of the cube along Y").Group("Cube", &IsCube);
	c.Field<&P::cubeDepth>("Depth", "Size of the cube along Z").Group("Cube", &IsCube);

	c.Field<&P::sphereRadius>("Radius", "Radius of the sphere").Group("Sphere", &IsSphere);
	c.Field<&P::sphereLongitudeLines>("LongitudeLines", "Number of lines from pole to pole").Range(3.0f, 600.0f).Group("Sphere", &IsSphere);
	c.Field<&P::sphereLatitudeLines>("LatitudeLines", "Number of lines around the sphere").Range(3.0f, 600.0f).Group("Sphere", &IsSphere);

	c.Field<&P::planeWidth>("Width", "Size of the plane along X").Group("Plane", &IsPlane);
	c.Field<&P::planeLength>("Length", "Size of the plane along Z").Group("Plane", &IsPlane);
	c.Field<&P::planeWidthLines>("WidthLines", "Number of grid lines across the width").Range(2.0f, 1000.0f).Group("Plane", &IsPlane);
	c.Field<&P::planeLengthLines>("LengthLines", "Number of grid lines along the length").Range(2.0f, 1000.0f).Group("Plane", &IsPlane);
	c.Field<&P::planeTileU>("TileU", "How many times the texture repeats across the width").Group("Plane", &IsPlane);
	c.Field<&P::planeTileV>("TileV", "How many times the texture repeats along the length").Group("Plane", &IsPlane);

	c.Field<&P::cylinderBottomRadius>("BottomRadius", "Radius of the cylinder's base").Group("Cylinder", &IsCylinder);
	c.Field<&P::cylinderTopRadius>("TopRadius", "Radius of the cylinder's top").Group("Cylinder", &IsCylinder);
	c.Field<&P::cylinderHeight>("Height", "Height of the cylinder").Group("Cylinder", &IsCylinder);
	c.Field<&P::cylinderSliceCount>("SliceCount", "Number of segments around the cylinder").Range(3.0f, 600.0f).Group("Cylinder", &IsCylinder);
	c.Field<&P::cylinderStackCount>("StackCount", "Number of segments up the cylinder").Range(1.0f, 600.0f).Group("Cylinder", &IsCylinder);

	c.Field<&P::coneBottomRadius>("BottomRadius", "Radius of the cone's base").Group("Cone", &IsCone);
	c.Field<&P::coneHeight>("Height", "Height of the cone").Group("Cone", &IsCone);
	c.Field<&P::coneSliceCount>("SliceCount", "Number of segments around the cone").Range(3.0f, 600.0f).Group("Cone", &IsCone);
	c.Field<&P::coneStackCount>("StackCount", "Number of segments up the cone").Range(1.0f, 600.0f).Group("Cone", &IsCone);

	c.Field<&P::torusOuterRadius>("OuterRadius", "Radius of the ring, to the middle of the tube").Range(0.0f, 100.0f).Group("Torus", &IsTorus);
	c.Field<&P::torusInnerRadius>("InnerRadius", "Radius of the tube").Range(0.0f, 100.0f).Group("Torus", &IsTorus);
	c.Field<&P::torusSliceCount>("SliceCount", "Number of segments around the ring").Range(3.0f, 600.0f).Group("Torus", &IsTorus);

	c.OnChanged(&MarkForUpdate);
}
