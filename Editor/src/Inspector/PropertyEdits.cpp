#include "PropertyEdits.h"

#include "History/PropertyCommands.h"
#include "History/SceneHistory.h"
#include "Scene/Scene.h"

namespace
{
	Reflection::PropertyPath WholeField(Reflection::PropertyPath path)
	{
		path.part = -1;
		return path;
	}
}

void PropertyEdits::Begin(const std::vector<Entity>& entities, const Reflection::PropertyPath& path)
{
	End();
	m_Interaction = NewInteraction();
	m_Path = WholeField(path);
	for (Entity entity : entities)
		m_Entities.push_back(entity.GetID());

	if (path.Field().Has(Reflection::FieldFlags::EditFromStart))
	{
		for (Entity entity : entities)
		{
			m_Start.push_back(Reflection::Capture(entity, *path.component));
			m_StartValues.push_back(Reflection::Get(entity, m_Path));
		}
	}
}

bool PropertyEdits::Set(const std::vector<Entity>& entities, const Reflection::PropertyPath& path, const Reflection::Value& value)
{
	if (entities.empty())
		return false;

	const bool inInteraction = IsOn(entities, path);
	const uint32_t interaction = InteractionFor(entities, path);

	if (m_History)
		HistoryCommands::RecordProperty(*m_History, interaction, entities, WholeField(path));

	bool changed = false;
	for (size_t i = 0; i < entities.size(); ++i)
	{
		Entity entity = entities[i];
		if (inInteraction && i < m_Start.size() && Reflection::Get(entity, m_Path) != m_StartValues[i])
			Reflection::Restore(entity, m_Start[i], Reflection::ChangeSource::Editor, interaction);
		changed = Reflection::Set(entity, path, value, Reflection::ChangeSource::Editor, interaction) || changed;
	}
	return changed;
}

void PropertyEdits::End()
{
	m_Interaction = 0;
	m_Entities.clear();
	m_Path = {};
	m_Start.clear();
	m_StartValues.clear();
}

void PropertyEdits::Record(const std::vector<Entity>& entities, const Reflection::PropertyPath& path)
{
	if (m_History)
		HistoryCommands::RecordProperty(*m_History, InteractionFor(entities, path), entities, WholeField(path));
}

void PropertyEdits::RecordComponent(const std::vector<Entity>& entities, const Reflection::ComponentType& type)
{
	if (m_History)
		HistoryCommands::RecordComponent(*m_History, NewInteraction(), entities, type);
}

uint32_t PropertyEdits::NewInteraction()
{
	static uint32_t s_NextInteraction = 1;
	return s_NextInteraction++;
}

bool PropertyEdits::IsOn(const std::vector<Entity>& entities, const Reflection::PropertyPath& path) const
{
	if (m_Interaction == 0 || m_Path != WholeField(path) || m_Entities.size() != entities.size())
		return false;
	for (size_t i = 0; i < entities.size(); ++i)
	{
		Entity entity = entities[i];
		if (m_Entities[i] != entity.GetID())
			return false;
	}
	return true;
}

uint32_t PropertyEdits::InteractionFor(const std::vector<Entity>& entities, const Reflection::PropertyPath& path)
{
	return IsOn(entities, path) ? m_Interaction : NewInteraction();
}
