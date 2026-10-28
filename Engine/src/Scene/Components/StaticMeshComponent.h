#pragma once

#include "Core/core.h"
#include "Asset/StaticMesh.h"
#include "Asset/Material.h"
#include "Scene/AssetManager.h"

struct StaticMeshComponent
{
	Ref<StaticMesh> mesh;
	// Indexed by the mesh's material slots; an empty entry uses the mesh's own material
	std::vector<Ref<Material>> materialOverrides;

	StaticMeshComponent() = default;
	StaticMeshComponent(const StaticMeshComponent&) = default;
	StaticMeshComponent(Ref<StaticMesh> mesh)
		:mesh(mesh) {}

	// Keeps the overrides for slots the new mesh still has
	void SetMesh(const Ref<StaticMesh> newMesh)
	{
		mesh = newMesh;
		if (!mesh)
			materialOverrides.clear();
		else if (!mesh->IsMissing() && materialOverrides.size() > GetMaterialSlotCount())
			materialOverrides.resize(GetMaterialSlotCount());
	}

	// A missing mesh's slots are unknown, so its overrides are all kept
	size_t GetMaterialSlotCount() const
	{
		if (!mesh || !mesh->GetMesh())
			return 0;
		return mesh->IsMissing() ? materialOverrides.size() : mesh->GetMesh()->GetMaterials().size();
	}

	// The mesh's own material for a slot, or null
	Ref<Material> GetMeshMaterial(size_t slot) const
	{
		if (!mesh || !mesh->GetMesh() || mesh->IsMissing())
			return nullptr;
		const std::vector<Ref<Material>>& materials = mesh->GetMesh()->GetMaterials();
		return slot < materials.size() ? materials[slot] : nullptr;
	}

	// The material a slot draws with: its override, otherwise the mesh's own
	Ref<Material> GetMaterial(size_t slot) const
	{
		if (slot < materialOverrides.size() && materialOverrides[slot])
			return materialOverrides[slot];
		return GetMeshMaterial(slot);
	}

	// Null, or the mesh's own material, removes the override
	void SetMaterialOverride(size_t slot, const Ref<Material>& material)
	{
		Ref<Material> slotOverride = material == GetMeshMaterial(slot) ? nullptr : material;
		if (slot >= materialOverrides.size())
		{
			if (!slotOverride)
				return;
			materialOverrides.resize(slot + 1);
		}
		materialOverrides[slot] = slotOverride;
		while (!materialOverrides.empty() && !materialOverrides.back())
			materialOverrides.pop_back();
	}

private:
	friend cereal::access;
	template<typename Archive>
	void save(Archive& archive) const
	{
		std::string relativeMeshPath;
		std::vector<std::string> relativeMaterials;
		if (mesh)
		{
			if (!mesh->GetFilepath().empty())
				relativeMeshPath = mesh->GetFilepath().string();

			for (const Ref<Material>& materialOverride : materialOverrides)
				relativeMaterials.push_back(materialOverride ? materialOverride->GetFilepath().string() : std::string());
		}
		archive(cereal::make_nvp("Mesh", relativeMeshPath));
		archive(cereal::make_nvp("MaterialOverrides", relativeMaterials));
	}

	template<typename Archive>
	void load(Archive& archive)
	{
		std::string relativeMeshPath;
		std::vector<std::string> relativeMaterials;
		archive(cereal::make_nvp("Mesh", relativeMeshPath));
		archive(cereal::make_nvp("MaterialOverrides", relativeMaterials));

		mesh = relativeMeshPath.empty() ? nullptr : AssetManager::GetAsset<StaticMesh>(relativeMeshPath);
		materialOverrides.clear();
		if (!mesh)
			return;

		for (const std::string& relativeMaterial : relativeMaterials)
			materialOverrides.push_back(relativeMaterial.empty() ? nullptr : AssetManager::GetAsset<Material>(relativeMaterial));
	}
};
