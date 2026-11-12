#include "vendor/doctest.h"

#include "Utilities/AssetReferenceUtils.h"
#include "TinyXml2/tinyxml2.h"
#include "Utilities/MeshSerializer.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

namespace
{
	struct TemporaryProject
	{
		std::filesystem::path directory = std::filesystem::temp_directory_path() / "AttercopReferenceTests";

		TemporaryProject()
		{
			std::filesystem::remove_all(directory);
			std::filesystem::create_directories(directory / "Materials");
		}
		~TemporaryProject() { std::filesystem::remove_all(directory); }

		void Write(const std::filesystem::path& relativePath, const std::string& contents) const
		{
			std::ofstream file(directory / relativePath);
			file << contents;
		}

		std::string Read(const std::filesystem::path& relativePath) const
		{
			std::ifstream file(directory / relativePath, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		}
	};

	const char* k_Material = R"(<Material><Texture Filepath="Textures/Brick.png"/></Material>)";
	const char* k_Scene = R"(<Scene>
	<Entity Name="Parent">
		<Sprite><Texture Filepath="Textures/Grass.png"/></Sprite>
		<Entity Name="Child">
			<Button><NormalTexture Filepath="Textures/Brick.png"/></Button>
		</Entity>
	</Entity>
</Scene>)";
}

TEST_CASE("Every asset reference is visited, including in nested entities")
{
	TemporaryProject project;
	project.Write("Materials/Wall.material", k_Material);
	project.Write("Level.scene", k_Scene);
	project.Write("Notes.txt", R"(<Note Filepath="Textures/Ignored.png"/>)");

	std::multiset<std::string> visited;
	AssetReferenceUtils::ForEachReference(project.directory, [&](const std::filesystem::path&, tinyxml2::XMLElement* element, const char* referencedPath)
		{
			visited.insert(std::string(element->Name()) + ":" + referencedPath);
			return false;
		});

	CHECK(visited.size() == 3);
	CHECK(visited.count("Texture:Textures/Brick.png") == 1);
	CHECK(visited.count("Texture:Textures/Grass.png") == 1);
	CHECK(visited.count("NormalTexture:Textures/Brick.png") == 1);
}

TEST_CASE("Changes made while visiting references are saved")
{
	TemporaryProject project;
	project.Write("Materials/Wall.material", k_Material);
	project.Write("Level.scene", k_Scene);

	AssetReferenceUtils::ForEachReference(project.directory, [](const std::filesystem::path&, tinyxml2::XMLElement* element, const char* referencedPath)
		{
			if (std::string(referencedPath) != "Textures/Brick.png")
				return false;
			element->SetAttribute("Filepath", "Textures/Stone.png");
			return true;
		});

	CHECK(project.Read("Materials/Wall.material").find("Textures/Stone.png") != std::string::npos);
	CHECK(project.Read("Level.scene").find("Textures/Stone.png") != std::string::npos);
	CHECK(project.Read("Level.scene").find("Textures/Grass.png") != std::string::npos);
}

TEST_CASE("References inside a folder are found and renamed with the folder")
{
	TemporaryProject project;
	project.Write("Materials/Wall.material", k_Material);
	project.Write("Level.scene", k_Scene);
	project.Write("Materials/Old.material", R"(<Material><Texture Filepath="TexturesOld/Brick.png"/></Material>)");

	std::vector<std::filesystem::path> references = AssetReferenceUtils::FindReferences("Textures", project.directory);
	CHECK(references.size() == 2);

	std::vector<std::filesystem::path> updated = AssetReferenceUtils::UpdateReferences("Textures", "Art/Textures", project.directory);
	CHECK(updated.size() == 2);
	CHECK(project.Read("Materials/Wall.material").find("Art/Textures/Brick.png") != std::string::npos);
	CHECK(project.Read("Level.scene").find("Art/Textures/Grass.png") != std::string::npos);

	// A folder whose name only starts the same isn't inside it
	CHECK(project.Read("Materials/Old.material").find("\"TexturesOld/Brick.png\"") != std::string::npos);
}

TEST_CASE("Renaming a file only changes references to that file")
{
	TemporaryProject project;
	project.Write("Level.scene", k_Scene);

	AssetReferenceUtils::UpdateReferences("Textures/Brick.png", "Textures/Wall.png", project.directory);
	CHECK(project.Read("Level.scene").find("Textures/Wall.png") != std::string::npos);
	CHECK(project.Read("Level.scene").find("Textures/Grass.png") != std::string::npos);
}

TEST_CASE("Material references in a mesh header are renamed without touching the mesh data")
{
	TemporaryProject project;
	std::vector<uint8_t> binary = { 0x00, '<', 0xFF, '\n', 0x10, 0x00, 0x7F };
	const std::string header = R"(<StaticMesh Version="2" Layout="Static" VertexFloats="0" Indices="0">
	<Material Filepath="Materials/Wall.material"/>
	<Material/>
</StaticMesh>)";
	std::vector<uint8_t> data(header.begin(), header.end());
	data.push_back('\n');
	size_t binaryStart = data.size();
	data.insert(data.end(), binary.begin(), binary.end());
	REQUIRE(MeshSerializer::WriteWithHeader(project.directory / "Rock.staticmesh", header, data, binaryStart));

	CHECK(AssetReferenceUtils::FindReferences("Materials", project.directory).size() == 1);
	CHECK(AssetReferenceUtils::UpdateReferences("Materials/Wall.material", "Materials/Stone.material", project.directory).size() == 1);

	std::string written = project.Read("Rock.staticmesh");
	std::vector<uint8_t> writtenData(written.begin(), written.end());
	std::string writtenHeader;
	size_t writtenBinaryStart = 0;
	REQUIRE(MeshSerializer::SplitHeader(writtenData, writtenHeader, writtenBinaryStart));
	CHECK(writtenHeader.find("Materials/Stone.material") != std::string::npos);
	CHECK(std::vector<uint8_t>(writtenData.begin() + writtenBinaryStart, writtenData.end()) == binary);
}

TEST_CASE("The old binary-only mesh format has no header")
{
	std::vector<uint8_t> data = { 0x48, 0x53, 0x45, 0x4D, 0x01, 0x00, 0x00, 0x00 };
	std::string header;
	size_t binaryStart = 0;
	CHECK_FALSE(MeshSerializer::SplitHeader(data, header, binaryStart));
}

TEST_CASE("A mesh survives being saved and loaded")
{
	TemporaryProject project;
	std::vector<float> vertices(s_StaticMeshLayout.GetStride() / sizeof(float) * 3);
	for (size_t i = 0; i < vertices.size(); i++)
		vertices[i] = (float)i * 0.5f;
	std::vector<uint32_t> indices = { 0, 1, 2 };

	Submesh submesh{};
	submesh.indexCount = 3;
	submesh.vertexCount = 3;
	submesh.transform = Matrix4x4::Translate(Vector3f(1.0f, 2.0f, 3.0f));
	submesh.localTransform = Matrix4x4::RotateZ(0.25f);
	submesh.boundingBox = BoundingBox(Vector3f(-1.0f, -2.0f, -3.0f), Vector3f(1.0f, 2.0f, 3.0f));

	Ref<Mesh> mesh = CreateRef<Mesh>(vertices, indices, std::vector<Submesh>{ submesh }, std::vector<Ref<Material>>{ Material::GetDefaultMaterial() }, s_StaticMeshLayout);
	REQUIRE(MeshSerializer::Serialize(project.directory / "Triangle.staticmesh", mesh, MeshLayout::StaticMesh));

	Ref<Mesh> loaded = MeshSerializer::Deserialize(project.directory / "Triangle.staticmesh");
	REQUIRE(loaded);
	CHECK(loaded->GetVertexList() == vertices);
	CHECK(loaded->GetIndexList() == indices);
	REQUIRE(loaded->GetSubmeshes().size() == 1);
	const Submesh& loadedSubmesh = loaded->GetSubmeshes()[0];
	CHECK(loadedSubmesh.indexCount == 3);
	CHECK(loadedSubmesh.transform(0, 3) == doctest::Approx(1.0f));
	CHECK(loadedSubmesh.localTransform(0, 1) == doctest::Approx(submesh.localTransform(0, 1)));
	CHECK(loadedSubmesh.boundingBox.Max().z == doctest::Approx(3.0f));
	CHECK(loaded->GetMaterials().size() == 1);

	// The start of the file is readable
	CHECK(project.Read("Triangle.staticmesh").rfind("<StaticMesh Version=\"2\"", 0) == 0);
}

TEST_CASE("Script references in behaviour trees and state machines are renamed")
{
	TemporaryProject project;
	project.Write("Enemy.behaviourtree", R"(<BehaviourTree><Node Type="CustomTask" Filepath="Scripts/Chase.lua"/></BehaviourTree>)");
	project.Write("Door.statemachine", R"(<StateMachine><State Name="Open" Filepath="Scripts/Chase.lua"/></StateMachine>)");

	CHECK(AssetReferenceUtils::UpdateReferences("Scripts", "AI/Scripts", project.directory).size() == 2);
	CHECK(project.Read("Enemy.behaviourtree").find("AI/Scripts/Chase.lua") != std::string::npos);
	CHECK(project.Read("Door.statemachine").find("AI/Scripts/Chase.lua") != std::string::npos);
}
