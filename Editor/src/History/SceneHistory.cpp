#include "SceneHistory.h"

#include "Scene/Scene.h"

SceneHistory::SceneHistory(size_t budget)
	: m_Budget(budget)
{
}

void SceneHistory::Begin(Scene& scene, const EditKey& key, Scope<SceneCommand> command)
{
	if (!m_Enabled || !command || IsOpen(key))
		return;

	End(scene);
	m_OpenKey = key;
	m_OpenCommand = std::move(command);
}

bool SceneHistory::IsOpen(const EditKey& key) const
{
	return m_OpenCommand && m_OpenKey == key;
}

void SceneHistory::End(Scene& scene)
{
	if (!m_OpenCommand)
		return;

	Scope<SceneCommand> command = std::move(m_OpenCommand);
	m_OpenKey = EditKey();
	if (command->End(scene))
		Add(scene, std::move(command));
	else
		UpdateDirty(scene);
}

void SceneHistory::Push(Scene& scene, Scope<SceneCommand> command)
{
	if (!m_Enabled || !command)
		return;

	End(scene);
	if (command->End(scene))
		Add(scene, std::move(command));
}

void SceneHistory::Undo(Scene& scene)
{
	if (!m_Enabled)
		return;

	End(scene);
	if (m_Position == 0)
		return;

	m_Steps[--m_Position].command->Undo(scene);
	UpdateDirty(scene);
}

void SceneHistory::Redo(Scene& scene)
{
	if (!m_Enabled)
		return;

	End(scene);
	if (m_Position == m_Steps.size())
		return;

	m_Steps[m_Position++].command->Redo(scene);
	UpdateDirty(scene);
}

bool SceneHistory::CanUndo() const
{
	return m_Enabled && (m_Position > 0 || m_OpenCommand);
}

bool SceneHistory::CanRedo() const
{
	return m_Enabled && m_Position < m_Steps.size();
}

void SceneHistory::MarkSaved()
{
	m_SavedId = CurrentId();
	m_UnrecordedChange = false;
}

void SceneHistory::MarkUnrecordedChange()
{
	m_UnrecordedChange = true;
}

bool SceneHistory::IsAtSavedPoint() const
{
	return !m_UnrecordedChange && CurrentId() == m_SavedId;
}

void SceneHistory::SetEnabled(bool enabled)
{
	m_Enabled = enabled;
	if (!enabled)
	{
		m_OpenCommand.reset();
		m_OpenKey = EditKey();
	}
}

void SceneHistory::Clear()
{
	m_Steps.clear();
	m_Position = 0;
	m_Size = 0;
	m_BaseId = 0;
	m_SavedId = 0;
	m_UnrecordedChange = false;
	m_OpenCommand.reset();
	m_OpenKey = EditKey();
}

void SceneHistory::Add(Scene& scene, Scope<SceneCommand> command)
{
	// A new step drops the redo tail
	for (size_t i = m_Position; i < m_Steps.size(); ++i)
		m_Size -= m_Steps[i].size;
	m_Steps.erase(m_Steps.begin() + (std::ptrdiff_t)m_Position, m_Steps.end());

	Step step;
	step.size = command->GetSize();
	step.id = m_NextId++;
	step.command = std::move(command);
	m_Size += step.size;
	m_Steps.push_back(std::move(step));
	m_Position = m_Steps.size();

	// Over budget drops the oldest steps, always keeping the newest
	size_t dropped = 0;
	while (m_Size > m_Budget && m_Steps.size() - dropped > 1)
	{
		m_Size -= m_Steps[dropped].size;
		m_BaseId = m_Steps[dropped].id;
		++dropped;
	}
	if (dropped > 0)
	{
		m_Steps.erase(m_Steps.begin(), m_Steps.begin() + (std::ptrdiff_t)dropped);
		m_Position -= dropped;
	}

	UpdateDirty(scene);
}

uint64_t SceneHistory::CurrentId() const
{
	return m_Position > 0 ? m_Steps[m_Position - 1].id : m_BaseId;
}

void SceneHistory::UpdateDirty(Scene& scene) const
{
	if (IsAtSavedPoint())
		scene.MakeClean();
	else
		scene.MakeDirty();
}
