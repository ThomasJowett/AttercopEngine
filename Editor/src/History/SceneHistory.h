#pragma once

#include "Core/core.h"
#include "History/SceneCommand.h"
#include "Selection/EntitySelection.h"

#include <cstdint>
#include <string>
#include <vector>

class CompoundCommand;

// The open scene's undo and redo steps
class SceneHistory
{
public:
	static constexpr size_t DefaultBudget = 64 * 1024 * 1024;

	explicit SceneHistory(size_t budget = DefaultBudget);
	~SceneHistory();

	// The selection each step stores and undo and redo restore; none leaves the selection alone
	void SetSelection(EntitySelection* selection) { m_Selection = selection; }

	// Opens a step whose command has captured the "before" state; closes any open step with another key
	void Begin(Scene& scene, const EditKey& key, Scope<SceneCommand> command);
	bool IsOpen(const EditKey& key) const;
	bool HasOpenStep() const { return m_OpenCommand != nullptr; }
	// The open step's command if its key is this one, otherwise nullptr
	SceneCommand* GetOpenCommand(const EditKey& key) const;
	// Captures the open step's "after" state and pushes it, unless nothing changed
	void End(Scene& scene);
	// A change made in one go; its "before" selection is the one given, or the current one if none
	void Push(Scene& scene, Scope<SceneCommand> command, const SelectionState* before = nullptr);

	// Restore the selection stored when the step opened (undo) or closed (redo), skipping entities that are gone
	void Undo(Scene& scene);
	void Redo(Scene& scene);
	bool CanUndo() const;
	bool CanRedo() const;

	// The scene matches its file at the current position
	void MarkSaved();
	// A change that has no step, so the scene stays unsaved until the next save
	void MarkUnrecordedChange();
	bool IsAtSavedPoint() const;

	// Off outside Edit mode: nothing is recorded, and undo and redo do nothing
	void SetEnabled(bool enabled);
	bool IsEnabled() const { return m_Enabled; }

	void Clear();

	// Everything pushed or ended until the matching EndGroup is one step; nested groups join the outer one
	void BeginGroup(Scene& scene, const std::string& name);
	void EndGroup(Scene& scene);
	bool IsGrouping() const { return m_GroupDepth > 0; }

	size_t GetStepCount() const { return m_Steps.size(); }
	// Steps that are done, the rest can be redone
	size_t GetPosition() const { return m_Position; }
	// Bytes held by all steps
	size_t GetSize() const { return m_Size; }
	void SetBudget(size_t budget) { m_Budget = budget; }

private:
	struct Step
	{
		Scope<SceneCommand> command;
		SelectionState before;
		SelectionState after;
		uint64_t id = 0;
		size_t size = 0;
	};

	// Adds a finished command to the open group, or as a new step
	void Record(Scene& scene, Scope<SceneCommand> command, SelectionState before);
	void Add(Scene& scene, Scope<SceneCommand> command, SelectionState before);
	SelectionState CaptureSelection() const;
	uint64_t CurrentId() const;
	void UpdateDirty(Scene& scene) const;

	std::vector<Step> m_Steps;
	size_t m_Position = 0;
	size_t m_Size = 0;
	size_t m_Budget;

	uint64_t m_NextId = 1;
	// The id of the state before the first kept step
	uint64_t m_BaseId = 0;
	uint64_t m_SavedId = 0;
	bool m_UnrecordedChange = false;

	EntitySelection* m_Selection = nullptr;

	EditKey m_OpenKey;
	Scope<SceneCommand> m_OpenCommand;
	SelectionState m_OpenBefore;

	Scope<CompoundCommand> m_Group;
	SelectionState m_GroupBefore;
	int m_GroupDepth = 0;

	bool m_Enabled = true;
};

// One undo step for everything recorded while it exists
class HistoryGroup
{
public:
	HistoryGroup(SceneHistory& history, Scene& scene, const std::string& name);
	~HistoryGroup();
	HistoryGroup(const HistoryGroup&) = delete;
	HistoryGroup& operator=(const HistoryGroup&) = delete;
private:
	SceneHistory& m_History;
	Scene& m_Scene;
};
