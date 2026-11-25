#pragma once

#include "Scene.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace tinyxml2
{
class XMLDocument;
class XMLElement;
}

class SceneSerializer
{
public:
	explicit SceneSerializer(Scene* scene);

	~SceneSerializer() = default;

	bool Serialize(const std::filesystem::path& filepath) const;
	bool Deserialize(const std::filesystem::path& filepath);
	bool Deserialize(const std::filesystem::path& filepath, const std::vector<uint8_t>& data);

	static void SerializeEntity(tinyxml2::XMLElement* pElement, Entity entity);
	static Entity DeserializeEntity(Scene* scene, tinyxml2::XMLElement* pEntityElement, bool resetUuid = false);

	static std::string SerializeEntity(Entity entity);
	static Entity DeserializeEntity(Scene* scene, const std::string& prefab, bool resetUuid = false);

	// The subtrees in an <Entities> element, in the order given
	static std::string SerializeEntities(const std::vector<Entity>& roots);
	// Reads an <Entities> element, or a single <Entity>; returns the roots, none if the text is neither
	static std::vector<Entity> DeserializeEntities(Scene* scene, const std::string& text, bool resetUuid = false);

	// The play-mode snapshot: every root entity as a save writes it, with each entity's handle
	static void WriteSnapshot(Scene* scene, tinyxml2::XMLDocument& document);
	// Loads a snapshot into an empty registry, giving each entity its handle back
	static void ReadSnapshot(Scene* scene, const tinyxml2::XMLDocument& document);
private:
	static void WriteEntity(tinyxml2::XMLElement* pElement, Entity entity, bool withHandles);

	bool LoadXML(tinyxml2::XMLDocument& doc, const std::filesystem::path& filepath);

	static Entity ReadEntityTree(Scene* scene, const tinyxml2::XMLElement* pEntityElement, bool resetUuid, const std::filesystem::path& file);
	// Of entities read with the same ID, gives it to the first in the file, or keeps it on the one already in the scene
	static void ResolveDuplicateIDs(Scene* scene, const tinyxml2::XMLElement* pFirstElement, bool withSiblings,
		const std::vector<std::pair<const tinyxml2::XMLElement*, Entity>>& duplicates, const std::filesystem::path& file);

	Scene* m_Scene;
};
