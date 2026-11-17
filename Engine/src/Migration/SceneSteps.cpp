#include "Migration/MigrationSteps.h"

#include "Core/Application.h"
#include "Logging/Logger.h"
#include "Physics/CollisionLayers.h"
#include "Utilities/FileUtils.h"

#include "TinyXml2/tinyxml2.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace Migration
{
	namespace
	{
		// The names of a 0.0.3 enum's numbers, in order
		using Names = std::vector<const char*>;

		const Names k_ProjectionTypes = { "Perspective", "Orthographic" };
		const Names k_FilterMethods = { "Linear", "Nearest" };
		const Names k_WrapMethods = { "Clamp", "Mirror", "Repeat" };
		const Names k_BodyTypes = { "Static", "Kinematic", "Dynamic" };
		const Names k_PrimitiveShapes = { "Cube", "Sphere", "Plane", "Cylinder", "Cone", "Torus" };
		const Names k_CapsuleDirections = { "Vertical", "Horizontal" };
		const Names k_BillboardOrientations = { "WorldUp", "Camera" };

		const char* const k_ButtonStates[] = { "Normal", "Hovered", "Clicked", "Disabled" };
		const char* const k_Colliders[] = { "BoxCollider2D", "CircleCollider2D", "CapsuleCollider2D", "PolygonCollider2D", "Tilemap" };

		void Warn(Report& report, const tinyxml2::XMLElement* element, const std::string& message)
		{
			std::string text = message + " (line " + std::to_string(element->GetLineNum()) + ")";
			ENGINE_WARN("{0}", text);
			report.warnings.push_back(text);
		}

		// The name of a number, or nullptr if it isn't one of them
		const char* NameOf(const Names& names, const char* text)
		{
			int number = 0;
			if (!text || !tinyxml2::XMLUtil::ToInt(text, &number) || number < 0 || number >= (int)names.size())
				return nullptr;
			return names[number];
		}

		// Sets an attribute, renamed or not, keeping its place among the others
		void ReplaceAttribute(tinyxml2::XMLElement* element, const char* oldName, const char* newName, const std::string& value)
		{
			std::vector<std::pair<std::string, std::string>> attributes;
			for (const tinyxml2::XMLAttribute* attribute = element->FirstAttribute(); attribute; attribute = attribute->Next())
			{
				if (std::strcmp(attribute->Name(), oldName) == 0)
					attributes.emplace_back(newName, value);
				else
					attributes.emplace_back(attribute->Name(), attribute->Value());
			}
			for (const auto& [name, attributeValue] : attributes)
				element->DeleteAttribute(name.c_str());
			element->DeleteAttribute(oldName);
			for (const auto& [name, attributeValue] : attributes)
				element->SetAttribute(name.c_str(), attributeValue.c_str());
		}

		// A number attribute to its name, renamed or not; an unknown number is left for the loader to warn about
		void NumberToName(tinyxml2::XMLElement* element, const char* oldName, const char* newName, const Names& names, Report& report)
		{
			const char* text = element->Attribute(oldName);
			if (!text)
				return;
			if (const char* name = NameOf(names, text))
				ReplaceAttribute(element, oldName, newName, name);
			else
			{
				Warn(report, element, std::string("<") + element->Name() + "> " + oldName + "=\"" + text + "\" isn't a known value");
				ReplaceAttribute(element, oldName, newName, text);
			}
		}

		// Moves a number attribute from one element to another, as its name
		void MoveAsName(tinyxml2::XMLElement* from, const char* name, tinyxml2::XMLElement* to, const char* newName,
			const Names& names, Report& report)
		{
			const char* text = from->Attribute(name);
			if (!text)
				return;
			const char* value = NameOf(names, text);
			if (!value)
			{
				Warn(report, from, std::string("<") + from->Name() + "> " + name + "=\"" + text + "\" isn't a known value");
				value = text;
			}
			to->SetAttribute(newName, value);
			from->DeleteAttribute(name);
		}

		/* ---------------------------------------------------------------------------------------------------------- */

		// <SceneCamera ProjectionType="1" ... FOV="..."/> folded into <Camera Projection="Orthographic" ... Fov="...">
		void UpgradeCamera(tinyxml2::XMLElement* camera, Report& report)
		{
			tinyxml2::XMLElement* sceneCamera = camera->FirstChildElement("SceneCamera");
			if (!sceneCamera)
				return;
			MoveAsName(sceneCamera, "ProjectionType", camera, "Projection", k_ProjectionTypes, report);
			for (const char* name : { "OrthoSize", "OrthoNear", "OrthoFar", "PerspectiveNear", "PerspectiveFar", "FOV", "AspectRatio" })
			{
				if (const char* value = sceneCamera->Attribute(name))
					camera->SetAttribute(std::strcmp(name, "FOV") == 0 ? "Fov" : name, value);
			}
			camera->DeleteChild(sceneCamera);
		}

		// <Texture FilterMethod="1" WrapMethod="2"/> to FilterMethod="Nearest" WrapMethod="Repeat" on <Sprite>
		void UpgradeSprite(tinyxml2::XMLElement* sprite, Report& report)
		{
			if (tinyxml2::XMLElement* texture = sprite->FirstChildElement("Texture"))
			{
				MoveAsName(texture, "FilterMethod", sprite, "FilterMethod", k_FilterMethods, report);
				MoveAsName(texture, "WrapMethod", sprite, "WrapMethod", k_WrapMethods, report);
			}
		}

		// Each <NormalTexture FilterMethod WrapMethod> to NormalFilterMethod and NormalWrapMethod names on <Button>
		void UpgradeButton(tinyxml2::XMLElement* button, Report& report)
		{
			for (const char* method : { "FilterMethod", "WrapMethod" })
			{
				for (const char* state : k_ButtonStates)
				{
					tinyxml2::XMLElement* texture = button->FirstChildElement((std::string(state) + "Texture").c_str());
					if (texture)
						MoveAsName(texture, method, button, (std::string(state) + method).c_str(),
							std::strcmp(method, "FilterMethod") == 0 ? k_FilterMethods : k_WrapMethods, report);
				}
			}
		}

		// A path saved absolute is made relative to the project
		void UpgradeBehaviourTree(tinyxml2::XMLElement* behaviourTree)
		{
			const char* filepath = behaviourTree->Attribute("Filepath");
			if (!filepath || !std::filesystem::path(filepath).is_absolute())
				return;
			std::filesystem::path relative = FileUtils::RelativePath(filepath, Application::GetOpenDocumentDirectory());
			behaviourTree->SetAttribute("Filepath", relative.generic_string().c_str());
		}

		// Layer and Mask bits to the names in the project's collision layers file
		void UpgradeCollisionFilter(tinyxml2::XMLElement* collider, Report& report)
		{
			if (const char* text = collider->Attribute("Layer"))
			{
				unsigned bits = 0;
				if (!tinyxml2::XMLUtil::ToUnsigned(text, &bits))
					Warn(report, collider, std::string("<") + collider->Name() + "> Layer=\"" + text + "\" isn't a number");
				else
				{
					uint16_t layer = (uint16_t)bits;
					if (layer == 0 || (layer & (layer - 1)) != 0)
					{
						layer = layer == 0 ? CollisionLayers::DefaultLayer : (uint16_t)(1u << CollisionLayers::GetLayerIndex(layer));
						Warn(report, collider, std::string("<") + collider->Name() + "> Layer=\"" + text
							+ "\" isn't one layer, so it's on " + CollisionLayers::GetLayerName(layer));
					}
					ReplaceAttribute(collider, "Layer", "Layer", CollisionLayers::GetLayerName(layer));
				}
			}

			if (const char* text = collider->Attribute("Mask"))
			{
				unsigned bits = 0;
				if (!tinyxml2::XMLUtil::ToUnsigned(text, &bits))
				{
					Warn(report, collider, std::string("<") + collider->Name() + "> Mask=\"" + text + "\" isn't a number");
					return;
				}
				const uint16_t mask = (uint16_t)bits;
				std::string names;
				if (mask == CollisionLayers::Everything)
					names = "Everything";
				else if (mask == 0)
					names = "Nothing";
				else
				{
					for (int i = 0; i < CollisionLayers::MaxLayers; ++i)
					{
						if (mask & (1u << i))
							names += (names.empty() ? "" : ", ") + CollisionLayers::GetDisplayName(i);
					}
				}
				ReplaceAttribute(collider, "Mask", "Mask", names);
			}
		}

		// 0.0.1 saved one <MaterialOverride> per submesh, which becomes the slot of the same number
		void UpgradeStaticMesh(tinyxml2::XMLElement* staticMesh, Report& report)
		{
			uint32_t submesh = 0;
			for (tinyxml2::XMLElement* materialOverride = staticMesh->FirstChildElement("MaterialOverride"); materialOverride;
				materialOverride = materialOverride->NextSiblingElement("MaterialOverride"), ++submesh)
			{
				// <Material Slot="2" Filepath="..."/>, as 0.0.3 writes it
				const std::string filepath = materialOverride->Attribute("Filepath") ? materialOverride->Attribute("Filepath") : "";
				materialOverride->SetName("Material");
				materialOverride->DeleteAttribute("Filepath");
				materialOverride->SetAttribute("Slot", submesh);
				materialOverride->SetAttribute("Filepath", filepath.c_str());
			}
			if (submesh > 0)
				Warn(report, staticMesh, "<StaticMesh> material overrides from 0.0.1 are kept by submesh number, which may not be the material slot; check them in the inspector");
		}

		void UpgradeEntity(tinyxml2::XMLElement* entity, Report& report)
		{
			for (tinyxml2::XMLElement* component = entity->FirstChildElement(); component; component = component->NextSiblingElement())
			{
				const char* name = component->Name();
				if (std::strcmp(name, "Entity") == 0)
					UpgradeEntity(component, report);
				else if (std::strcmp(name, "Camera") == 0)
					UpgradeCamera(component, report);
				else if (std::strcmp(name, "Sprite") == 0)
					UpgradeSprite(component, report);
				else if (std::strcmp(name, "Button") == 0)
					UpgradeButton(component, report);
				else if (std::strcmp(name, "RigidBody2D") == 0)
					NumberToName(component, "BodyType", "Type", k_BodyTypes, report);
				else if (std::strcmp(name, "Primitive") == 0)
					NumberToName(component, "Type", "Type", k_PrimitiveShapes, report);
				else if (std::strcmp(name, "Billboard") == 0)
					NumberToName(component, "Orientation", "Orientation", k_BillboardOrientations, report);
				else if (std::strcmp(name, "BehaviourTree") == 0)
					UpgradeBehaviourTree(component);
				else if (std::strcmp(name, "StaticMesh") == 0)
					UpgradeStaticMesh(component, report);

				if (std::strcmp(name, "CapsuleCollider2D") == 0)
					NumberToName(component, "Direction", "Direction", k_CapsuleDirections, report);
				for (const char* collider : k_Colliders)
				{
					if (std::strcmp(name, collider) == 0)
						UpgradeCollisionFilter(component, report);
				}
			}
		}

		// The components written from their descriptions (reflection.md section 7)
		bool UpgradeToReflectionFormat(tinyxml2::XMLDocument& document, Report& report)
		{
			tinyxml2::XMLElement* root = document.RootElement();
			if (std::strcmp(root->Name(), "Entity") == 0)
			{
				UpgradeEntity(root, report);
				return true;
			}
			for (tinyxml2::XMLElement* entity = root->FirstChildElement("Entity"); entity; entity = entity->NextSiblingElement("Entity"))
				UpgradeEntity(entity, report);
			return true;
		}
	}

	std::vector<Step> SceneSteps()
	{
		return {
			{ FileType::Scene, 1, "Components saved from their descriptions: enums and collision layers by name, the camera and sprite settings moved", &UpgradeToReflectionFormat },
		};
	}
}
