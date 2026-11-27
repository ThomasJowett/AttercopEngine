#include "SceneSerializer.h"

#include "Scene/Entity.h"
#include "Scene/SceneGraph.h"
#include "Components.h"
#include "Migration/Migration.h"
#include "Reflection/Reflection.h"
#include "Reflection/SceneXml.h"
#include "Utilities/SerializationUtils.h"

#include "TinyXml2/tinyxml2.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <unordered_map>

namespace
{
	// For warnings about entities that didn't come from a file
	const std::filesystem::path k_CopiedEntity = "a copied entity";
	const std::filesystem::path k_PastedEntity = "the pasted entity";
	const std::filesystem::path k_Snapshot = "the play-mode snapshot";

	// Components are described before the first scene is read or written, also without Application::Init
	void EnsureRegistered()
	{
		static const bool s_Registered = (RegisterComponents(), true);
		(void)s_Registered;
	}

	void WriteVector(tinyxml2::XMLElement* element, const Vector2f& vector)
	{
		element->SetAttribute("X", Reflection::SceneXml::FloatToText(vector.x).c_str());
		element->SetAttribute("Y", Reflection::SceneXml::FloatToText(vector.y).c_str());
	}

	void CheckAttributes(const tinyxml2::XMLElement* element, std::initializer_list<const char*> known, const std::filesystem::path& file)
	{
		for (const tinyxml2::XMLAttribute* attribute = element->FirstAttribute(); attribute; attribute = attribute->Next())
		{
			bool isKnown = false;
			for (const char* name : known)
				isKnown = isKnown || std::strcmp(attribute->Name(), name) == 0;
			if (!isKnown)
				ENGINE_WARN("Unknown attribute {0} on <{1}>, ignored{2}", attribute->Name(), element->Name(),
					Reflection::SceneXml::Where(file, element->GetLineNum()));
		}
	}

	const char* NameOf(const tinyxml2::XMLElement* pEntityElement)
	{
		const char* name = pEntityElement->Attribute("Name");
		return name ? name : "Unnamed entity";
	}

	// An entity read with an ID already in the scene, which was given a new one
	using Duplicates = std::vector<std::pair<const tinyxml2::XMLElement*, Entity>>;

	// What one load collects for after its last entity
	struct ReadState
	{
		Duplicates duplicates;
		// With new UUIDs: each entity's ID in the text to its new one, and the entities made
		std::unordered_map<Uuid, Uuid> newIDs;
		std::vector<Entity> created;
	};

	// withHandles reads the Handle attribute the play-mode snapshot writes
	Entity ReadEntity(Scene* scene, const tinyxml2::XMLElement* pEntityElement, bool resetUuid, ReadState& state,
		const std::filesystem::path& file, bool withHandles = false)
	{
		PROFILE_FUNCTION();

		Entity entity;
		if (!pEntityElement)
			return entity;
		EnsureRegistered();

		Uuid id;
		bool duplicate = false;
		if (!resetUuid)
		{
			if (const char* text = pEntityElement->Attribute("ID"))
			{
				std::optional<Uuid> parsed = Uuid::Parse(text);
				if (!parsed || parsed->IsNil())
				{
					ENGINE_WARN("{0} has an invalid ID \"{1}\", given a new one{2}", NameOf(pEntityElement), text,
						Reflection::SceneXml::Where(file, pEntityElement->GetLineNum()));
				}
				else if (scene->GetEntityByID(*parsed))
					duplicate = true;
				else
					id = *parsed;
			}
			else
			{
				ENGINE_WARN("Entity ID not found{0}", Reflection::SceneXml::Where(file, pEntityElement->GetLineNum()));
			}
		}

		const char* entityName = pEntityElement->Attribute("Name");
		if (!entityName)
			ENGINE_WARN("Entity name not found{0}", Reflection::SceneXml::Where(file, pEntityElement->GetLineNum()));
		entt::entity handle = entt::null;
		if (withHandles)
			handle = entt::entity(pEntityElement->UnsignedAttribute("Handle", entt::to_integral(entt::entity(entt::null))));
		entity = scene->CreateEntity(id, entityName ? entityName : "", handle);
		if (duplicate)
			state.duplicates.emplace_back(pEntityElement, entity);
		if (resetUuid)
		{
			if (const char* text = pEntityElement->Attribute("ID"))
			{
				if (std::optional<Uuid> oldID = Uuid::Parse(text))
					state.newIDs[*oldID] = entity.GetID();
			}
			state.created.push_back(entity);
		}

		if (withHandles)
			CheckAttributes(pEntityElement, { "Name", "ID", "Handle" }, file);
		else
			CheckAttributes(pEntityElement, { "Name", "ID", "FormatVersion" }, file);
		Reflection::SceneXml::ReadComponents(pEntityElement, entity, file);

		// Made last to first, so the registry lists them in file order
		Entity nextChild;
		for (const tinyxml2::XMLElement* pChildElement = pEntityElement->LastChildElement("Entity"); pChildElement;
			pChildElement = pChildElement->PreviousSiblingElement("Entity"))
		{
			Entity child = ReadEntity(scene, pChildElement, resetUuid, state, file, withHandles);
			if (nextChild)
				SceneGraph::MoveBefore(child, nextChild);
			else
				SceneGraph::Reparent(child, entity);
			nextChild = child;
		}
		return entity;
	}

	// The entity elements with this ID, in file order
	void FindElementsWithID(const tinyxml2::XMLElement* pEntityElement, const Uuid& id, std::vector<const tinyxml2::XMLElement*>& found)
	{
		if (const char* text = pEntityElement->Attribute("ID"); text && Uuid::Parse(text) == id)
			found.push_back(pEntityElement);
		for (const tinyxml2::XMLElement* pChildElement = pEntityElement->FirstChildElement("Entity"); pChildElement;
			pChildElement = pChildElement->NextSiblingElement("Entity"))
			FindElementsWithID(pChildElement, id, found);
	}
}

/* ------------------------------------------------------------------------------------------------------------------ */

void SceneSerializer::ResolveDuplicateIDs(Scene* scene, const tinyxml2::XMLElement* pFirstElement, bool withSiblings,
	const Duplicates& duplicates, const std::filesystem::path& file)
{
	auto entityOf = [&duplicates](const tinyxml2::XMLElement* pElement)
		{
			for (const auto& [pDuplicate, entity] : duplicates)
				if (pDuplicate == pElement)
					return entity;
			return Entity();
		};

	std::vector<Uuid> resolved;
	for (const auto& [pDuplicate, duplicate] : duplicates)
	{
		const Uuid id = *Uuid::Parse(pDuplicate->Attribute("ID"));
		if (std::find(resolved.begin(), resolved.end(), id) != resolved.end())
			continue;
		resolved.push_back(id);

		std::vector<const tinyxml2::XMLElement*> elements;
		for (const tinyxml2::XMLElement* pElement = pFirstElement; pElement; pElement = withSiblings ? pElement->NextSiblingElement("Entity") : nullptr)
			FindElementsWithID(pElement, id, elements);

		// The element the ID's current holder was read from, if it came from this file
		Entity holder = scene->GetEntityByID(id);
		const tinyxml2::XMLElement* pHolderElement = nullptr;
		for (const tinyxml2::XMLElement* pElement : elements)
			if (!entityOf(pElement))
				pHolderElement = pElement;

		if (!pHolderElement)
		{
			for (const tinyxml2::XMLElement* pElement : elements)
				ENGINE_WARN("{0} has the same ID as {1} in the scene, given a new one{2}", NameOf(pElement), holder.GetName(),
					Reflection::SceneXml::Where(file, pElement->GetLineNum()));
			continue;
		}

		// Entities are read last to first, so the first in the file may not have been the one to keep it
		const tinyxml2::XMLElement* pKeeper = elements.front();
		if (pKeeper != pHolderElement)
			scene->SwapIDs(entityOf(pKeeper), holder);
		for (const tinyxml2::XMLElement* pElement : elements)
			if (pElement != pKeeper)
				ENGINE_WARN("{0} has the same ID as {1} on line {2}, given a new one{3}", NameOf(pElement), NameOf(pKeeper),
					pKeeper->GetLineNum(), Reflection::SceneXml::Where(file, pElement->GetLineNum()));
	}
}

/* ------------------------------------------------------------------------------------------------------------------ */

SceneSerializer::SceneSerializer(Scene* scene)
	:m_Scene(scene)
{
}

/* ------------------------------------------------------------------------------------------------------------------ */

bool SceneSerializer::Serialize(const std::filesystem::path& filepath) const
{
	PROFILE_FUNCTION();

	tinyxml2::XMLDocument doc;
	tinyxml2::XMLElement* pRoot = doc.NewElement("Scene");

	pRoot->SetAttribute("FormatVersion", Migration::CurrentVersion(Migration::FileType::Scene));

	WriteVector(pRoot->InsertNewChildElement("Gravity"), m_Scene->GetGravity());

	doc.InsertFirstChild(pRoot);

	for (Entity root : SceneGraph::GetRoots(*m_Scene))
		SerializeEntity(pRoot->InsertNewChildElement("Entity"), root);

	tinyxml2::XMLError error = doc.SaveFile(filepath.string().c_str());
	return error == tinyxml2::XML_SUCCESS;
}

/* ------------------------------------------------------------------------------------------------------------------ */

bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
{
	PROFILE_FUNCTION();

	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(filepath.string().c_str()) != tinyxml2::XML_SUCCESS)
	{
		ENGINE_ERROR("Could not load scene {0}. {1} on line {2}", filepath, doc.ErrorName(), doc.ErrorLineNum());
		return false;
	}
	return LoadXML(doc, filepath);
}

bool SceneSerializer::Deserialize(const std::filesystem::path& filepath, const std::vector<uint8_t>& data)
{
	PROFILE_FUNCTION();

	tinyxml2::XMLDocument doc;
	if (doc.Parse((const char*)data.data(), data.size()) != tinyxml2::XML_SUCCESS)
	{
		ENGINE_ERROR("Could not load scene {0}. {1} on line {2}", filepath, doc.ErrorName(), doc.ErrorLineNum());
		return false;
	}
	return LoadXML(doc, filepath);
}

bool SceneSerializer::LoadXML(tinyxml2::XMLDocument& doc, const std::filesystem::path& filepath)
{
	m_Scene->SetFilepath(filepath);
	m_Scene->m_NewerFormatFile.clear();

	if (!doc.FirstChildElement("Scene"))
	{
		ENGINE_ERROR("Not a valid scene file. Could not find Scene node");
		return false;
	}

	Migration::Report report;
	if (Migration::Upgrade(Migration::FileType::Scene, doc, filepath, report) == Migration::Result::TooNew)
		m_Scene->m_NewerFormatFile = filepath;

	tinyxml2::XMLElement* pRoot = doc.FirstChildElement("Scene");
	if (!pRoot)
		return false;

	CheckAttributes(pRoot, { "FormatVersion" }, filepath);
	for (const tinyxml2::XMLElement* child = pRoot->FirstChildElement(); child; child = child->NextSiblingElement())
	{
		if (std::strcmp(child->Name(), "Gravity") != 0 && std::strcmp(child->Name(), "Entity") != 0)
			ENGINE_WARN("Unknown element <{0}> in <Scene>, ignored{1}", child->Name(), Reflection::SceneXml::Where(filepath, child->GetLineNum()));
	}

	Vector2f gravity = m_Scene->GetGravity();
	SerializationUtils::Decode(pRoot->FirstChildElement("Gravity"), gravity);
	m_Scene->SetGravity(gravity);

	// Made last to first, so the registry lists them in file order; each goes before the
	// one after it, so the roots keep the file's order
	ReadState state;
	Entity nextRoot;
	for (const tinyxml2::XMLElement* pEntityElement = pRoot->LastChildElement("Entity"); pEntityElement;
		pEntityElement = pEntityElement->PreviousSiblingElement("Entity"))
	{
		Entity root = ReadEntity(m_Scene, pEntityElement, false, state, filepath);
		if (nextRoot)
			SceneGraph::MoveBefore(root, nextRoot);
		nextRoot = root;
	}
	if (!state.duplicates.empty())
		ResolveDuplicateIDs(m_Scene, pRoot->FirstChildElement("Entity"), true, state.duplicates, filepath);

	return true;
}

/* ------------------------------------------------------------------------------------------------------------------ */

void SceneSerializer::SerializeEntity(tinyxml2::XMLElement* pElement, Entity entity)
{
	PROFILE_FUNCTION();

	EnsureRegistered();
	WriteEntity(pElement, entity, false);
}

void SceneSerializer::WriteEntity(tinyxml2::XMLElement* pElement, Entity entity, bool withHandles)
{
	pElement->SetAttribute("Name", entity.GetName().c_str());
	pElement->SetAttribute("ID", entity.GetID().to_string().c_str());
	if (withHandles)
		pElement->SetAttribute("Handle", entt::to_integral(entity.GetHandle()));

	Reflection::SceneXml::WriteComponents(pElement, entity);

	for (Entity child : SceneGraph::GetChildren(entity))
		WriteEntity(pElement->InsertNewChildElement("Entity"), child, withHandles);
}

/* ------------------------------------------------------------------------------------------------------------------ */

void SceneSerializer::WriteSnapshot(Scene* scene, tinyxml2::XMLDocument& document)
{
	PROFILE_FUNCTION();

	EnsureRegistered();
	document.Clear();
	tinyxml2::XMLElement* pRoot = document.NewElement("Scene");
	document.InsertFirstChild(pRoot);
	for (Entity root : SceneGraph::GetRoots(*scene))
		WriteEntity(pRoot->InsertNewChildElement("Entity"), root, true);
}

void SceneSerializer::ReadSnapshot(Scene* scene, const tinyxml2::XMLDocument& document)
{
	PROFILE_FUNCTION();

	const tinyxml2::XMLElement* pRoot = document.FirstChildElement("Scene");
	if (!pRoot)
		return;

	// Made last to first, as a load does, each going before the one after it
	ReadState state;
	Entity nextRoot;
	for (const tinyxml2::XMLElement* pEntityElement = pRoot->LastChildElement("Entity"); pEntityElement;
		pEntityElement = pEntityElement->PreviousSiblingElement("Entity"))
	{
		Entity root = ReadEntity(scene, pEntityElement, false, state, k_Snapshot, true);
		if (nextRoot)
			SceneGraph::MoveBefore(root, nextRoot);
		nextRoot = root;
	}
}

Entity SceneSerializer::DeserializeEntity(Scene* scene, tinyxml2::XMLElement* pEntityElement, bool resetUuid)
{
	return ReadEntityTree(scene, pEntityElement, resetUuid, k_CopiedEntity);
}

Entity SceneSerializer::ReadEntityTree(Scene* scene, const tinyxml2::XMLElement* pEntityElement, bool resetUuid,
	const std::filesystem::path& file)
{
	std::vector<Entity> roots = ReadEntityTrees(scene, pEntityElement, false, resetUuid, file);
	return roots.empty() ? Entity() : roots.front();
}

std::vector<Entity> SceneSerializer::ReadEntityTrees(Scene* scene, const tinyxml2::XMLElement* pFirstElement, bool withSiblings,
	bool resetUuid, const std::filesystem::path& file)
{
	ReadState state;
	std::vector<Entity> roots;
	for (const tinyxml2::XMLElement* pElement = pFirstElement; pElement; pElement = withSiblings ? pElement->NextSiblingElement("Entity") : nullptr)
	{
		if (Entity root = ReadEntity(scene, pElement, resetUuid, state, file))
			roots.push_back(root);
	}
	if (!state.duplicates.empty())
		ResolveDuplicateIDs(scene, pFirstElement, withSiblings, state.duplicates, file);

	// References between the entities this load made point at the new ones; others are kept
	if (!state.newIDs.empty())
	{
		for (Entity entity : state.created)
			Reflection::RemapEntities(entity, state.newIDs);
	}
	return roots;
}

std::string SceneSerializer::SerializeEntity(Entity entity)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLElement* pEntityElement = doc.NewElement("Entity");
	doc.InsertFirstChild(pEntityElement);
	SceneSerializer::SerializeEntity(pEntityElement, entity);
	pEntityElement->SetAttribute("FormatVersion", Migration::CurrentVersion(Migration::FileType::Scene));
	tinyxml2::XMLPrinter printer;
	doc.Accept(&printer);
	return printer.CStr();
}

Entity SceneSerializer::DeserializeEntity(Scene* scene, const std::string& prefab, bool resetUuid)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError error = doc.Parse(prefab.c_str());

	if (error == tinyxml2::XMLError::XML_SUCCESS && doc.FirstChildElement("Entity"))
	{
		Migration::Report report;
		Migration::Upgrade(Migration::FileType::Scene, doc, k_PastedEntity, report);
		if (tinyxml2::XMLElement* pEntityElement = doc.FirstChildElement("Entity"))
			return ReadEntityTree(scene, pEntityElement, resetUuid, k_PastedEntity);
	}
	return Entity();
}

std::string SceneSerializer::SerializeEntities(const std::vector<Entity>& roots)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLElement* pEntitiesElement = doc.NewElement("Entities");
	doc.InsertFirstChild(pEntitiesElement);
	for (Entity root : roots)
		SceneSerializer::SerializeEntity(pEntitiesElement->InsertNewChildElement("Entity"), root);
	pEntitiesElement->SetAttribute("FormatVersion", Migration::CurrentVersion(Migration::FileType::Scene));
	tinyxml2::XMLPrinter printer;
	doc.Accept(&printer);
	return printer.CStr();
}

std::vector<Entity> SceneSerializer::DeserializeEntities(Scene* scene, const std::string& text, bool resetUuid)
{
	std::vector<Entity> roots;
	tinyxml2::XMLDocument doc;
	if (doc.Parse(text.c_str()) != tinyxml2::XMLError::XML_SUCCESS || !doc.RootElement())
		return roots;

	const bool single = std::strcmp(doc.RootElement()->Name(), "Entity") == 0;
	if (!single && std::strcmp(doc.RootElement()->Name(), "Entities") != 0)
		return roots;

	Migration::Report report;
	Migration::Upgrade(Migration::FileType::Scene, doc, k_PastedEntity, report);
	tinyxml2::XMLElement* pRoot = doc.RootElement();
	if (!pRoot)
		return roots;

	if (single)
		return ReadEntityTrees(scene, pRoot, false, resetUuid, k_PastedEntity);
	return ReadEntityTrees(scene, pRoot->FirstChildElement("Entity"), true, resetUuid, k_PastedEntity);
}

std::vector<Entity> SceneSerializer::CopyEntities(Scene* scene, const std::vector<Entity>& sources)
{
	PROFILE_FUNCTION();

	EnsureRegistered();
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLElement* pEntitiesElement = doc.NewElement("Entities");
	doc.InsertFirstChild(pEntitiesElement);
	for (Entity source : sources)
		WriteEntity(pEntitiesElement->InsertNewChildElement("Entity"), source, false);
	return ReadEntityTrees(scene, pEntitiesElement->FirstChildElement("Entity"), true, true, k_CopiedEntity);
}
