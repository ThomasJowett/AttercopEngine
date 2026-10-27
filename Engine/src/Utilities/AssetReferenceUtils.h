#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "Core/core.h"

class SpriteSheet;
namespace tinyxml2 { class XMLElement; }

// Every asset that can reference another asset by path does so via the same
// Filepath="..." XML attribute convention (SerializationUtils::Encode/Decode).
// These utilities scan every .material/.scene/.spritesheet/.tileset file under
// the project for that convention, so renaming/deleting an asset can find or
// fix up whatever else references it, regardless of asset type.
namespace AssetReferenceUtils
{
	// Called for each Filepath attribute; return true if the element was changed so the file is saved
	using ReferenceVisitor = std::function<bool(const std::filesystem::path& file, tinyxml2::XMLElement* element, const char* referencedPath)>;

	// Visits every asset reference in every file under projectDirectory that can hold one.
	// This is the only place that knows which files hold references and how they're stored.
	void ForEachReference(const std::filesystem::path& projectDirectory, const ReferenceVisitor& visit);

	// Every asset file under the project's open-document-directory whose
	// Filepath attribute (anywhere in its XML tree) equals `path`, or is inside it when `path` is a folder.
	std::vector<std::filesystem::path> FindReferences(const std::filesystem::path& path);
	std::vector<std::filesystem::path> FindReferences(const std::filesystem::path& path, const std::filesystem::path& projectDirectory);

	// Rewrites every such reference from oldPath to newPath, keeping the rest of the path for
	// references inside a renamed folder. Returns the files that were updated.
	std::vector<std::filesystem::path> UpdateReferences(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
	std::vector<std::filesystem::path> UpdateReferences(const std::filesystem::path& oldPath, const std::filesystem::path& newPath, const std::filesystem::path& projectDirectory);

	// Directly repoints (or, with newPath empty, clears) any texture reference to oldPath
	// held by the currently loaded scene's live entities (SpriteComponent, ButtonComponent).
	// This works regardless of whether the scene has been saved - FindReferences/
	// UpdateReferences only see what's on disk, but a texture can be assigned to a sprite
	// in an unsaved scene, in which case the disk scan can't find or fix that reference at
	// all. Marks the scene dirty if anything changed, never touches unrelated entities/data,
	// so it's always safe to call even on a scene with other unsaved edits.
	void UpdateCurrentSceneTextureReferences(const std::filesystem::path& oldPath, const std::filesystem::path& newPath = {});

	// Repoints any live AnimatedSpriteComponent using spriteSheet + oldName to newName instead.
	void UpdateCurrentSceneAnimationReferences(const Ref<SpriteSheet>& spriteSheet, const std::string& oldName, const std::string& newName);

	// Reloads any already-cached Material/SpriteSheet/Tileset among the given files (as
	// returned by FindReferences/UpdateReferences), so a live in-memory texture reference
	// (e.g. a Material's texture slot) picks up whatever was just rewritten - or, for a
	// straight delete, now-missing - on disk. Materials/SpriteSheets/Tilesets are always
	// backed by the AssetManager cache (unlike a scene's live entity data, which can diverge
	// from disk), so re-reading from disk is always correct here. Scenes are excluded - see
	// UpdateCurrentSceneTextureReferences for those.
	void ReloadAffectedNonSceneAssets(const std::vector<std::filesystem::path>& affectedFiles);
}
