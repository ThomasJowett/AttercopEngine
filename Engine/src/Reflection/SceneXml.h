#pragma once

#include "Physics/CollisionLayers.h"

#include <filesystem>
#include <string>

class Entity;

namespace tinyxml2
{
	class XMLElement;
}

namespace Reflection
{
	struct ComponentType;

	// Scene XML written and read from the component descriptions
	namespace SceneXml
	{
		// The shortest text that reads back to the same float: "-9.81", not "-9.8100004"
		std::string FloatToText(float value);

		// Radians as the shortest degrees that DegreesToRadians reads back to the same float: "90", not "90.0000025"
		std::string RadiansToDegreesText(float radians);
		float DegreesToRadians(double degrees);

		// Writes each component the entity has, in registration order, into its <Entity> element; internal
		// components are left to the caller
		void WriteComponents(tinyxml2::XMLElement* entityElement, Entity entity);
		void WriteComponent(tinyxml2::XMLElement* entityElement, Entity entity, const ComponentType& type);

		// Adds and reads each component in an <Entity> element, then runs its OnChanged hooks once. Child
		// <Entity> elements are left to the caller; other unknown elements and attributes are warned about,
		// naming the file and line
		void ReadComponents(const tinyxml2::XMLElement* entityElement, Entity entity, const std::filesystem::path& file);
		void ReadComponent(const tinyxml2::XMLElement* element, Entity entity, const ComponentType& type,
			const std::filesystem::path& file);

		// " (file, line N)", for warnings
		std::string Where(const std::filesystem::path& file, int line);

		// Rewrites every collision layer and mask in a scene or entity document from the old layer names to the new,
		// keeping each collider's layers; unknown names are left as they are. False if nothing changed
		bool RenameCollisionLayers(tinyxml2::XMLElement* root, const CollisionLayers::Names& oldNames,
			const CollisionLayers::Names& newNames);
	}
}
