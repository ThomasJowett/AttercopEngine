#include "SceneSerializer.h"

#include "Scene/Entity.h"
#include "Scene/SceneGraph.h"
#include "Components.h"
#include "Migration/Migration.h"
#include "Reflection/SceneXml.h"
#include "Utilities/SerializationUtils.h"

#include "TinyXml2/tinyxml2.h"

#include <cstring>

namespace
{
	// For warnings about entities that didn't come from a file
	const std::filesystem::path k_CopiedEntity = "a copied entity";
	const std::filesystem::path k_PastedEntity = "the pasted entity";

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

	Entity ReadEntity(Scene* scene, const tinyxml2::XMLElement* pEntityElement, bool resetUuid, const std::filesystem::path& file)
	{
		PROFILE_FUNCTION();

		Entity entity;
		if (!pEntityElement)
			return entity;
		EnsureRegistered();
		const char* entityName = pEntityElement->Attribute("Name");

		if (const char* uuidChar = pEntityElement->Attribute("ID");
			uuidChar && entityName && !resetUuid)
		{
			entity = scene->CreateEntity(Uuid(uuidChar), entityName);
		}
		else if (entityName && resetUuid)
		{
			entity = scene->CreateEntity(Uuid(), entityName);
		}
		else if (entityName)
		{
			ENGINE_WARN("Entity ID not found{0}", Reflection::SceneXml::Where(file, pEntityElement->GetLineNum()));
			entity = scene->CreateEntity(entityName);
		}
		else
		{
			ENGINE_WARN("Entity name not found{0}", Reflection::SceneXml::Where(file, pEntityElement->GetLineNum()));
			entity = scene->CreateEntity();
		}

		CheckAttributes(pEntityElement, { "Name", "ID", "FormatVersion" }, file);
		Reflection::SceneXml::ReadComponents(pEntityElement, entity, file);

		// Made last to first, so the registry lists them in file order
		Entity nextChild;
		for (const tinyxml2::XMLElement* pChildElement = pEntityElement->LastChildElement("Entity"); pChildElement;
			pChildElement = pChildElement->PreviousSiblingElement("Entity"))
		{
			Entity child = ReadEntity(scene, pChildElement, resetUuid, file);
			if (nextChild)
				SceneGraph::MoveBefore(child, nextChild);
			else
				SceneGraph::Reparent(child, entity);
			nextChild = child;
		}
		return entity;
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
	Entity nextRoot;
	for (const tinyxml2::XMLElement* pEntityElement = pRoot->LastChildElement("Entity"); pEntityElement;
		pEntityElement = pEntityElement->PreviousSiblingElement("Entity"))
	{
		Entity root = ReadEntity(m_Scene, pEntityElement, false, filepath);
		if (nextRoot)
			SceneGraph::MoveBefore(root, nextRoot);
		nextRoot = root;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------------------------ */

void SceneSerializer::SerializeEntity(tinyxml2::XMLElement* pElement, Entity entity)
{
	PROFILE_FUNCTION();

	EnsureRegistered();
	pElement->SetAttribute("Name", entity.GetName().c_str());
	pElement->SetAttribute("ID", entity.GetID().to_string().c_str());

	Reflection::SceneXml::WriteComponents(pElement, entity);

	for (Entity child : SceneGraph::GetChildren(entity))
		SerializeEntity(pElement->InsertNewChildElement("Entity"), child);
}

Entity SceneSerializer::DeserializeEntity(Scene* scene, tinyxml2::XMLElement* pEntityElement, bool resetUuid)
{
	return ReadEntity(scene, pEntityElement, resetUuid, k_CopiedEntity);
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
			return ReadEntity(scene, pEntityElement, resetUuid, k_PastedEntity);
	}
	return Entity();
}
