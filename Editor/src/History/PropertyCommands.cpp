#include "PropertyCommands.h"

#include "History/HistoryCommands.h"
#include "History/SceneHistory.h"
#include "Inspector/PropertyEdits.h"
#include "Reflection/ComponentType.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Logging/Logger.h"

#include "TinyXml2/tinyxml2.h"

#include <algorithm>
#include <tuple>

namespace HistoryCommands
{
	static void* TryGet(Entity entity, const Reflection::ComponentType& type)
	{
		return type.tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle());
	}

	std::optional<Reflection::Value> ReadProperty(Entity entity, const Reflection::PropertyPath& path)
	{
		const Reflection::FieldInfo& field = path.Field();
		if (!field.IsCustom())
			return Reflection::Get(entity, path);

		const void* component = TryGet(entity, *path.component);
		if (!component)
			return std::nullopt;
		tinyxml2::XMLDocument document;
		tinyxml2::XMLElement* element = document.NewElement(path.component->name.c_str());
		document.InsertEndChild(element);
		field.save(component, element);
		tinyxml2::XMLPrinter printer(nullptr, true);
		document.Print(&printer);
		return Reflection::Value(std::string(printer.CStr()));
	}

	bool WriteProperty(Entity entity, const Reflection::PropertyPath& path, const Reflection::Value& value)
	{
		const Reflection::FieldInfo& field = path.Field();
		if (!field.IsCustom())
			return Reflection::Set(entity, path, value, Reflection::ChangeSource::Undo);

		void* component = TryGet(entity, *path.component);
		const std::string* text = value.TryAs<std::string>();
		if (!component || !text)
			return false;
		if (ReadProperty(entity, path) == value)
			return true;

		tinyxml2::XMLDocument document;
		if (document.Parse(text->c_str()) != tinyxml2::XML_SUCCESS || !document.RootElement())
		{
			CLIENT_WARN("Undo can't read {0}: {1}", path.ToString(), document.ErrorStr());
			return false;
		}
		field.load(component, document.RootElement());
		field.onChanged(entity, component);
		path.component->onChanged(entity, component);
		if (!entity.GetScene()->IsRunning())
			entity.GetScene()->MakeDirty();
		return true;
	}

	static EditPropertiesCommand* OpenStep(SceneHistory& history, uint32_t interaction, const std::vector<Entity>& entities)
	{
		if (entities.empty() || !history.IsEnabled())
			return nullptr;
		ASSERT(interaction != 0, "Recording a property change with no interaction");
		const EditKey key = EditKey::ForInteraction(interaction);
		Entity first = entities.front();
		if (!history.IsOpen(key))
			history.Begin(*first.GetScene(), key, CreateScope<EditPropertiesCommand>());
		return dynamic_cast<EditPropertiesCommand*>(history.GetOpenCommand(key));
	}

	static void AddSavedFields(EditPropertiesCommand& step, const std::vector<Entity>& entities, const Reflection::ComponentType& type)
	{
		for (int i = 0; i < (int)type.fields.size(); ++i)
		{
			const Reflection::FieldInfo& field = type.fields[i];
			if (field.IsSaved() && !field.Has(Reflection::FieldFlags::ReadOnly))
				step.Add(entities, Reflection::PropertyPath{ &type, i });
		}
	}

	void RecordProperty(SceneHistory& history, uint32_t interaction, const std::vector<Entity>& entities, const Reflection::PropertyPath& path)
	{
		EditPropertiesCommand* step = OpenStep(history, interaction, entities);
		if (!step)
			return;
		const Reflection::FieldInfo& field = path.Field();
		if (field.edit || field.Has(Reflection::FieldFlags::EditFromStart))
			AddSavedFields(*step, entities, *path.component);
		step->Add(entities, path);
	}

	void RecordComponent(SceneHistory& history, uint32_t interaction, const std::vector<Entity>& entities, const Reflection::ComponentType& type)
	{
		if (EditPropertiesCommand* step = OpenStep(history, interaction, entities))
			AddSavedFields(*step, entities, type);
	}

	void SetHidden(SceneHistory& history, Entity entity, bool hidden)
	{
		SetHidden(history, std::vector<Entity>{ entity }, hidden);
	}

	void SetHidden(SceneHistory& history, const std::vector<Entity>& entities, bool hidden)
	{
		if (entities.empty())
			return;

		Entity first = entities.front();
		Scene& scene = *first.GetScene();
		const Reflection::ComponentType& type = Reflection::TypeOf<HiddenComponent>();
		const Reflection::PropertyPath path{ &type, type.FindField("Hidden") };
		const std::string name = entities.size() == 1 ? first.GetName() : std::to_string(entities.size()) + " Entities";
		HistoryGroup group(history, scene, (hidden ? "Hide " : "Show ") + name);

		std::vector<Entity> missing;
		for (Entity entity : entities)
		{
			if (!entity.HasComponent<HiddenComponent>())
				missing.push_back(entity);
		}
		if (!missing.empty())
		{
			Scope<AddRemoveComponentCommand> add = CreateScope<AddRemoveComponentCommand>(missing, type,
				AddRemoveComponentCommand::Action::Add);
			for (Entity entity : missing)
				entity.AddComponent<HiddenComponent>();
			history.Push(scene, std::move(add));
		}
		RecordProperty(history, PropertyEdits::NewInteraction(), entities, path);
		for (Entity entity : entities)
			Reflection::Set(entity, path, hidden, Reflection::ChangeSource::Editor);
	}
}

SetPropertyCommand::SetPropertyCommand(const std::vector<Entity>& entities, const Reflection::PropertyPath& path)
	: m_Path(path)
{
	for (Entity entity : entities)
	{
		if (std::optional<Reflection::Value> value = HistoryCommands::ReadProperty(entity, path))
		{
			m_Entities.push_back(entity.GetID());
			m_Before.push_back(std::move(*value));
		}
	}
}

bool SetPropertyCommand::End(Scene& scene)
{
	m_After = m_Before;
	bool changed = false;
	for (size_t i = 0; i < m_Entities.size(); ++i)
	{
		Entity entity = HistoryCommands::Find(scene, m_Entities[i]);
		if (!entity)
			continue;
		if (std::optional<Reflection::Value> value = HistoryCommands::ReadProperty(entity, m_Path))
		{
			changed |= *value != m_Before[i];
			m_After[i] = std::move(*value);
		}
	}
	return changed;
}

void SetPropertyCommand::Undo(Scene& scene)
{
	Apply(scene, m_Before);
}

void SetPropertyCommand::Redo(Scene& scene)
{
	Apply(scene, m_After);
}

void SetPropertyCommand::Apply(Scene& scene, const std::vector<Reflection::Value>& values)
{
	for (size_t i = 0; i < m_Entities.size(); ++i)
	{
		if (Entity entity = HistoryCommands::Find(scene, m_Entities[i]))
			HistoryCommands::WriteProperty(entity, m_Path, values[i]);
	}
}

bool SetPropertyCommand::Covers(const Uuid& entity) const
{
	return std::find(m_Entities.begin(), m_Entities.end(), entity) != m_Entities.end();
}

std::string SetPropertyCommand::GetName() const
{
	const std::string component = Reflection::SplitWords(m_Path.component->name);
	const std::string& label = m_Path.Field().label;
	return label == component ? "Edit " + label : "Edit " + component + " " + label;
}

size_t SetPropertyCommand::GetSize() const
{
	size_t size = sizeof(*this) + m_Entities.capacity() * sizeof(Uuid);
	for (const std::vector<Reflection::Value>* values : { &m_Before, &m_After })
	{
		size += (values->capacity() - values->size()) * sizeof(Reflection::Value);
		for (const Reflection::Value& value : *values)
			size += HistoryCommands::SizeOf(value);
	}
	return size;
}

static auto PropertyOrder(const Reflection::PropertyPath& path)
{
	return std::make_tuple(path.component, path.field, path.index, path.part);
}

void EditPropertiesCommand::Add(const std::vector<Entity>& entities, const Reflection::PropertyPath& path)
{
	std::vector<Entity> added;
	for (Entity entity : entities)
	{
		const bool held = std::any_of(m_Commands.begin(), m_Commands.end(), [&](const Scope<SetPropertyCommand>& command)
			{
				return command->GetPath() == path && command->Covers(entity.GetID());
			});
		if (!held)
			added.push_back(entity);
	}
	if (added.empty())
		return;

	Scope<SetPropertyCommand> command = CreateScope<SetPropertyCommand>(added, path);
	if (command->GetEntities().empty())
		return;
	auto position = std::upper_bound(m_Commands.begin(), m_Commands.end(), PropertyOrder(path),
		[](const auto& order, const Scope<SetPropertyCommand>& other) { return order < PropertyOrder(other->GetPath()); });
	m_Commands.insert(position, std::move(command));
}

bool EditPropertiesCommand::End(Scene& scene)
{
	// A component with any change keeps all its properties, since a setter can move another field, as Size moves the margins
	std::vector<const Reflection::ComponentType*> changed;
	for (Scope<SetPropertyCommand>& command : m_Commands)
	{
		if (command->End(scene))
			changed.push_back(command->GetPath().component);
	}
	m_Commands.erase(std::remove_if(m_Commands.begin(), m_Commands.end(), [&changed](const Scope<SetPropertyCommand>& command)
		{
			return std::find(changed.begin(), changed.end(), command->GetPath().component) == changed.end();
		}), m_Commands.end());
	return !m_Commands.empty();
}

void EditPropertiesCommand::Undo(Scene& scene)
{
	for (Scope<SetPropertyCommand>& command : m_Commands)
		command->Undo(scene);
}

void EditPropertiesCommand::Redo(Scene& scene)
{
	for (Scope<SetPropertyCommand>& command : m_Commands)
		command->Redo(scene);
}

std::string EditPropertiesCommand::GetName() const
{
	if (m_Commands.size() == 1)
		return m_Commands.front()->GetName();
	const Reflection::ComponentType* component = m_Commands.empty() ? nullptr : m_Commands.front()->GetPath().component;
	for (const Scope<SetPropertyCommand>& command : m_Commands)
	{
		if (command->GetPath().component != component)
			return "Edit Properties";
	}
	return component ? "Edit " + Reflection::SplitWords(component->name) : "Edit Properties";
}

size_t EditPropertiesCommand::GetSize() const
{
	size_t size = sizeof(*this) + m_Commands.capacity() * sizeof(Scope<SetPropertyCommand>);
	for (const Scope<SetPropertyCommand>& command : m_Commands)
		size += command->GetSize();
	return size;
}

AddRemoveComponentCommand::AddRemoveComponentCommand(const std::vector<Entity>& entities, const Reflection::ComponentType& type, Action action)
	: m_Type(&type), m_Action(action)
{
	for (Entity entity : entities)
	{
		const bool has = HistoryCommands::TryGet(entity, type) != nullptr;
		if (has != (action == Action::Remove))
			continue;
		m_Entities.push_back(entity.GetID());
		m_Values.push_back(action == Action::Remove ? Reflection::Capture(entity, type) : Reflection::ComponentValues{ &type, {}, {} });
	}
}

bool AddRemoveComponentCommand::End(Scene& scene)
{
	for (size_t i = 0; i < m_Entities.size();)
	{
		Entity entity = HistoryCommands::Find(scene, m_Entities[i]);
		const bool has = entity && HistoryCommands::TryGet(entity, *m_Type);
		// Entities whose component wasn't added or removed after all
		if (!entity || has != (m_Action == Action::Add))
		{
			m_Entities.erase(m_Entities.begin() + (std::ptrdiff_t)i);
			m_Values.erase(m_Values.begin() + (std::ptrdiff_t)i);
			continue;
		}
		if (m_Action == Action::Add)
			m_Values[i] = Reflection::Capture(entity, *m_Type);
		++i;
	}
	return !m_Entities.empty();
}

void AddRemoveComponentCommand::Undo(Scene& scene)
{
	if (m_Action == Action::Add)
		Remove(scene);
	else
		Add(scene);
}

void AddRemoveComponentCommand::Redo(Scene& scene)
{
	if (m_Action == Action::Add)
		Add(scene);
	else
		Remove(scene);
}

void AddRemoveComponentCommand::Add(Scene& scene)
{
	for (size_t i = 0; i < m_Entities.size(); ++i)
	{
		if (Entity entity = HistoryCommands::Find(scene, m_Entities[i]))
			Reflection::Restore(entity, m_Values[i], Reflection::ChangeSource::Undo);
	}
}

void AddRemoveComponentCommand::Remove(Scene& scene)
{
	for (const Uuid& id : m_Entities)
	{
		if (Entity entity = HistoryCommands::Find(scene, id))
			m_Type->remove(scene.GetRegistry(), entity.GetHandle());
	}
}

std::string AddRemoveComponentCommand::GetName() const
{
	return (m_Action == Action::Add ? "Add " : "Remove ") + Reflection::SplitWords(m_Type->name);
}

size_t AddRemoveComponentCommand::GetSize() const
{
	size_t size = sizeof(*this) + m_Entities.capacity() * sizeof(Uuid);
	for (const Reflection::ComponentValues& values : m_Values)
		size += HistoryCommands::SizeOf(values);
	return size;
}

SetSceneSettingCommand::SetSceneSettingCommand(Scene& scene, Setting setting)
	: m_Setting(setting), m_Before(Get(scene, setting))
{
}

bool SetSceneSettingCommand::End(Scene& scene)
{
	m_After = Get(scene, m_Setting);
	return m_After != m_Before;
}

void SetSceneSettingCommand::Undo(Scene& scene)
{
	Set(scene, m_Setting, m_Before);
}

void SetSceneSettingCommand::Redo(Scene& scene)
{
	Set(scene, m_Setting, m_After);
}

std::string SetSceneSettingCommand::GetName() const
{
	return m_Setting == Setting::Gravity ? "Edit Gravity" : "Edit Pixels Per Unit";
}

EditKey SetSceneSettingCommand::KeyOf(Setting setting)
{
	return EditKey({}, nullptr, setting == Setting::Gravity ? "Gravity" : "PixelsPerUnit");
}

Reflection::Value SetSceneSettingCommand::Get(Scene& scene, Setting setting)
{
	if (setting == Setting::Gravity)
		return Reflection::Value(scene.GetGravity());
	return Reflection::Value(scene.GetPixelsPerUnit());
}

void SetSceneSettingCommand::Set(Scene& scene, Setting setting, const Reflection::Value& value)
{
	if (setting == Setting::Gravity)
		scene.SetGravity(value.As<Vector2f>());
	else
		scene.SetPixelsPerUnit(value.As<uint32_t>());
}

SetTilesCommand::SetTilesCommand(Entity tilemap)
	: m_Entity(tilemap.GetID())
{
	const TilemapComponent& component = tilemap.GetComponent<TilemapComponent>();
	m_Width = component.tilesWide;
	m_Start = component.tiles;
}

bool SetTilesCommand::End(Scene& scene)
{
	// Already ended
	if (m_Start.empty())
		return !m_Cells.empty();

	Entity entity = HistoryCommands::Find(scene, m_Entity);
	if (TilemapComponent* tilemap = entity ? entity.TryGetComponent<TilemapComponent>() : nullptr)
	{
		const std::vector<std::vector<uint32_t>>& tiles = tilemap->tiles;
		for (size_t y = 0; y < m_Start.size() && y < tiles.size(); ++y)
		{
			for (size_t x = 0; x < m_Start[y].size() && x < tiles[y].size() && x < m_Width; ++x)
			{
				if (tiles[y][x] != m_Start[y][x])
					m_Cells.push_back({ (uint32_t)(y * m_Width + x), m_Start[y][x], tiles[y][x] });
			}
		}
	}
	m_Start.clear();
	m_Start.shrink_to_fit();
	m_Cells.shrink_to_fit();
	return !m_Cells.empty();
}

void SetTilesCommand::Undo(Scene& scene)
{
	Apply(scene, false);
}

void SetTilesCommand::Redo(Scene& scene)
{
	Apply(scene, true);
}

void SetTilesCommand::Apply(Scene& scene, bool newTiles)
{
	Entity entity = HistoryCommands::Find(scene, m_Entity);
	TilemapComponent* tilemap = entity ? entity.TryGetComponent<TilemapComponent>() : nullptr;
	if (!tilemap || m_Width == 0)
		return;

	for (const Cell& cell : m_Cells)
	{
		const size_t y = cell.index / m_Width;
		const size_t x = cell.index % m_Width;
		if (y < tilemap->tiles.size() && x < tilemap->tiles[y].size())
			tilemap->tiles[y][x] = newTiles ? cell.newTile : cell.oldTile;
	}

	const Reflection::ComponentType& type = Reflection::TypeOf<TilemapComponent>();
	const int tiles = type.FindField("Tiles");
	if (tiles >= 0)
		type.fields[tiles].onChanged(entity, tilemap);
	type.onChanged(entity, tilemap);
}

size_t SetTilesCommand::GetSize() const
{
	return sizeof(*this) + m_Cells.capacity() * sizeof(Cell);
}

EditKey SetTilesCommand::KeyOf(Entity tilemap)
{
	return EditKey({ tilemap.GetID() }, &Reflection::TypeOf<TilemapComponent>(), "Tiles");
}
