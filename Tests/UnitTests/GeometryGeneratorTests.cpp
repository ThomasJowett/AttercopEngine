#include "vendor/doctest.h"

#include "Utilities/GeometryGenerator.h"

#include <cmath>
#include <string>
#include <vector>

namespace GeometryTestHelpers
{
	uint32_t VertexCount(const Ref<Mesh>& mesh)
	{
		return (uint32_t)(mesh->GetVertexList().size() / (s_StaticMeshLayout.GetStride() / sizeof(float)));
	}

	void CheckIndices(const Ref<Mesh>& mesh, const std::string& name)
	{
		INFO(name);
		REQUIRE(mesh);
		uint32_t vertexCount = VertexCount(mesh);
		const std::vector<uint32_t>& indices = mesh->GetIndexList();
		CHECK(vertexCount > 0);
		CHECK(indices.size() % 3 == 0);
		uint32_t outOfRange = 0;
		for (uint32_t index : indices)
			if (index >= vertexCount)
				outOfRange++;
		CHECK(outOfRange == 0);
	}

	Vertex MakeVertex(Vector3f position, Vector2f texcoord)
	{
		Vertex vertex{};
		vertex.position = position;
		vertex.texcoord = texcoord;
		return vertex;
	}
}

using namespace GeometryTestHelpers;

TEST_CASE("Generated meshes only index their own vertices")
{
	CheckIndices(GeometryGenerator::CreateCube(1.0f, 2.0f, 3.0f), "cube");
	CheckIndices(GeometryGenerator::CreateGrid(4.0f, 6.0f, 3, 5, 1.0f, 1.0f), "grid");
	CheckIndices(GeometryGenerator::CreateCylinder(1.0f, 0.5f, 2.0f, 12, 4), "cylinder");
	CheckIndices(GeometryGenerator::CreateCylinder(1.0f, 0.0f, 2.0f, 12, 1), "cone");
	CheckIndices(GeometryGenerator::CreateTorus(2.0f, 0.5f, 16), "torus");
}

TEST_CASE("Generated spheres only index their own vertices")
{
	CheckIndices(GeometryGenerator::CreateSphere(1.0f, 16, 32), "default sphere");
	CheckIndices(GeometryGenerator::CreateSphere(0.5f, 3, 3), "smallest sphere");

	// Two triangles per quad between rings, one at each pole
	CHECK(GeometryGenerator::CreateSphere(1.0f, 16, 32)->GetIndexList().size() == 6 * 16 * (32 - 2));
}

TEST_CASE("Sphere vertices lie on the sphere")
{
	Ref<Mesh> sphere = GeometryGenerator::CreateSphere(2.5f, 8, 6);
	const std::vector<float>& data = sphere->GetVertexList();
	size_t stride = s_StaticMeshLayout.GetStride() / sizeof(float);
	for (size_t i = 0; i < data.size(); i += stride)
	{
		float radius = std::sqrt(data[i] * data[i] + data[i + 1] * data[i + 1] + data[i + 2] * data[i + 2]);
		CHECK(radius == doctest::Approx(2.5f));
	}
}

TEST_CASE("Sphere texture coordinates cover the texture once")
{
	const uint32_t longitudeLines = 8, latitudeLines = 6;
	Ref<Mesh> sphere = GeometryGenerator::CreateSphere(1.0f, longitudeLines, latitudeLines);
	const std::vector<float>& data = sphere->GetVertexList();
	size_t stride = s_StaticMeshLayout.GetStride() / sizeof(float);
	size_t ringSize = longitudeLines + 1;
	REQUIRE(data.size() == stride * ringSize * latitudeLines);

	// U and V follow position, normal and tangent
	auto u = [&](size_t vertex) { return data[vertex * stride + 9]; };
	auto v = [&](size_t vertex) { return data[vertex * stride + 10]; };

	for (size_t vertex = 0; vertex < ringSize * latitudeLines; vertex++)
	{
		CAPTURE(vertex);
		CHECK(u(vertex) >= 0.0f);
		CHECK(u(vertex) <= 1.0f);
	}

	// Each ring goes once round the texture, from U = 1 to U = 0
	for (size_t ring = 0; ring < latitudeLines; ring++)
	{
		CAPTURE(ring);
		CHECK(u(ring * ringSize) == doctest::Approx(1.0f));
		CHECK(u(ring * ringSize + longitudeLines) == doctest::Approx(0.0f).scale(1.0f));
	}

	// V runs from the top to the bottom
	CHECK(v(0) == doctest::Approx(1.0f));
	CHECK(v(ringSize * (latitudeLines - 1)) == doctest::Approx(0.0f).scale(1.0f));
}

TEST_CASE("A tangent points along increasing U")
{
	// U follows X on a skewed triangle
	std::vector<Vertex> vertices = {
		MakeVertex({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }),
		MakeVertex({ 2.0f, 0.0f, 0.0f }, { 2.0f, 0.0f }),
		MakeVertex({ 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }),
	};
	GeometryGenerator::GenerateTangents(vertices.data(), 3);
	for (const Vertex& vertex : vertices)
	{
		CHECK(vertex.tangent.x == doctest::Approx(1.0f));
		CHECK(vertex.tangent.y == doctest::Approx(0.0f));
	}
}

TEST_CASE("A tangent follows texture coordinates rotated against the positions")
{
	// U increases towards -Y
	std::vector<Vertex> vertices = {
		MakeVertex({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }),
		MakeVertex({ 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }),
		MakeVertex({ 0.0f, 1.0f, 0.0f }, { -1.0f, 0.0f }),
	};
	GeometryGenerator::GenerateTangents(vertices.data(), 3);
	CHECK(vertices[0].tangent.x == doctest::Approx(0.0f));
	CHECK(vertices[0].tangent.y == doctest::Approx(-1.0f));
}
