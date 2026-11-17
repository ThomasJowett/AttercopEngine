#include "StaticMeshComponent.h"

#include "Reflection/Describe.h"
#include "Utilities/SerializationUtils.h"

#include "TinyXml2/tinyxml2.h"

namespace StaticMeshComponentReflection
{
	const Ref<StaticMesh>& GetMesh(const StaticMeshComponent& c) { return c.mesh; }

	// <Material Slot="2" Filepath="..."/> for each overridden slot
	void SaveMaterials(const StaticMeshComponent& c, tinyxml2::XMLElement* element)
	{
		for (size_t slot = 0; slot < c.materialOverrides.size(); ++slot)
		{
			const Ref<Material>& materialOverride = c.materialOverrides[slot];
			if (!materialOverride)
				continue;
			tinyxml2::XMLElement* materialElement = element->InsertNewChildElement("Material");
			materialElement->SetAttribute("Slot", (uint32_t)slot);
			SerializationUtils::Encode(materialElement, materialOverride->GetFilepath());
		}
	}

	// A missing mesh's slots are unknown, so its overrides are kept whatever the slot
	void LoadMaterials(StaticMeshComponent& c, const tinyxml2::XMLElement* element)
	{
		c.materialOverrides.clear();
		if (!c.mesh)
			return;
		for (const tinyxml2::XMLElement* materialElement = element->FirstChildElement("Material"); materialElement;
			materialElement = materialElement->NextSiblingElement("Material"))
		{
			uint32_t slot = 0;
			const char* filepath = materialElement->Attribute("Filepath");
			if (materialElement->QueryUnsignedAttribute("Slot", &slot) != tinyxml2::XML_SUCCESS || !filepath)
				continue;
			Ref<Material> material = AssetManager::GetAsset<Material>(filepath);
			if (!material)
				continue;
			if (c.mesh->IsMissing())
			{
				if (slot >= c.materialOverrides.size())
					c.materialOverrides.resize(slot + 1);
				c.materialOverrides[slot] = material;
			}
			else if (slot < c.GetMaterialSlotCount())
			{
				c.SetMaterialOverride(slot, material);
			}
		}
	}
}

void StaticMeshComponent::Describe(Reflection::Builder<StaticMeshComponent>& c)
{
	using namespace StaticMeshComponentReflection;

	c.Name("StaticMesh").Category("Rendering 3D")
		.Description("Draws an imported mesh, with its own materials or overrides per slot");

	c.Property<&GetMesh, &StaticMeshComponent::SetMesh>("Mesh", "The mesh to draw").MainAsset();
	c.Custom<&SaveMaterials, &LoadMaterials>("Materials", "Materials that replace the mesh's own, by slot").Item("Material");
}
