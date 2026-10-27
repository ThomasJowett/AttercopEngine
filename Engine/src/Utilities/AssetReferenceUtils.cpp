#include "AssetReferenceUtils.h"

#include "Core/Application.h"
#include "Logging/Instrumentor.h"
#include "TinyXml2/tinyxml2.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/AssetManager.h"
#include "Scene/Components/SpriteComponent.h"
#include "Scene/Components/AnimatedSpriteComponent.h"
#include "Scene/Components/UIWidgets/ButtonComponent.h"
#include "Utilities/FileUtils.h"
#include "Utilities/MeshSerializer.h"
#include "Asset/Material.h"
#include "Asset/SpriteSheet.h"
#include "Asset/Tileset.h"
#include "Asset/StaticMesh.h"

#include <fstream>
#include <iterator>
#include <optional>

namespace
{
	std::string NormalisePath(const std::filesystem::path& path)
	{
		std::filesystem::path normalised = path;
		normalised.make_preferred();
		std::string pathString = normalised.string();
		std::replace(pathString.begin(), pathString.end(), '\\', '/');
		return pathString;
	}

	// Every asset reference uses the "Filepath" attribute (SerializationUtils::Encode/Decode),
	// whichever element it's on and whatever it points at
	bool VisitFilepathAttributesRecursive(const std::filesystem::path& file, tinyxml2::XMLElement* element, const AssetReferenceUtils::ReferenceVisitor& visit)
	{
		bool changed = false;

		if (const char* attribute = element->Attribute("Filepath"))
			changed = visit(file, element, attribute);

		for (tinyxml2::XMLElement* child = element->FirstChildElement(); child; child = child->NextSiblingElement())
		{
			if (VisitFilepathAttributesRecursive(file, child, visit))
				changed = true;
		}

		return changed;
	}

	// The part of reference after path: empty for the path itself, "/rest" for something inside it, nullopt otherwise
	std::optional<std::string> MatchReference(const std::string& reference, const std::string& path)
	{
		if (reference.compare(0, path.size(), path) != 0)
			return std::nullopt;
		if (reference.size() == path.size())
			return std::string();
		if (reference[path.size()] == '/')
			return reference.substr(path.size());
		return std::nullopt;
	}

	static const char* s_ScannedExtensions[] = { ".material", ".scene", ".spritesheet", ".tileset", ".proj" };

	bool IsScannedExtension(const std::filesystem::path& extension)
	{
		for (const char* candidate : s_ScannedExtensions)
		{
			if (extension == candidate)
				return true;
		}
		return false;
	}
}

namespace AssetReferenceUtils
{
	void ForEachReference(const std::filesystem::path& projectDirectory, const ReferenceVisitor& visit)
	{
		PROFILE_FUNCTION();

		std::error_code errorCode;
		for (auto& entry : std::filesystem::recursive_directory_iterator(projectDirectory, errorCode))
		{
			if (!entry.is_regular_file())
				continue;

			// Meshes keep their references in an XML header in front of the binary data
			if (entry.path().extension() == ".staticmesh")
			{
				std::ifstream file(entry.path(), std::ios::binary);
				std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
				file.close();

				std::string header;
				size_t binaryStart = 0;
				if (!MeshSerializer::SplitHeader(data, header, binaryStart))
					continue;

				tinyxml2::XMLDocument document;
				if (document.Parse(header.c_str(), header.size()) != tinyxml2::XML_SUCCESS)
					continue;

				tinyxml2::XMLElement* root = document.RootElement();
				if (root && VisitFilepathAttributesRecursive(entry.path(), root, visit))
				{
					tinyxml2::XMLPrinter printer;
					document.Print(&printer);
					MeshSerializer::WriteWithHeader(entry.path(), printer.CStr(), data, binaryStart);
				}
				continue;
			}

			if (!IsScannedExtension(entry.path().extension()))
				continue;

			tinyxml2::XMLDocument document;
			if (document.LoadFile(entry.path().string().c_str()) != tinyxml2::XML_SUCCESS)
				continue;

			tinyxml2::XMLElement* root = document.RootElement();
			if (root && VisitFilepathAttributesRecursive(entry.path(), root, visit))
				document.SaveFile(entry.path().string().c_str());
		}
	}

	std::vector<std::filesystem::path> FindReferences(const std::filesystem::path& path)
	{
		return FindReferences(path, Application::GetOpenDocumentDirectory());
	}

	std::vector<std::filesystem::path> FindReferences(const std::filesystem::path& path, const std::filesystem::path& projectDirectory)
	{
		PROFILE_FUNCTION();

		std::string pathString = NormalisePath(path);
		std::vector<std::filesystem::path> references;

		ForEachReference(projectDirectory, [&](const std::filesystem::path& file, tinyxml2::XMLElement*, const char* referencedPath)
			{
				if (MatchReference(NormalisePath(referencedPath), pathString) && (references.empty() || references.back() != file))
					references.push_back(file);
				return false;
			});

		return references;
	}

	std::vector<std::filesystem::path> UpdateReferences(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
	{
		return UpdateReferences(oldPath, newPath, Application::GetOpenDocumentDirectory());
	}

	std::vector<std::filesystem::path> UpdateReferences(const std::filesystem::path& oldPath, const std::filesystem::path& newPath, const std::filesystem::path& projectDirectory)
	{
		PROFILE_FUNCTION();

		std::string oldPathString = NormalisePath(oldPath);
		std::string newPathString = NormalisePath(newPath);

		std::vector<std::filesystem::path> updatedFiles;

		ForEachReference(projectDirectory, [&](const std::filesystem::path& file, tinyxml2::XMLElement* element, const char* referencedPath)
			{
				std::optional<std::string> rest = MatchReference(NormalisePath(referencedPath), oldPathString);
				if (!rest)
					return false;

				element->SetAttribute("Filepath", (newPathString + *rest).c_str());
				if (updatedFiles.empty() || updatedFiles.back() != file)
				{
					updatedFiles.push_back(file);
					ENGINE_INFO("Updated asset reference to {0} in {1}", newPathString, file.string());
				}
				return true;
			});

		return updatedFiles;
	}

	void UpdateCurrentSceneTextureReferences(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
	{
		PROFILE_FUNCTION();

		Scene* currentScene = SceneManager::CurrentScene();
		if (!currentScene)
			return;

		std::string oldPathString = NormalisePath(oldPath);

		// Resolved lazily, only once a real match is found - loading newPath as a texture up front
		// meant renaming any non-texture asset tried to load it as an image regardless.
		bool replacementResolved = false;
		Ref<Texture2D> replacement;
		auto getReplacement = [&]() -> Ref<Texture2D>
		{
			if (!replacementResolved)
			{
				replacement = newPath.empty() ? nullptr : AssetManager::GetTexture(newPath);
				replacementResolved = true;
			}
			return replacement;
		};

		auto matches = [&](const Ref<Texture2D>& texture)
		{
			return texture && NormalisePath(texture->GetFilepath()) == oldPathString;
		};

		bool changed = false;

		currentScene->GetRegistry().view<SpriteComponent>().each([&](SpriteComponent& sprite)
			{
				if (matches(sprite.texture) || (!sprite.texture && NormalisePath(sprite.texturePath) == oldPathString))
				{
					sprite.texture = getReplacement();
					sprite.texturePath = sprite.texture ? sprite.texture->GetFilepath() : std::filesystem::path();
					changed = true;
				}
			});

		currentScene->GetRegistry().view<ButtonComponent>().each([&](ButtonComponent& button)
			{
				if (matches(button.icon)) { button.icon = getReplacement(); changed = true; }
				if (matches(button.normalTexture)) { button.normalTexture = getReplacement(); changed = true; }
				if (matches(button.hoveredTexture)) { button.hoveredTexture = getReplacement(); changed = true; }
				if (matches(button.clickedTexture)) { button.clickedTexture = getReplacement(); changed = true; }
				if (matches(button.disabledTexture)) { button.disabledTexture = getReplacement(); changed = true; }
			});

		if (changed)
			currentScene->MakeDirty();
	}

	void UpdateCurrentSceneAnimationReferences(const Ref<SpriteSheet>& spriteSheet, const std::string& oldName, const std::string& newName)
	{
		PROFILE_FUNCTION();

		Scene* currentScene = SceneManager::CurrentScene();
		if (!currentScene || !spriteSheet)
			return;

		bool changed = false;

		currentScene->GetRegistry().view<AnimatedSpriteComponent>().each([&](AnimatedSpriteComponent& animatedSprite)
			{
				if (animatedSprite.spriteSheet == spriteSheet && animatedSprite.animation == oldName)
				{
					animatedSprite.animation = newName;
					changed = true;
				}
			});

		if (changed)
			currentScene->MakeDirty();
	}

	void ReloadAffectedNonSceneAssets(const std::vector<std::filesystem::path>& affectedFiles)
	{
		PROFILE_FUNCTION();

		for (const std::filesystem::path& file : affectedFiles)
		{
			std::filesystem::path relativePath = FileUtils::RelativePath(file, Application::GetOpenDocumentDirectory());
			std::filesystem::path extension = file.extension();

			if (extension == ".material")
			{
				if (Ref<Material> material = AssetManager::GetAsset<Material>(relativePath))
					material->Reload();
			}
			else if (extension == ".spritesheet")
			{
				if (Ref<SpriteSheet> spriteSheet = AssetManager::GetAsset<SpriteSheet>(relativePath))
					spriteSheet->Reload();
			}
			else if (extension == ".tileset")
			{
				if (Ref<Tileset> tileset = AssetManager::GetAsset<Tileset>(relativePath))
					tileset->Reload();
			}
			else if (extension == ".staticmesh")
			{
				if (Ref<StaticMesh> mesh = AssetManager::GetAsset<StaticMesh>(relativePath))
					mesh->Reload();
			}
		}
	}
}
