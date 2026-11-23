#include "PropertyEdits.h"

#include "History/PropertyCommands.h"
#include "History/SceneHistory.h"
#include "Scene/Scene.h"

void PropertyEdits::Begin(Entity entity, const Reflection::PropertyPath& path)
{
	End();
	m_Interaction = NewInteraction();
	m_Entity = entity.GetID();
	m_Path = path;
	if (path.Field().Has(Reflection::FieldFlags::EditFromStart))
	{
		m_Start = Reflection::Capture(entity, *path.component);
		m_StartValue = Reflection::Get(entity, path);
	}
}

bool PropertyEdits::Set(Entity entity, const Reflection::PropertyPath& path, const Reflection::Value& value)
{
	const bool inInteraction = IsOn(entity, path);
	const uint32_t interaction = InteractionFor(entity, path);

	if (m_History)
		HistoryCommands::RecordProperty(*m_History, interaction, { entity }, path);

	if (inInteraction && m_Start && Reflection::Get(entity, path) != m_StartValue)
		Reflection::Restore(entity, *m_Start, Reflection::ChangeSource::Editor, interaction);
	return Reflection::Set(entity, path, value, Reflection::ChangeSource::Editor, interaction);
}

void PropertyEdits::End()
{
	m_Interaction = 0;
	m_Entity.reset();
	m_Path = {};
	m_Start.reset();
	m_StartValue.reset();
}

void PropertyEdits::Record(Entity entity, const Reflection::PropertyPath& path)
{
	if (m_History)
		HistoryCommands::RecordProperty(*m_History, InteractionFor(entity, path), { entity }, path);
}

void PropertyEdits::RecordComponent(Entity entity, const Reflection::ComponentType& type)
{
	if (m_History)
		HistoryCommands::RecordComponent(*m_History, NewInteraction(), { entity }, type);
}

uint32_t PropertyEdits::NewInteraction()
{
	static uint32_t s_NextInteraction = 1;
	return s_NextInteraction++;
}

bool PropertyEdits::IsOn(Entity entity, const Reflection::PropertyPath& path) const
{
	return m_Interaction != 0 && m_Entity && *m_Entity == entity.GetID() && m_Path == path;
}

uint32_t PropertyEdits::InteractionFor(Entity entity, const Reflection::PropertyPath& path)
{
	return IsOn(entity, path) ? m_Interaction : NewInteraction();
}
