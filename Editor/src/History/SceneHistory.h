#pragma once

#include "Core/core.h"
#include "History/SceneCommand.h"

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

	// Opens a step whose command has captured the "before" state; closes any open step with another key
	void Begin(Scene& scene, const EditKey& key, Scope<SceneCommand> command);
	bool IsOpen(const EditKey& key) const;
	bool HasOpenStep() const { return m_OpenCommand != nullptr; }
	// Captures the open step's "after" state and pushes it, unless nothing changed
	void End(Scene& scene);
	// A change made in one go
	void Push(Scene& scene, Scope<SceneCommand> command);

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
		uint64_t id = 0;
		size_t size = 0;
	};

	// Adds a finished command to the open group, or as a new step
	void Record(Scene& scene, Scope<SceneCommand> command);
	void Add(Scene& scene, Scope<SceneCommand> command);
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

	EditKey m_OpenKey;
	Scope<SceneCommand> m_OpenCommand;

	Scope<CompoundCommand> m_Group;
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
