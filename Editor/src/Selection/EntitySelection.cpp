#include "EntitySelection.h"

#include "Scene/Scene.h"

#include <algorithm>

void EntitySelection::Select(const Uuid& id)
{
	m_Ids.clear();
	m_Set.clear();
	m_Ids.push_back(id);
	m_Set.insert(id);
	m_Active = id;
	++m_Version;
}

void EntitySelection::Add(const Uuid& id)
{
	if (m_Set.insert(id).second)
		m_Ids.push_back(id);
	m_Active = id;
	++m_Version;
}

void EntitySelection::Remove(const Uuid& id)
{
	if (m_Set.erase(id) == 0)
		return;
	m_Ids.erase(std::find(m_Ids.begin(), m_Ids.end(), id));
	// The most recently selected one left takes over
	if (m_Active == id)
		m_Active = m_Ids.empty() ? Uuid::Nil() : m_Ids.back();
	++m_Version;
}

void EntitySelection::Toggle(const Uuid& id)
{
	if (Contains(id))
		Remove(id);
	else
		Add(id);
}

void EntitySelection::SetAll(std::vector<Uuid> ids, const Uuid& active)
{
	m_Ids.clear();
	m_Set.clear();
	for (const Uuid& id : ids)
	{
		if (m_Set.insert(id).second)
			m_Ids.push_back(id);
	}
	if (!m_Ids.empty() && m_Set.count(active))
		m_Active = active;
	else
		m_Active = m_Ids.empty() ? Uuid::Nil() : m_Ids.back();
	++m_Version;
}

void EntitySelection::Clear()
{
	m_Ids.clear();
	m_Set.clear();
	m_Active = Uuid::Nil();
	++m_Version;
}

void EntitySelection::Restore(Scene& scene, const SelectionState& state)
{
	std::vector<Uuid> existing;
	for (const Uuid& id : state.ids)
	{
		if (scene.GetEntityByID(id))
			existing.push_back(id);
	}
	SetAll(std::move(existing), state.active);
}

std::vector<Entity> EntitySelection::Resolve(Scene& scene) const
{
	std::vector<Entity> entities;
	entities.reserve(m_Ids.size());
	for (const Uuid& id : m_Ids)
	{
		if (Entity entity = scene.GetEntityByID(id))
			entities.push_back(entity);
	}
	return entities;
}

std::vector<Entity> EntitySelection::GetTopLevel(Scene& scene) const
{
	std::vector<Entity> topLevel;
	for (Entity entity : Resolve(scene))
	{
		bool underSelected = false;
		for (Entity ancestor = entity.GetParent(); ancestor && !underSelected; ancestor = ancestor.GetParent())
			underSelected = Contains(ancestor.GetID());
		if (!underSelected)
			topLevel.push_back(entity);
	}
	return topLevel;
}

Entity EntitySelection::GetActiveEntity(Scene& scene) const
{
	return m_Active.IsNil() ? Entity() : scene.GetEntityByID(m_Active);
}

bool EntitySelection::HasLive(Scene& scene) const
{
	for (const Uuid& id : m_Ids)
	{
		if (scene.GetEntityByID(id))
			return true;
	}
	return false;
}

void EntitySelection::Invert(const std::vector<Uuid>& all)
{
	std::vector<Uuid> inverted;
	for (const Uuid& id : all)
	{
		if (!Contains(id))
			inverted.push_back(id);
	}
	SetAll(std::move(inverted));
}
