#include "PropertyEdits.h"

#include "History/HistoryCommands.h"
#include "History/SceneHistory.h"
#include "Scene/Scene.h"

void PropertyEdits::Begin(Entity entity, const Reflection::PropertyPath& path)
{
	End();
	m_Interaction = m_NextInteraction++;
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
	const uint32_t interaction = inInteraction ? m_Interaction : m_NextInteraction++;

	RecordComponentEdit(entity, *path.component);

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

void PropertyEdits::RecordComponentEdit(Entity entity, const Reflection::ComponentType& type)
{
	if (!m_History || !m_History->IsEnabled())
		return;
	const EditKey key = ComponentEditKey(entity, type);
	if (!m_History->IsOpen(key))
		m_History->Begin(*entity.GetScene(), key, CreateScope<EditComponentCommand>(entity, type));
}

bool PropertyEdits::IsOn(Entity entity, const Reflection::PropertyPath& path) const
{
	return m_Interaction != 0 && m_Entity && *m_Entity == entity.GetID() && m_Path == path;
}
