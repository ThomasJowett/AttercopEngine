#pragma once

#include "Core/UUID.h"
#include "Scene/Entity.h"

#include <cstdint>
#include <unordered_set>
#include <vector>

class Scene;

// A selection as stored in a history step
struct SelectionState
{
	std::vector<Uuid> ids;
	Uuid active = Uuid::Nil();
};

// The selected entities by UUID, in the order they were selected, with one of them active
class EntitySelection
{
public:
	// Replaces the selection; id becomes active
	void Select(const Uuid& id);
	// Adds id and makes it active
	void Add(const Uuid& id);
	void Remove(const Uuid& id);
	void Toggle(const Uuid& id);
	// A nil active takes the last of ids
	void SetAll(std::vector<Uuid> ids, const Uuid& active = Uuid::Nil());
	void Clear();

	bool Contains(const Uuid& id) const { return m_Set.count(id) > 0; }
	bool IsEmpty() const { return m_Ids.empty(); }
	size_t Count() const { return m_Ids.size(); }
	// Nil when nothing is selected
	Uuid GetActive() const { return m_Active; }
	// In the order they were selected
	const std::vector<Uuid>& GetAll() const { return m_Ids; }

	SelectionState Capture() const { return { m_Ids, m_Active }; }
	// Selects what still exists in the scene, ignoring the rest
	void Restore(Scene& scene, const SelectionState& state);

	// Live entities in selection order, skipping UUIDs no longer in the scene
	std::vector<Entity> Resolve(Scene& scene) const;
	// Selected entities with no selected ancestor, in selection order
	std::vector<Entity> GetTopLevel(Scene& scene) const;
	// The active entity, or a null entity if it isn't in the scene
	Entity GetActiveEntity(Scene& scene) const;
	// True if any selected entity is in the scene
	bool HasLive(Scene& scene) const;
	// Selects the entities in all that aren't selected; the last becomes active
	void Invert(const std::vector<Uuid>& all);

	// Changes on every edit, for panels that cache per selection
	uint64_t GetVersion() const { return m_Version; }

private:
	std::vector<Uuid> m_Ids;
	std::unordered_set<Uuid> m_Set;
	Uuid m_Active = Uuid::Nil();
	uint64_t m_Version = 0;
};
