#pragma once

#include "History/SceneCommand.h"
#include "Reflection/Reflection.h"
#include "Scene/Entity.h"
#include "Core/UUID.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class SceneHistory;

namespace HistoryCommands
{
	// The property's value, or a custom field's saved XML; nullopt if the entity doesn't have it
	std::optional<Reflection::Value> ReadProperty(Entity entity, const Reflection::PropertyPath& path);
	// Sets the property as undo does, through Reflection::Set; a custom field loads its saved XML and runs the hooks
	bool WriteProperty(Entity entity, const Reflection::PropertyPath& path, const Reflection::Value& value);

	// Adds the property to the interaction's step, opening it if needed, with each entity's value now; call before the change.
	// A field with an edit hook or EditFromStart adds every saved field of its component, which its change can reach
	void RecordProperty(SceneHistory& history, uint32_t interaction, const std::vector<Entity>& entities,
		const Reflection::PropertyPath& path);
	// Adds every saved field of the component, for a change made to it directly
	void RecordComponent(SceneHistory& history, uint32_t interaction, const std::vector<Entity>& entities,
		const Reflection::ComponentType& type);

	// The Hierarchy's eye button: sets Hidden, adding the component if it's missing, as one step
	void SetHidden(SceneHistory& history, Entity entity, bool hidden);
}

// One property's value on each entity before and after a change, put back through Reflection::Set
class SetPropertyCommand : public SceneCommand
{
public:
	// Construct before the change; entities without the property are left out
	SetPropertyCommand(const std::vector<Entity>& entities, const Reflection::PropertyPath& path);
	bool End(Scene& scene) override;
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override;
	std::vector<Uuid> GetSelectionAfterUndo() const override { return m_Entities; }
	std::vector<Uuid> GetSelectionAfterRedo() const override { return m_Entities; }

	const Reflection::PropertyPath& GetPath() const { return m_Path; }
	const std::vector<Uuid>& GetEntities() const { return m_Entities; }
	bool Covers(const Uuid& entity) const;
	// One per entity, in the same order
	const std::vector<Reflection::Value>& GetBefore() const { return m_Before; }
	const std::vector<Reflection::Value>& GetAfter() const { return m_After; }

private:
	void Apply(Scene& scene, const std::vector<Reflection::Value>& values);

	Reflection::PropertyPath m_Path;
	std::vector<Uuid> m_Entities;
	std::vector<Reflection::Value> m_Before;
	std::vector<Reflection::Value> m_After;
};

// The properties one interaction changed, as one step; undo and redo set them in field order, as Restore does
class EditPropertiesCommand : public SceneCommand
{
public:
	// Adds the property for the entities it doesn't hold yet, capturing their values now
	void Add(const std::vector<Entity>& entities, const Reflection::PropertyPath& path);
	// Drops the components none of whose properties changed; false if none did
	bool End(Scene& scene) override;
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override;
	std::vector<Uuid> GetSelectionAfterUndo() const override;
	std::vector<Uuid> GetSelectionAfterRedo() const override;

	const std::vector<Scope<SetPropertyCommand>>& GetCommands() const { return m_Commands; }

private:
	// Sorted by component, then field
	std::vector<Scope<SetPropertyCommand>> m_Commands;
};

// Adds or removes a component on entities, keeping its saved values so either can be put back
class AddRemoveComponentCommand : public SceneCommand
{
public:
	enum class Action { Add, Remove };

	// Construct before the component is added or removed; entities it doesn't apply to are left out
	AddRemoveComponentCommand(const std::vector<Entity>& entities, const Reflection::ComponentType& type, Action action);
	// An add captures the values the component was given
	bool End(Scene& scene) override;
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override;
	std::vector<Uuid> GetSelectionAfterUndo() const override { return m_Entities; }
	std::vector<Uuid> GetSelectionAfterRedo() const override { return m_Entities; }

private:
	void Add(Scene& scene);
	void Remove(Scene& scene);

	const Reflection::ComponentType* m_Type;
	Action m_Action;
	std::vector<Uuid> m_Entities;
	// One per entity
	std::vector<Reflection::ComponentValues> m_Values;
};

// A scene setting from the inspector with nothing selected, before and after a change
class SetSceneSettingCommand : public SceneCommand
{
public:
	enum class Setting { Gravity, PixelsPerUnit };

	// Construct before the change
	SetSceneSettingCommand(Scene& scene, Setting setting);
	bool End(Scene& scene) override;
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override;
	size_t GetSize() const override { return sizeof(*this); }

	static EditKey KeyOf(Setting setting);
	static Reflection::Value Get(Scene& scene, Setting setting);
	static void Set(Scene& scene, Setting setting, const Reflection::Value& value);

private:
	Setting m_Setting;
	Reflection::Value m_Before;
	Reflection::Value m_After;
};

// The cells a paint stroke changed on a tilemap, each with its old and new tile
class SetTilesCommand : public SceneCommand
{
public:
	struct Cell
	{
		// Row times the map's width, plus the column
		uint32_t index;
		uint32_t oldTile;
		uint32_t newTile;
	};

	// Construct as the stroke starts; the whole grid is held until End keeps only the changed cells
	explicit SetTilesCommand(Entity tilemap);
	bool End(Scene& scene) override;
	void Undo(Scene& scene) override;
	void Redo(Scene& scene) override;
	std::string GetName() const override { return "Paint Tiles"; }
	size_t GetSize() const override;
	std::vector<Uuid> GetSelectionAfterUndo() const override { return { m_Entity }; }
	std::vector<Uuid> GetSelectionAfterRedo() const override { return { m_Entity }; }

	const std::vector<Cell>& GetCells() const { return m_Cells; }
	static EditKey KeyOf(Entity tilemap);

private:
	void Apply(Scene& scene, bool newTiles);

	Uuid m_Entity;
	uint32_t m_Width = 0;
	std::vector<std::vector<uint32_t>> m_Start;
	std::vector<Cell> m_Cells;
};
