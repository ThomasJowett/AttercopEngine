#include "ComponentInspector.h"

#include "History/PropertyCommands.h"
#include "History/SceneHistory.h"
#include "Inspector/InspectorWidgets.h"

#include "ImGui/ImGuiAssetEdit.h"
#include "ImGui/ImGuiTextureEdit.h"
#include "ImGui/ImGuiUtilities.h"
#include "ImGui/ImGuiVectorEdit.h"

#include "Asset/Font.h"
#include "Asset/Material.h"
#include "Asset/PhysicsMaterial.h"
#include "Core/Application.h"
#include "FileSystem/Directory.h"
#include "Physics/CollisionLayers.h"
#include "Scene/AssetManager.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Utilities/FileUtils.h"
#include "Viewers/ViewerManager.h"

#include "IconsFontAwesome6.h"
#include "IconsMaterialDesign.h"
#include "Fonts/IconsMaterialDesignIcons.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <climits>

using namespace Reflection;

namespace
{
	struct ComponentLook
	{
		const char* name;
		const char* title;
		bool shown = true;
	};

	// Editor-only, as the icon fonts are; a component missing here is shown with its name
	const ComponentLook s_Looks[] = {
		{ "Transform", ICON_MDI_AXIS_ARROW " Transform" },
		{ "Widget", ICON_MDI_WIDGETS " Widget" },
		{ "Sprite", ICON_FA_IMAGE " Sprite" },
		{ "AnimatedSprite", ICON_FA_FILM " Animated Sprite" },
		{ "CircleRenderer", ICON_FA_CIRCLE " Circle Renderer" },
		{ "Tilemap", ICON_FA_BORDER_ALL " Tilemap" },
		{ "StaticMesh", ICON_FA_SHAPES " Static Mesh" },
		{ "Camera", ICON_FA_VIDEO " Camera" },
		{ "Primitive", ICON_FA_CUBE " Primitive" },
		{ "Text", ICON_FA_FONT " Text" },
		{ "RigidBody2D", ICON_FA_BASEBALL " Rigid Body 2D" },
		{ "BoxCollider2D", ICON_FA_VECTOR_SQUARE " Box Collider 2D" },
		{ "CircleCollider2D", ICON_MDI_CIRCLE_OUTLINE " Circle Collider 2D" },
		{ "PolygonCollider2D", ICON_FA_DRAW_POLYGON " Polygon Collider 2D" },
		{ "CapsuleCollider2D", ICON_FA_CAPSULES " Capsule Collider 2D" },
		{ "WeldJoint2D", ICON_FA_LINK " Weld Joint 2D" },
		{ "PointLight", ICON_FA_LIGHTBULB " Point Light" },
		{ "BehaviourTree", ICON_FA_CODE_BRANCH " Behaviour Tree" },
		{ "StateMachine", ICON_FA_DIAGRAM_PROJECT " State Machine" },
		{ "Billboard", ICON_FA_SIGN_HANGING " Billboard" },
		{ "Canvas", ICON_FA_OBJECT_GROUP " Canvas" },
		{ "Button", ICON_FA_CIRCLE_DOT " Button" },
		{ "StackLayout", ICON_FA_BARS " Stack Layout" },
		{ "GridLayout", ICON_FA_TABLE_CELLS " Grid Layout" },
		{ "ScrollBox", ICON_FA_SCROLL " Scroll Box" },
		{ "AudioSource", ICON_FA_VOLUME_HIGH " Audio Source" },
		{ "AudioListener", ICON_FA_MICROPHONE " Audio Listener" },
		{ "LuaScript", ICON_FA_FILE_CODE " Lua Script" },
		// Set by the Hierarchy's eye button
		{ "Hidden", nullptr, false },
	};

	const ComponentLook* FindLook(const std::string& name)
	{
		for (const ComponentLook& look : s_Looks)
		{
			if (name == look.name)
				return &look;
		}
		return nullptr;
	}

	// The asset a kind falls back to when unset, shown as "Default"
	Ref<Asset> DefaultAsset(std::string_view type)
	{
		if (type == "Material")
			return Material::GetDefaultMaterial();
		if (type == "PhysicsMaterial")
			return PhysicsMaterial::GetDefaultPhysicsMaterial();
		if (type == "Font")
			return Font::GetDefaultFont();
		return nullptr;
	}

	constexpr float s_DegreesPerRadian = 57.2957795f;

	int ResizeString(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			std::string* text = (std::string*)data->UserData;
			text->resize(data->BufTextLen);
			data->Buf = text->data();
		}
		return 0;
	}

	// hint shows while the text is empty
	bool InputString(const char* label, std::string& text, bool multiline, const char* hint = nullptr)
	{
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize;
		if (multiline)
			return ImGui::InputTextMultiline(label, text.data(), text.capacity() + 1, ImVec2(0, 0),
				flags | ImGuiInputTextFlags_AllowTabInput, &ResizeString, &text);
		if (hint)
			return ImGui::InputTextWithHint(label, hint, text.data(), text.capacity() + 1, flags, &ResizeString, &text);
		return ImGui::InputText(label, text.data(), text.capacity() + 1, flags, &ResizeString, &text);
	}

	// A number's format, or a dash while it's mixed and not being edited
	const char* NumberFormat(const char* label, bool mixed, const char* format)
	{
		return mixed && ImGui::GetActiveID() != ImGui::GetID(label) ? ComponentInspector::MixedText : format;
	}

	// After a pause, so passing over fields doesn't flash them
	void DescriptionTooltip(const std::string& description)
	{
		if (!description.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s", description.c_str());
	}

	bool ResetClicked()
	{
		return ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
	}

	bool LayerEdit(const char* label, uint16_t& layer, bool mixed)
	{
		bool changed = false;
		if (ImGui::BeginCombo(label, mixed ? ComponentInspector::MixedText : CollisionLayers::GetLayerName(layer).c_str()))
		{
			for (int i = 0; i < CollisionLayers::MaxLayers; ++i)
			{
				if (!CollisionLayers::IsNamed(i))
					continue;
				const uint16_t bit = (uint16_t)(1 << i);
				if (ImGui::Selectable(CollisionLayers::GetDisplayName(i).c_str(), layer == bit))
				{
					layer = bit;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	bool MaskEdit(const char* label, uint16_t& mask, bool mixed)
	{
		uint16_t namedBits = 0;
		for (int i = 0; i < CollisionLayers::MaxLayers; ++i)
		{
			if (CollisionLayers::IsNamed(i))
				namedBits |= (uint16_t)(1 << i);
		}

		std::string preview;
		if (mixed)
			preview = ComponentInspector::MixedText;
		else if ((mask & namedBits) == namedBits)
			preview = "Everything";
		else if ((mask & namedBits) == 0)
			preview = "Nothing";
		else
		{
			for (const std::string& name : CollisionLayers::GetMaskNames(mask))
				preview += (preview.empty() ? "" : ", ") + name;
		}

		bool changed = false;
		if (ImGui::BeginCombo(label, preview.c_str()))
		{
			if (ImGui::Button("Everything"))
			{
				mask = CollisionLayers::Everything;
				changed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Nothing"))
			{
				mask = 0;
				changed = true;
			}
			for (int i = 0; i < CollisionLayers::MaxLayers; ++i)
			{
				if (!CollisionLayers::IsNamed(i))
					continue;
				unsigned int flags = mask;
				if (ImGui::CheckboxFlags(CollisionLayers::GetDisplayName(i).c_str(), &flags, 1u << i))
				{
					mask = (uint16_t)flags;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	// A picker for any asset kind, from the project's files of its type
	std::optional<AssetValue> AssetPicker(const char* label, const AssetValue& asset, const Inspector::AssetKind& kind, bool mixed)
	{
		std::optional<AssetValue> picked;
		const Ref<Asset> defaultAsset = DefaultAsset(kind.type);
		const bool isDefault = !asset.IsSet() || (defaultAsset && asset.path == defaultAsset->GetFilepath());

		ImGui::TextUnformatted(label);
		std::string preview = mixed ? ComponentInspector::MixedText
			: isDefault ? (defaultAsset ? "Default" : "None") : asset.path.filename().string();
		if (ImGui::BeginAssetCombo("##Asset", preview, mixed ? nullptr : asset.asset.get()))
		{
			for (std::filesystem::path& file : Directory::GetFilesRecursive(Application::GetOpenDocumentDirectory(), ViewerManager::GetExtensions(kind.fileType)))
			{
				std::filesystem::path relativePath = FileUtils::RelativePath(file, Application::GetOpenDocumentDirectory());
				ImGui::PushID(relativePath.string().c_str());
				bool clicked = ImGui::Selectable(file.filename().string().c_str(), relativePath == asset.path);
				ImGui::Tooltip(relativePath.string().c_str());
				ImGui::PopID();
				if (clicked)
					picked = AssetValue{ relativePath, nullptr };
			}
			ImGui::EndCombo();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_AcceptPeekOnly))
			{
				const std::filesystem::path* file = (const std::filesystem::path*)payload->Data;
				for (const std::string& extension : ViewerManager::GetExtensions(kind.fileType))
				{
					if (file->extension().string() == extension && ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_None))
						picked = AssetValue{ FileUtils::RelativePath(*file, Application::GetOpenDocumentDirectory()), nullptr };
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (!isDefault && !mixed)
		{
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_PEN_TO_SQUARE "##EditAsset"))
				ViewerManager::OpenViewer(asset.path);
			ImGui::Tooltip("Open Asset Viewer");
		}
		if (!isDefault || mixed)
		{
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_XMARK "##ClearAsset"))
				picked = AssetValue{};
			ImGui::Tooltip(defaultAsset ? "Use the default" : "Clear");
		}
		return picked;
	}
}


namespace
{
	// Whether a description's condition holds for every entity's component; one with no condition always does
	bool HoldsForAll(const ComponentInspector::Entities& entities, const ComponentType& type, const Predicate& predicate)
	{
		if (!predicate)
			return true;
		for (Entity entity : entities)
		{
			const void* component = type.tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle());
			if (!component || !predicate(component))
				return false;
		}
		return true;
	}
}

ComponentInspector::ComponentInspector(SceneHistory* history)
	: m_Edits(history), m_History(history)
{
}

void ComponentInspector::ReplaceComponent(std::string_view component, Support support, ComponentDrawer draw)
{
	m_ReplacedComponents[std::string(component)] = { support, std::move(draw) };
}

void ComponentInspector::AppendToComponent(std::string_view component, Support support, ComponentDrawer draw)
{
	m_Appended[std::string(component)] = { support, std::move(draw) };
}

void ComponentInspector::ReplaceField(std::string_view path, Support support, FieldDrawer draw)
{
	m_ReplacedFields[std::string(path)] = { support, std::move(draw) };
}

bool ComponentInspector::IsShown(const ComponentType& type)
{
	const ComponentLook* look = FindLook(type.name);
	return !type.IsInternal() && (!look || look->shown);
}

std::string ComponentInspector::Title(const ComponentType& type)
{
	const ComponentLook* look = FindLook(type.name);
	return look && look->title ? look->title : SplitWords(type.name);
}

void ComponentInspector::DrawSummary(const Entities& entities)
{
	if (entities.size() < 2)
		return;

	std::vector<const ComponentType*> hidden;
	for (const ComponentType* type : Split(entities).partial)
	{
		if (IsShown(*type))
			hidden.push_back(type);
	}

	ImGui::Text("%zu entities", entities.size());
	if (hidden.empty())
		return;
	ImGui::SameLine();
	ImGui::TextDisabled("%zu component%s hidden", hidden.size(), hidden.size() == 1 ? "" : "s");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("Not on every selected entity:");
		for (const ComponentType* type : hidden)
			ImGui::BulletText("%s", SplitWords(type->name).c_str());
		ImGui::EndTooltip();
	}
}

void ComponentInspector::Draw(const Entities& entities)
{
	for (const ComponentType* type : Split(entities).common)
	{
		if (IsShown(*type))
			DrawComponent(entities, *type);
	}

	// An interaction whose widget went away, as when a ShowIf stopped holding
	if (m_Edits.IsOpen() && !ImGui::IsAnyItemActive())
		m_Edits.End();
}

const MultiEdit::ComponentSplit& ComponentInspector::Split(const Entities& entities)
{
	// Components change outside the history only while the scene runs
	Entity active = entities.empty() ? Entity() : entities.front();
	const bool cacheable = m_History && m_History->IsEnabled() && active && !active.GetScene()->IsRunning();
	const uint64_t stateId = m_History ? m_History->GetStateId() : 0;
	if (!cacheable || !m_SplitCache.valid || m_SplitCache.stateId != stateId || m_SplitCache.entities != entities)
	{
		m_SplitCache.split = MultiEdit::SplitComponents(entities);
		m_SplitCache.entities = entities;
		m_SplitCache.stateId = stateId;
		m_SplitCache.valid = cacheable;
	}
	return m_SplitCache.split;
}

void ComponentInspector::DrawOverride(Support support, const Entities& entities, const std::function<void(const Entities&)>& draw)
{
	if (support == Support::ManyEntities || entities.size() == 1)
	{
		draw(entities);
		return;
	}

	const float top = ImGui::GetCursorPosY();
	ImGui::BeginDisabled();
	draw(Entities{ entities.front() });
	ImGui::EndDisabled();
	if (ImGui::GetCursorPosY() != top)
		ImGui::TextDisabled(ICON_FA_CIRCLE_INFO " Edits one entity at a time; showing the active one");
}

void ComponentInspector::DrawComponent(const Entities& entities, const ComponentType& type)
{
	const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding;
	const bool open = ImGui::TreeNodeEx((const void*)&type, flags, "%s", Title(type).c_str());
	DescriptionTooltip(type.description);

	bool remove = false;
	if (!type.Has(ComponentFlags::Required) && ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem(ICON_FA_TRASH_CAN " Delete"))
			remove = true;
		ImGui::EndPopup();
	}

	if (open)
	{
		ImGui::PushID(&type);
		auto replaced = m_ReplacedComponents.find(type.name);
		if (replaced != m_ReplacedComponents.end())
			DrawOverride(replaced->second.support, entities, [&](const Entities& shown) { replaced->second.draw(*this, shown); });
		else
			DrawFields(entities, type);

		auto appended = m_Appended.find(type.name);
		if (appended != m_Appended.end())
			DrawOverride(appended->second.support, entities, [&](const Entities& shown) { appended->second.draw(*this, shown); });
		ImGui::PopID();
		ImGui::TreePop();
	}

	if (remove)
	{
		// An open edit of the component closes while it's still there
		m_Edits.End();
		MultiEdit::RemoveComponent(m_History, entities, type);
	}
}

void ComponentInspector::DrawFields(const Entities& entities, const ComponentType& type)
{
	if (entities.empty())
		return;
	Entity active = entities.front();
	int heading = -1;
	for (int i = 0; i < (int)type.fields.size(); ++i)
	{
		const FieldInfo& field = type.fields[i];
		if (!type.tryGet(active.GetScene()->GetRegistry(), active.GetHandle()))
			return;
		if (field.group >= 0 && field.group != heading && HoldsForAll(entities, type, type.groups[field.group].when))
		{
			heading = field.group;
			ImGui::SeparatorText(type.groups[field.group].name.c_str());
		}
		DrawField(entities, type, i);
	}
}

bool ComponentInspector::DrawField(const Entities& entities, const ComponentType& type, int index)
{
	if (entities.empty())
		return false;
	Entity active = entities.front();
	const FieldInfo& field = type.fields[index];
	const void* component = type.tryGet(active.GetScene()->GetRegistry(), active.GetHandle());
	if (!component || field.Has(FieldFlags::HideInInspector) || !HoldsForAll(entities, type, field.showIf))
		return false;
	if (field.group >= 0 && !HoldsForAll(entities, type, type.groups[field.group].when))
		return false;

	ImGui::PushID(index);
	auto replaced = m_ReplacedFields.find(type.name + "." + field.name);
	if (replaced != m_ReplacedFields.end())
	{
		DrawOverride(replaced->second.support, entities, [&](const Entities& shown) { replaced->second.draw(*this, shown, field); });
		ImGui::PopID();
		return true;
	}
	std::optional<MultiEdit::FieldState> state;
	if (Inspector::WidgetFor(field) != Inspector::Widget::None)
		state = MultiEdit::ReadField(entities, type, index);
	if (!state)
	{
		ImGui::PopID();
		return false;
	}

	const PropertyPath path{ &type, index };
	const bool disabled = field.Has(FieldFlags::ReadOnly) || (field.enableIf && !HoldsForAll(entities, type, field.enableIf));
	ImGui::BeginDisabled(disabled);
	ImGui::BeginGroup();
	std::optional<Edit> edit = DrawWidget(field, *state, DefaultOf(type, index), component, entities.size() > 1);
	ImGui::EndGroup();
	ImGui::EndDisabled();
	DescriptionTooltip(field.Has(FieldFlags::Degrees) ? field.description + ", in degrees" : field.description);

	if (ImGui::IsItemActivated())
		m_Edits.Begin(entities, path);
	if (edit)
	{
		PropertyPath target = path;
		target.part = edit->part;
		m_Edits.Set(entities, target, edit->value);
	}
	if (ImGui::IsItemDeactivated() && m_Edits.IsOpen())
		m_Edits.End();

	ImGui::PopID();
	return true;
}

bool ComponentInspector::Set(const Entities& entities, const PropertyPath& path, const Value& value)
{
	return m_Edits.Set(entities, path, value);
}

Value ComponentInspector::DefaultOf(const ComponentType& type, int field)
{
	std::shared_ptr<void>& defaults = m_Defaults[&type];
	if (!defaults)
		defaults = type.create();
	return type.fields[field].get(defaults.get());
}

std::optional<ComponentInspector::Edit> ComponentInspector::DrawWidget(const FieldInfo& field, const MultiEdit::FieldState& state,
	const Value& defaultValue, const void* component, bool many)
{
	const char* label = field.label.c_str();
	const Value& value = state.value;
	const bool mixed = state.mixed;
	const bool degrees = field.Has(FieldFlags::Degrees);
	const float scale = degrees ? s_DegreesPerRadian : 1.0f;

	switch (field.type)
	{
	case ValueType::Bool:
	{
		// A mixed box shows a dash, and a click checks every entity
		bool checked = mixed ? false : value.As<bool>();
		ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);
		const bool clicked = ImGui::Checkbox(label, &checked);
		ImGui::PopItemFlag();
		return clicked ? std::optional<Edit>(Edit{ checked }) : std::nullopt;
	}
	case ValueType::Int:
	{
		int32_t number = value.As<int32_t>();
		const float speed = field.speed > 0.0f ? field.speed : 1.0f;
		bool changed = ImGui::DragInt(label, &number, speed, field.hasRange ? (int)field.min : 0, field.hasRange ? (int)field.max : 0,
			NumberFormat(label, mixed, "%d"));
		if (ResetClicked())
		{
			number = defaultValue.As<int32_t>();
			changed = true;
		}
		return changed ? std::optional<Edit>(Edit{ number }) : std::nullopt;
	}
	case ValueType::UInt:
	{
		uint32_t number = value.As<uint32_t>();
		const uint32_t min = field.hasRange ? (uint32_t)field.min : 0;
		const uint32_t max = field.hasRange ? (uint32_t)field.max : UINT_MAX;
		const float speed = field.speed > 0.0f ? field.speed : 1.0f;
		bool changed = ImGui::DragScalar(label, ImGuiDataType_U32, &number, speed, &min, &max, NumberFormat(label, mixed, "%u"));
		if (ResetClicked())
		{
			number = defaultValue.As<uint32_t>();
			changed = true;
		}
		return changed ? std::optional<Edit>(Edit{ number }) : std::nullopt;
	}
	case ValueType::Float:
	{
		float number = value.As<float>() * scale;
		const float speed = field.speed > 0.0f ? field.speed : 0.1f;
		bool changed = ImGui::DragFloat(label, &number, speed, field.hasRange ? field.min : 0.0f, field.hasRange ? field.max : 0.0f,
			NumberFormat(label, mixed, "%.3f"));
		if (ResetClicked())
		{
			number = defaultValue.As<float>() * scale;
			changed = true;
		}
		return changed ? std::optional<Edit>(Edit{ number / scale }) : std::nullopt;
	}
	case ValueType::String:
	{
		std::string text = mixed ? std::string() : value.As<std::string>();
		if (field.choices)
		{
			std::optional<Edit> chosen;
			if (ImGui::BeginCombo(label, mixed ? MixedText : text.c_str()))
			{
				for (const std::string& choice : field.choices(component))
				{
					if (ImGui::Selectable(choice.c_str(), !mixed && choice == text))
						chosen = Edit{ choice };
				}
				ImGui::EndCombo();
			}
			return chosen;
		}
		if (InputString(label, text, field.Has(FieldFlags::AsText), mixed ? MixedText : nullptr))
			return Edit{ text };
		return std::nullopt;
	}
	case ValueType::Vec2:
	{
		Vector2f vector = value.As<Vector2f>();
		const bool mixedParts[2] = { state.partMixed[0], state.partMixed[1] };
		int part = -1;
		if (!ImGui::Vector(label, vector, defaultValue.As<Vector2f>(), mixedParts, part) || part < 0)
			return std::nullopt;
		return Edit{ part == 0 ? vector.x : vector.y, part };
	}
	case ValueType::Vec3:
	{
		const Vector3f before = value.As<Vector3f>() * scale;
		Vector3f vector = before;
		const bool mixedParts[3] = { state.partMixed[0], state.partMixed[1], state.partMixed[2] };

		const bool lockable = field.Has(FieldFlags::LockParts);
		const bool locked = lockable && m_Unlocked.find(&field) == m_Unlocked.end();
		int part = -1;
		const bool edited = ImGui::Vector(label, vector, defaultValue.As<Vector3f>() * scale, mixedParts, part);

		// After the label, so the parts line up with the fields above
		if (lockable)
		{
			ImGui::SameLine();
			bool lock = locked;
			if (ImGui::Checkbox(locked ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN, &lock))
			{
				if (lock)
					m_Unlocked.erase(&field);
				else
					m_Unlocked[&field] = true;
			}
			ImGui::Tooltip("Lock the parts together");
		}

		if (!edited || part < 0)
			return std::nullopt;

		if (locked)
		{
			// Every part moves by the change made to one, from the active entity's value, and is set on all of them
			const float change = vector[part] - before[part];
			return Edit{ (before + Vector3f(change, change, change)) / scale };
		}
		return Edit{ vector[part] / scale, part };
	}
	case ValueType::Quaternion:
		// No widget: a quaternion field is edited through its Euler form
		return std::nullopt;
	case ValueType::Colour:
	{
		Colour colour = value.As<Colour>();
		// Several entities keep the same widgets whether or not they differ, so a drag isn't cut off when they match
		if (!many)
		{
			if (ImGui::ColorEdit4(label, &colour.r))
				return Edit{ colour };
			return std::nullopt;
		}
		// A drag per channel, laid out as the colour edit's own, each showing "--" while it differs; the swatch sets the whole colour
		const ImGuiStyle& style = ImGui::GetStyle();
		const float spacing = style.ItemInnerSpacing.x;
		const float channelWidth = std::max(1.0f, (ImGui::CalcItemWidth() - ImGui::GetFrameHeight() - spacing * 4.0f) / 4.0f);
		const char* formats[] = { "R:%3d", "G:%3d", "B:%3d", "A:%3d" };
		float* channels = &colour.r;
		std::optional<Edit> edit;
		for (int part = 0; part < 4; ++part)
		{
			if (part > 0)
				ImGui::SameLine(0.0f, spacing);
			ImGui::PushID(part);
			ImGui::SetNextItemWidth(channelWidth);
			int channel = (int)(channels[part] * 255.0f + 0.5f);
			if (ImGui::DragInt("##Channel", &channel, 1.0f, 0, 255, NumberFormat("##Channel", state.partMixed[part], formats[part])))
				edit = Edit{ (float)channel / 255.0f, part };
			ImGui::PopID();
		}
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::ColorEdit4("##Swatch", &colour.r, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
			edit = Edit{ colour };
		ImGui::SameLine(0.0f, spacing);
		ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));
		return edit;
	}
	case ValueType::Enum:
	{
		const EnumValue current = value.As<EnumValue>();
		const char* currentName = current.type->NameOf(current.value);
		const std::string preview = mixed ? MixedText : currentName ? SplitWords(currentName) : "";
		std::optional<Edit> chosen;
		if (ImGui::BeginCombo(label, preview.c_str()))
		{
			for (const EnumType::Entry& entry : current.type->entries)
			{
				if (ImGui::Selectable(SplitWords(entry.name).c_str(), !mixed && entry.value == current.value))
					chosen = Edit{ EnumValue{ current.type, entry.value } };
			}
			ImGui::EndCombo();
		}
		return chosen;
	}
	case ValueType::Asset:
	{
		const AssetValue& asset = value.As<AssetValue>();
		const Inspector::AssetKind* kind = Inspector::FindAssetKind(field.assetType);
		if (!kind)
			return std::nullopt;
		if (kind->fileType == FileType::IMAGE)
		{
			Ref<Texture2D> texture;
			if (!mixed)
			{
				texture = std::dynamic_pointer_cast<Texture2D>(asset.asset);
				if (!texture && !asset.path.empty())
					texture = AssetManager::GetTexture(asset.path);
			}
			if (ImGui::Texture2DEdit(label, texture, ImVec2(64.0f, 64.0f), mixed))
				return Edit{ AssetValue{ texture ? texture->GetFilepath() : std::filesystem::path(), texture } };
			return std::nullopt;
		}
		if (std::optional<AssetValue> picked = AssetPicker(label, asset, *kind, mixed))
			return Edit{ *picked };
		return std::nullopt;
	}
	case ValueType::Layer:
	{
		uint16_t layer = value.As<LayerValue>().bits;
		if (LayerEdit(label, layer, mixed))
			return Edit{ LayerValue{ layer } };
		return std::nullopt;
	}
	case ValueType::Mask:
	{
		uint16_t mask = value.As<MaskValue>().bits;
		if (MaskEdit(label, mask, mixed))
			return Edit{ MaskValue{ mask } };
		return std::nullopt;
	}
	case ValueType::List:
	{
		ValueList items = value.As<ValueList>();
		bool changed = false;
		const bool open = ImGui::TreeNode(label);
		if (mixed)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("%s", MixedText);
			ImGui::Tooltip("Differs between the entities; editing it sets this list, the active entity's, on all of them");
		}
		if (open)
		{
			FieldInfo itemField = field;
			itemField.type = field.itemType;
			itemField.flags = FieldFlags::None;
			if (ImGui::Button(ICON_FA_PLUS "##AddItem"))
			{
				const ValueList& defaults = defaultValue.As<ValueList>();
				items.push_back(!items.empty() ? items.back() : (!defaults.empty() ? defaults.front() : Value()));
				changed = true;
			}
			for (size_t i = 0; i < items.size();)
			{
				ImGui::PushID((int)i);
				if (ImGui::Button(ICON_FA_TRASH_CAN "##RemoveItem"))
				{
					items.erase(items.begin() + i);
					changed = true;
					ImGui::PopID();
					continue;
				}
				ImGui::SameLine();
				itemField.label = std::to_string(i);
				MultiEdit::FieldState itemState;
				itemState.value = items[i];
				if (std::optional<Edit> item = DrawWidget(itemField, itemState, items[i], component, many))
				{
					// A part edit of an item changes only that part
					if (item->part >= 0)
						items[i].SetPart(item->part, item->value.As<float>());
					else
						items[i] = item->value;
					changed = true;
				}
				ImGui::PopID();
				++i;
			}
			ImGui::TreePop();
		}
		return changed ? std::optional<Edit>(Edit{ std::move(items) }) : std::nullopt;
	}
	}
	return std::nullopt;
}

void ComponentInspector::DrawAddComponentMenu(const Entities& entities)
{
	if (entities.empty())
		return;

	ImGui::Dummy({ 0, 0 });
	const float width = ImGui::GetContentRegionAvail().x;
	const char* addLabel = ICON_FA_CIRCLE_PLUS " Add Component";
	ImGui::SameLine((width / 2.0f) - (ImGui::CalcTextSize(addLabel).x / 2.0f));
	if (ImGui::Button(addLabel))
		ImGui::OpenPopup("Components");

	if (!ImGui::BeginPopup("Components"))
		return;

	Entity active = entities.front();
	Scene& scene = *active.GetScene();
	std::vector<std::string> categories;
	for (const ComponentType* type : Components())
	{
		if (IsShown(*type) && std::find(categories.begin(), categories.end(), type->category) == categories.end())
			categories.push_back(type->category);
	}

	for (const std::string& category : categories)
	{
		if (!ImGui::BeginMenu(category.c_str()))
			continue;
		for (const ComponentType* type : Components())
		{
			if (!IsShown(*type) || type->category != category)
				continue;

			size_t lacking = 0;
			size_t addable = 0;
			const ComponentType* excluded = nullptr;
			for (Entity entity : entities)
			{
				if (type->tryGet(scene.GetRegistry(), entity.GetHandle()))
					continue;
				++lacking;
				if (const ComponentType* by = ExcludedBy(scene.GetRegistry(), entity.GetHandle(), *type))
					excluded = excluded ? excluded : by;
				else
					++addable;
			}

			// A script is chosen as it's added
			if (type == &TypeOf<LuaScriptComponent>())
			{
				std::vector<std::filesystem::path> scripts = Directory::GetFilesRecursive(Application::GetOpenDocumentDirectory(), ViewerManager::GetExtensions(FileType::SCRIPT));
				if (ImGui::BeginMenu(Title(*type).c_str(), addable > 0 && !scripts.empty()))
				{
					for (std::filesystem::path& file : scripts)
					{
						if (ImGui::MenuItem(file.filename().string().c_str()))
						{
							const std::filesystem::path script = FileUtils::RelativePath(file, Application::GetOpenDocumentDirectory());
							AddComponent(entities, *type, [&](Entity entity) { entity.AddComponent<LuaScriptComponent>(script); });
							break;
						}
					}
					ImGui::EndMenu();
				}
				continue;
			}

			const bool added = ImGui::MenuItem(Title(*type).c_str(), nullptr, false, addable > 0);
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				if (addable == 0 && excluded)
					ImGui::SetTooltip("Can't be on the same entity as a %s", SplitWords(excluded->name).c_str());
				else if (addable > 0 && addable < entities.size())
					ImGui::SetTooltip("Adds it to %zu of the %zu entities", addable, entities.size());
			}
			if (added)
				AddComponent(entities, *type, [&](Entity entity) { type->emplace(scene.GetRegistry(), entity.GetHandle()); });
		}
		ImGui::EndMenu();
	}
	ImGui::EndPopup();
}

void ComponentInspector::AddComponent(const Entities& entities, const ComponentType& type, const std::function<void(Entity)>& add)
{
	MultiEdit::AddComponent(m_History, entities, type, add);
}
