#include "PrimitiveComponent.h"

#include "Reflection/Describe.h"

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

void PrimitiveComponent::Describe(Reflection::Builder<PrimitiveComponent>& c)
{
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

	c.Field<&P::type>("Type", "The shape to generate");
	c.Field<&P::material>("Material", "The material the shape is drawn with");

	c.Field<&P::cubeWidth>("Width", "Size of the cube along X").Group("Cube", &IsCube);
	c.Field<&P::cubeHeight>("Height", "Size of the cube along Y").Group("Cube", &IsCube);
	c.Field<&P::cubeDepth>("Depth", "Size of the cube along Z").Group("Cube", &IsCube);

	c.Field<&P::sphereRadius>("Radius", "Radius of the sphere").Group("Sphere", &IsSphere);
	c.Field<&P::sphereLongitudeLines>("LongitudeLines", "Number of lines from pole to pole").Group("Sphere", &IsSphere);
	c.Field<&P::sphereLatitudeLines>("LatitudeLines", "Number of lines around the sphere").Group("Sphere", &IsSphere);

	c.Field<&P::planeWidth>("Width", "Size of the plane along X").Group("Plane", &IsPlane);
	c.Field<&P::planeLength>("Length", "Size of the plane along Z").Group("Plane", &IsPlane);
	c.Field<&P::planeWidthLines>("WidthLines", "Number of grid lines across the width").Group("Plane", &IsPlane);
	c.Field<&P::planeLengthLines>("LengthLines", "Number of grid lines along the length").Group("Plane", &IsPlane);
	c.Field<&P::planeTileU>("TileU", "How many times the texture repeats across the width").Group("Plane", &IsPlane);
	c.Field<&P::planeTileV>("TileV", "How many times the texture repeats along the length").Group("Plane", &IsPlane);

	c.Field<&P::cylinderBottomRadius>("BottomRadius", "Radius of the cylinder's base").Group("Cylinder", &IsCylinder);
	c.Field<&P::cylinderTopRadius>("TopRadius", "Radius of the cylinder's top").Group("Cylinder", &IsCylinder);
	c.Field<&P::cylinderHeight>("Height", "Height of the cylinder").Group("Cylinder", &IsCylinder);
	c.Field<&P::cylinderSliceCount>("SliceCount", "Number of segments around the cylinder").Group("Cylinder", &IsCylinder);
	c.Field<&P::cylinderStackCount>("StackCount", "Number of segments up the cylinder").Group("Cylinder", &IsCylinder);

	c.Field<&P::coneBottomRadius>("BottomRadius", "Radius of the cone's base").Group("Cone", &IsCone);
	c.Field<&P::coneHeight>("Height", "Height of the cone").Group("Cone", &IsCone);
	c.Field<&P::coneSliceCount>("SliceCount", "Number of segments around the cone").Group("Cone", &IsCone);
	c.Field<&P::coneStackCount>("StackCount", "Number of segments up the cone").Group("Cone", &IsCone);

	c.Field<&P::torusOuterRadius>("OuterRadius", "Radius of the ring, to the middle of the tube").Group("Torus", &IsTorus);
	c.Field<&P::torusInnerRadius>("InnerRadius", "Radius of the tube").Group("Torus", &IsTorus);
	c.Field<&P::torusSliceCount>("SliceCount", "Number of segments around the ring").Group("Torus", &IsTorus);

	c.OnChanged(&MarkForUpdate);
}
