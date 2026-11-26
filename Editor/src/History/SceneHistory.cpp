#include "SceneHistory.h"

#include "History/HistoryCommands.h"
#include "Scene/Scene.h"

SceneHistory::SceneHistory(size_t budget)
	: m_Budget(budget)
{
}

SceneHistory::~SceneHistory() = default;

void SceneHistory::Begin(Scene& scene, const EditKey& key, Scope<SceneCommand> command)
{
	if (!m_Enabled || !command || IsOpen(key))
		return;

	End(scene);
	m_OpenKey = key;
	m_OpenCommand = std::move(command);
	m_OpenBefore = CaptureSelection();
}

bool SceneHistory::IsOpen(const EditKey& key) const
{
	return m_OpenCommand && m_OpenKey == key;
}

SceneCommand* SceneHistory::GetOpenCommand(const EditKey& key) const
{
	return IsOpen(key) ? m_OpenCommand.get() : nullptr;
}

void SceneHistory::End(Scene& scene)
{
	if (!m_OpenCommand)
		return;

	Scope<SceneCommand> command = std::move(m_OpenCommand);
	m_OpenKey = EditKey();
	if (command->End(scene))
		Record(scene, std::move(command), std::move(m_OpenBefore));
	else
		UpdateDirty(scene);
}

void SceneHistory::Push(Scene& scene, Scope<SceneCommand> command, const SelectionState* before)
{
	if (!m_Enabled || !command)
		return;

	// Taken before End, which can't change the selection
	SelectionState beforeState = before ? *before : CaptureSelection();
	End(scene);
	if (command->End(scene))
		Record(scene, std::move(command), std::move(beforeState));
}

void SceneHistory::Undo(Scene& scene)
{
	if (!m_Enabled || IsGrouping())
		return;

	End(scene);
	if (m_Position == 0)
		return;

	Step& step = m_Steps[--m_Position];
	step.command->Undo(scene);
	if (m_Selection)
		m_Selection->Restore(scene, step.before);
	UpdateDirty(scene);
}

void SceneHistory::Redo(Scene& scene)
{
	if (!m_Enabled || IsGrouping())
		return;

	End(scene);
	if (m_Position == m_Steps.size())
		return;

	Step& step = m_Steps[m_Position++];
	step.command->Redo(scene);
	if (m_Selection)
		m_Selection->Restore(scene, step.after);
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
		m_Group.reset();
		m_GroupDepth = 0;
	}
}

void SceneHistory::Clear()
{
	m_Steps.clear();
	m_Position = 0;
	m_Size = 0;
	// A fresh id, so the cleared state never matches an earlier one
	m_BaseId = m_NextId++;
	m_SavedId = m_BaseId;
	m_UnrecordedChange = false;
	m_OpenCommand.reset();
	m_OpenKey = EditKey();
	m_Group.reset();
	m_GroupDepth = 0;
}

void SceneHistory::BeginGroup(Scene& scene, const std::string& name)
{
	if (!m_Enabled)
		return;
	if (m_GroupDepth++ > 0)
		return;

	// A step opened before the group isn't part of it
	End(scene);
	m_Group = CreateScope<CompoundCommand>(name);
	m_GroupBefore = CaptureSelection();
}

void SceneHistory::EndGroup(Scene& scene)
{
	if (m_GroupDepth == 0 || --m_GroupDepth > 0)
		return;

	End(scene);
	Scope<CompoundCommand> group = std::move(m_Group);
	if (group && !group->IsEmpty())
		Add(scene, std::move(group), std::move(m_GroupBefore));
	else
		UpdateDirty(scene);
}

void SceneHistory::Record(Scene& scene, Scope<SceneCommand> command, SelectionState before)
{
	if (m_Group)
		m_Group->Append(std::move(command));
	else
		Add(scene, std::move(command), std::move(before));
}

SelectionState SceneHistory::CaptureSelection() const
{
	return m_Selection ? m_Selection->Capture() : SelectionState();
}

void SceneHistory::Add(Scene& scene, Scope<SceneCommand> command, SelectionState before)
{
	// A new step drops the redo tail
	for (size_t i = m_Position; i < m_Steps.size(); ++i)
		m_Size -= m_Steps[i].size;
	m_Steps.erase(m_Steps.begin() + (std::ptrdiff_t)m_Position, m_Steps.end());

	Step step;
	step.id = m_NextId++;
	step.before = std::move(before);
	step.after = CaptureSelection();
	step.size = command->GetSize() + (step.before.ids.capacity() + step.after.ids.capacity()) * sizeof(Uuid);
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

HistoryGroup::HistoryGroup(SceneHistory& history, Scene& scene, const std::string& name)
	: m_History(history), m_Scene(scene)
{
	m_History.BeginGroup(m_Scene, name);
}

HistoryGroup::~HistoryGroup()
{
	m_History.EndGroup(m_Scene);
}
