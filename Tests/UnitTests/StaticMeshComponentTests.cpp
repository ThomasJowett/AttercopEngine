#include "vendor/doctest.h"

#include "Scene/Components/StaticMeshComponent.h"
#include "Utilities/MeshSerializer.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
	// A mesh with one triangle per material slot, loaded the way an asset would be
	Ref<StaticMesh> CreateMesh(uint32_t slotCount)
	{
		uint32_t floatsPerVertex = s_StaticMeshLayout.GetStride() / sizeof(float);
		std::vector<float> vertices(floatsPerVertex * 3 * slotCount);
		std::vector<uint32_t> indices;
		std::vector<Submesh> submeshes;
		std::vector<Ref<Material>> materials;
		for (uint32_t slot = 0; slot < slotCount; slot++)
		{
			Submesh submesh{};
			submesh.firstIndex = slot * 3;
			submesh.vertexOffset = slot * 3;
			submesh.indexCount = 3;
			submesh.vertexCount = 3;
			submesh.materialIndex = slot;
			submeshes.push_back(submesh);
			indices.insert(indices.end(), { 0, 1, 2 });
			materials.push_back(Material::GetDefaultMaterial());
		}

		std::filesystem::path path = std::filesystem::temp_directory_path() / "AttercopSlots.staticmesh";
		Ref<Mesh> mesh = CreateRef<Mesh>(vertices, indices, submeshes, materials, s_StaticMeshLayout);
		REQUIRE(MeshSerializer::Serialize(path, mesh, MeshLayout::StaticMesh));
		std::ifstream file(path, std::ios::binary);
		std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		Ref<StaticMesh> staticMesh = CreateRef<StaticMesh>();
		REQUIRE(staticMesh->Load("Meshes/Slots.staticmesh", data));
		return staticMesh;
	}
}

TEST_CASE("A static mesh has one material per slot, using the mesh's own until overridden")
{
	StaticMeshComponent component;
	component.SetMesh(CreateMesh(3));
	CHECK(component.GetMaterialSlotCount() == 3);
	CHECK(component.materialOverrides.empty());
	CHECK(component.GetMaterial(2) == Material::GetDefaultMaterial());

	Ref<Material> red = CreateRef<Material>();
	component.SetMaterialOverride(1, red);
	CHECK(component.GetMaterial(1) == red);
	CHECK(component.GetMaterial(0) == Material::GetDefaultMaterial());

	// Choosing the mesh's own material again removes the override
	component.SetMaterialOverride(1, Material::GetDefaultMaterial());
	CHECK(component.materialOverrides.empty());
}

TEST_CASE("Changing the mesh keeps overrides for slots it still has")
{
	StaticMeshComponent component;
	component.SetMesh(CreateMesh(3));
	Ref<Material> first = CreateRef<Material>();
	Ref<Material> last = CreateRef<Material>();
	component.SetMaterialOverride(0, first);
	component.SetMaterialOverride(2, last);

	component.SetMesh(CreateMesh(5));
	CHECK(component.GetMaterialSlotCount() == 5);
	CHECK(component.GetMaterial(0) == first);
	CHECK(component.GetMaterial(2) == last);
	CHECK(component.GetMaterial(4) == Material::GetDefaultMaterial());

	component.SetMesh(CreateMesh(2));
	CHECK(component.GetMaterialSlotCount() == 2);
	CHECK(component.GetMaterial(0) == first);
	CHECK(component.materialOverrides.size() <= 2);
}

TEST_CASE("A missing mesh keeps all of its overrides")
{
	StaticMeshComponent component;
	component.SetMesh(CreateMesh(3));
	Ref<Material> last = CreateRef<Material>();
	component.SetMaterialOverride(2, last);

	Ref<StaticMesh> missing = CreateRef<StaticMesh>();
	missing->MakeMissing("Meshes/Gone.staticmesh");
	component.SetMesh(missing);
	CHECK(component.materialOverrides.size() == 3);
	CHECK(component.materialOverrides[2] == last);
}
