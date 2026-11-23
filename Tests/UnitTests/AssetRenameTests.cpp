#include "vendor/doctest.h"

#include "Scene/AssetManager.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "Asset/Material.h"
#include "Asset/StaticMesh.h"
#include "Asset/SpriteSheet.h"
#include "Asset/Tileset.h"
#include "Asset/PhysicsMaterial.h"
#include "Asset/LuaScript.h"
#include "ComponentFixture.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace AssetRenameTests
{
	struct TemporaryProject
	{
		std::filesystem::path directory = std::filesystem::temp_directory_path() / "AttercopRenameTests";
		std::filesystem::path previousCurrentPath;

		TemporaryProject()
		{
			std::error_code ec;
			std::filesystem::remove_all(directory, ec);
			std::filesystem::create_directories(directory, ec);
			previousCurrentPath = std::filesystem::current_path();
			std::filesystem::current_path(directory, ec);
			AssetManager::CleanUp();
		}

		~TemporaryProject()
		{
			AssetManager::CleanUp();
			std::error_code ec;
			std::filesystem::current_path(previousCurrentPath, ec);
			std::filesystem::remove_all(directory, ec);
		}

		void Write(const std::filesystem::path& relativePath, const std::string& contents) const
		{
			std::filesystem::path fullPath = directory / relativePath;
			std::filesystem::create_directories(fullPath.parent_path());
			std::ofstream file(fullPath, std::ios::binary);
			file << contents;
		}

		std::string Read(const std::filesystem::path& relativePath) const
		{
			std::ifstream file(directory / relativePath, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		}
	};
}

TEST_CASE("AssetRename: One asset renamed returns same object, updates filepath, un-caches old path")
{
	AssetRenameTests::TemporaryProject project;
	project.Write("Materials/Wall.material", "<Material/>");

	Ref<Material> mat = AssetManager::GetAsset<Material>("Materials/Wall.material");
	REQUIRE(mat);
	CHECK(mat->GetFilepath() == std::filesystem::path("Materials/Wall.material").make_preferred());

	std::filesystem::rename(project.directory / "Materials/Wall.material", project.directory / "Materials/Stone.material");

	size_t moved = AssetManager::RenameAssets("Materials/Wall.material", "Materials/Stone.material");
	CHECK(moved == 1);

	Ref<Material> renamed = AssetManager::GetAsset<Material>("Materials/Stone.material");
	CHECK(renamed == mat);
	CHECK(mat->GetFilepath() == std::filesystem::path("Materials/Stone.material").make_preferred());
	CHECK_FALSE(AssetManager::Exists("Materials/Wall.material"));
}

TEST_CASE("AssetRename: Folder renamed moves assets inside, leaves similar prefix alone")
{
	AssetRenameTests::TemporaryProject project;

	Ref<Texture2D> texA = AssetManager::GetTexture("Textures/A.png");
	Ref<Texture2D> texB = AssetManager::GetTexture("Textures/Sub/B.png");
	Ref<Texture2D> texC = AssetManager::GetTexture("Textures2/C.png");
	REQUIRE(texA);
	REQUIRE(texB);
	REQUIRE(texC);

	size_t moved = AssetManager::RenameAssets("Textures", "Art/Textures");
	CHECK(moved == 2);

	CHECK(AssetManager::Exists("Art/Textures/A.png"));
	CHECK(AssetManager::Exists("Art/Textures/Sub/B.png"));
	CHECK_FALSE(AssetManager::Exists("Textures/A.png"));
	CHECK_FALSE(AssetManager::Exists("Textures/Sub/B.png"));

	CHECK(AssetManager::Exists("Textures2/C.png"));
	CHECK_FALSE(AssetManager::Exists("Art/Textures2/C.png"));

	CHECK(AssetManager::GetTexture("Art/Textures/A.png") == texA);
	CHECK(AssetManager::GetTexture("Art/Textures/Sub/B.png") == texB);
	CHECK(AssetManager::GetTexture("Textures2/C.png") == texC);

	CHECK(texA->GetFilepath() == std::filesystem::path("Art/Textures/A.png").make_preferred());
	CHECK(texB->GetFilepath() == std::filesystem::path("Art/Textures/Sub/B.png").make_preferred());
	CHECK(texC->GetFilepath() == std::filesystem::path("Textures2/C.png").make_preferred());
}

TEST_CASE("AssetRename: Missing placeholder at new path reloads and both holders share loaded asset")
{
	AssetRenameTests::TemporaryProject project;
	project.Write("Materials/Orig.material", "<Material/>");

	Ref<Material> orig = AssetManager::GetAsset<Material>("Materials/Orig.material");
	REQUIRE(orig);
	CHECK_FALSE(orig->IsMissing());

	Ref<Material> placeholder = AssetManager::GetAsset<Material>("Materials/Target.material");
	REQUIRE(placeholder);
	CHECK(placeholder->IsMissing());

	std::filesystem::rename(project.directory / "Materials/Orig.material", project.directory / "Materials/Target.material");

	size_t moved = AssetManager::RenameAssets("Materials/Orig.material", "Materials/Target.material");
	CHECK(moved == 1);

	CHECK(orig->GetFilepath() == std::filesystem::path("Materials/Target.material").make_preferred());
	CHECK_FALSE(orig->IsMissing());
	CHECK_FALSE(placeholder->IsMissing());

	Ref<Material> target = AssetManager::GetAsset<Material>("Materials/Target.material");
	CHECK(target == orig);
	CHECK_FALSE(AssetManager::Exists("Materials/Orig.material"));
}

TEST_CASE("AssetRename: Texture used by sprite and material is the same object after rename")
{
	AssetRenameTests::TemporaryProject project;

	Ref<Texture2D> tex = AssetManager::GetTexture("Textures/Shared.png");
	REQUIRE(tex);

	SpriteComponent sprite;
	sprite.texture = tex;

	Ref<Material> mat = CreateRef<Material>();
	mat->AddTexture(tex, 0);

	size_t moved = AssetManager::RenameAssets("Textures/Shared.png", "Textures/SharedRenamed.png");
	CHECK(moved == 1);

	Ref<Texture2D> renamedTex = AssetManager::GetTexture("Textures/SharedRenamed.png");
	REQUIRE(renamedTex);
	CHECK(renamedTex == tex);
	CHECK(sprite.texture == renamedTex);
	CHECK(mat->GetTexture(0) == renamedTex);
	CHECK(tex->GetFilepath() == std::filesystem::path("Textures/SharedRenamed.png").make_preferred());
}

TEST_CASE("AssetRename: Scene components with assets save new paths after rename")
{
	AssetRenameTests::TemporaryProject project;

	Scene scene("");
	Entity root = ComponentFixture::Create(scene);

	AssetManager::RenameAssets("Materials/Wall.material", "Materials/Wall2.material");
	AssetManager::RenameAssets("Materials/Moss.material", "Materials/Moss2.material");
	AssetManager::RenameAssets("Meshes/Rock.staticmesh", "Meshes/Rock2.staticmesh");
	AssetManager::RenameAssets("Sprites/Hero.spritesheet", "Sprites/Hero2.spritesheet");
	AssetManager::RenameAssets("Tilesets/Grass.tileset", "Tilesets/Grass2.tileset");
	AssetManager::RenameAssets("Scripts/Player.lua", "Scripts/Player2.lua");
	AssetManager::RenameAssets("Materials/Bouncy.physicsmaterial", "Materials/Bouncy2.physicsmaterial");

	std::string xml = SceneSerializer::SerializeEntity(root);

	CHECK(xml.find("Materials/Wall.material") == std::string::npos);
	CHECK(xml.find("Materials/Moss.material") == std::string::npos);
	CHECK(xml.find("Meshes/Rock.staticmesh") == std::string::npos);
	CHECK(xml.find("Sprites/Hero.spritesheet") == std::string::npos);
	CHECK(xml.find("Tilesets/Grass.tileset") == std::string::npos);
	CHECK(xml.find("Scripts/Player.lua") == std::string::npos);
	CHECK(xml.find("Materials/Bouncy.physicsmaterial") == std::string::npos);

	CHECK(xml.find("Materials/Wall2.material") != std::string::npos);
	CHECK(xml.find("Materials/Moss2.material") != std::string::npos);
	CHECK(xml.find("Meshes/Rock2.staticmesh") != std::string::npos);
	CHECK(xml.find("Sprites/Hero2.spritesheet") != std::string::npos);
	CHECK(xml.find("Tilesets/Grass2.tileset") != std::string::npos);
	CHECK(xml.find("Scripts/Player2.lua") != std::string::npos);
	CHECK(xml.find("Materials/Bouncy2.physicsmaterial") != std::string::npos);
}

TEST_CASE("AssetRename: A new file renamed onto a missing asset's path makes it load")
{
	AssetRenameTests::TemporaryProject project;

	Ref<Material> placeholder = AssetManager::GetAsset<Material>("Materials/Lost.material");
	REQUIRE(placeholder);
	CHECK(placeholder->IsMissing());

	project.Write("Materials/New Material.material", "<Material/>");
	std::filesystem::rename(project.directory / "Materials/New Material.material", project.directory / "Materials/Lost.material");

	CHECK(AssetManager::RenameAssets("Materials/New Material.material", "Materials/Lost.material") == 0);
	CHECK_FALSE(placeholder->IsMissing());
	CHECK(AssetManager::GetAsset<Material>("Materials/Lost.material") == placeholder);
}
