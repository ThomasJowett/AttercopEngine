#include "ComponentInspector.h"

#include "History/HistoryCommands.h"
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

	bool InputString(const char* label, std::string& text, bool multiline)
	{
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize;
		if (multiline)
			return ImGui::InputTextMultiline(label, text.data(), text.capacity() + 1, ImVec2(0, 0),
				flags | ImGuiInputTextFlags_AllowTabInput, &ResizeString, &text);
		return ImGui::InputText(label, text.data(), text.capacity() + 1, flags, &ResizeString, &text);
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

	bool LayerEdit(const char* label, uint16_t& layer)
	{
		bool changed = false;
		if (ImGui::BeginCombo(label, CollisionLayers::GetLayerName(layer).c_str()))
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

	bool MaskEdit(const char* label, uint16_t& mask)
	{
		uint16_t namedBits = 0;
		for (int i = 0; i < CollisionLayers::MaxLayers; ++i)
		{
			if (CollisionLayers::IsNamed(i))
				namedBits |= (uint16_t)(1 << i);
		}

		std::string preview;
		if ((mask & namedBits) == namedBits)
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
	std::optional<AssetValue> AssetPicker(const char* label, const AssetValue& asset, const Inspector::AssetKind& kind)
	{
		std::optional<AssetValue> picked;
		const Ref<Asset> defaultAsset = DefaultAsset(kind.type);
		const bool isDefault = !asset.IsSet() || (defaultAsset && asset.path == defaultAsset->GetFilepath());

		ImGui::TextUnformatted(label);
		std::string preview = isDefault ? (defaultAsset ? "Default" : "None") : asset.path.filename().string();
		if (ImGui::BeginAssetCombo("##Asset", preview, asset.asset.get()))
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

		if (!isDefault)
		{
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_PEN_TO_SQUARE "##EditAsset"))
				ViewerManager::OpenViewer(asset.path);
			ImGui::Tooltip("Open Asset Viewer");

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_XMARK "##ClearAsset"))
				picked = AssetValue{};
			ImGui::Tooltip(defaultAsset ? "Use the default" : "Clear");
		}
		return picked;
	}
}

ComponentInspector::ComponentInspector(SceneHistory* history)
	: m_Edits(history), m_History(history)
{
}

void ComponentInspector::ReplaceComponent(std::string_view component, ComponentDrawer draw)
{
	m_ReplacedComponents[std::string(component)] = std::move(draw);
}

void ComponentInspector::AppendToComponent(std::string_view component, ComponentDrawer draw)
{
	m_Appended[std::string(component)] = std::move(draw);
}

void ComponentInspector::ReplaceField(std::string_view path, FieldDrawer draw)
{
	m_ReplacedFields[std::string(path)] = std::move(draw);
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

void ComponentInspector::Draw(Entity entity)
{
	for (const ComponentType* type : Components())
	{
		if (IsShown(*type) && type->tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle()))
			DrawComponent(entity, *type);
	}

	// An interaction whose widget went away, as when a ShowIf stopped holding
	if (m_Edits.IsOpen() && !ImGui::IsAnyItemActive())
		m_Edits.End();
}

void ComponentInspector::DrawComponent(Entity entity, const ComponentType& type)
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
			replaced->second(*this, entity);
		else
			DrawFields(entity, type);

		auto appended = m_Appended.find(type.name);
		if (appended != m_Appended.end())
			appended->second(*this, entity);
		ImGui::PopID();
		ImGui::TreePop();
	}

	if (remove)
	{
		Scene& scene = *entity.GetScene();
		// An open edit of the component closes while it's still there
		m_Edits.End();
		if (m_History)
			m_History->End(scene);
		Scope<RemoveComponentCommand> command = CreateScope<RemoveComponentCommand>(entity, type);
		type.remove(scene.GetRegistry(), entity.GetHandle());
		scene.MakeDirty();
		if (m_History)
			m_History->Push(scene, std::move(command));
	}
}

void ComponentInspector::DrawFields(Entity entity, const ComponentType& type)
{
	int heading = -1;
	for (int i = 0; i < (int)type.fields.size(); ++i)
	{
		const FieldInfo& field = type.fields[i];
		const void* component = type.tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle());
		if (!component)
			return;
		if (field.group >= 0 && field.group != heading && type.groups[field.group].when(component))
		{
			heading = field.group;
			ImGui::SeparatorText(type.groups[field.group].name.c_str());
		}
		DrawField(entity, type, i);
	}
}

bool ComponentInspector::DrawField(Entity entity, const ComponentType& type, int index)
{
	const FieldInfo& field = type.fields[index];
	const void* component = type.tryGet(entity.GetScene()->GetRegistry(), entity.GetHandle());
	if (!component || field.Has(FieldFlags::HideInInspector) || !field.showIf(component))
		return false;
	if (field.group >= 0 && !type.groups[field.group].when(component))
		return false;

	ImGui::PushID(index);
	auto replaced = m_ReplacedFields.find(type.name + "." + field.name);
	if (replaced != m_ReplacedFields.end())
	{
		replaced->second(*this, entity, field);
		ImGui::PopID();
		return true;
	}
	if (Inspector::WidgetFor(field) == Inspector::Widget::None)
	{
		ImGui::PopID();
		return false;
	}

	const PropertyPath path{ &type, index };
	const bool disabled = field.Has(FieldFlags::ReadOnly) || (field.enableIf && !field.enableIf(component));
	ImGui::BeginDisabled(disabled);
	ImGui::BeginGroup();
	std::optional<Value> edited = DrawWidget(field, field.get(component), DefaultOf(type, index), component);
	ImGui::EndGroup();
	ImGui::EndDisabled();
	DescriptionTooltip(field.Has(FieldFlags::Degrees) ? field.description + ", in degrees" : field.description);

	if (ImGui::IsItemActivated())
		m_Edits.Begin(entity, path);
	if (edited)
		m_Edits.Set(entity, path, *edited);
	if (ImGui::IsItemDeactivated() && m_Edits.IsOpen())
		m_Edits.End();

	ImGui::PopID();
	return true;
}

bool ComponentInspector::Set(Entity entity, const PropertyPath& path, const Value& value)
{
	return m_Edits.Set(entity, path, value);
}

Value ComponentInspector::DefaultOf(const ComponentType& type, int field)
{
	std::shared_ptr<void>& defaults = m_Defaults[&type];
	if (!defaults)
		defaults = type.create();
	return type.fields[field].get(defaults.get());
}

std::optional<Value> ComponentInspector::DrawWidget(const FieldInfo& field, const Value& value, const Value& defaultValue,
	const void* component)
{
	const char* label = field.label.c_str();
	const bool degrees = field.Has(FieldFlags::Degrees);
	const float scale = degrees ? s_DegreesPerRadian : 1.0f;

	switch (field.type)
	{
	case ValueType::Bool:
	{
		bool checked = value.As<bool>();
		if (ImGui::Checkbox(label, &checked))
			return Value(checked);
		return std::nullopt;
	}
	case ValueType::Int:
	{
		int32_t number = value.As<int32_t>();
		const float speed = field.speed > 0.0f ? field.speed : 1.0f;
		bool changed = ImGui::DragInt(label, &number, speed, field.hasRange ? (int)field.min : 0, field.hasRange ? (int)field.max : 0);
		if (ResetClicked())
		{
			number = defaultValue.As<int32_t>();
			changed = true;
		}
		return changed ? std::optional<Value>(number) : std::nullopt;
	}
	case ValueType::UInt:
	{
		uint32_t number = value.As<uint32_t>();
		const uint32_t min = field.hasRange ? (uint32_t)field.min : 0;
		const uint32_t max = field.hasRange ? (uint32_t)field.max : UINT_MAX;
		const float speed = field.speed > 0.0f ? field.speed : 1.0f;
		bool changed = ImGui::DragScalar(label, ImGuiDataType_U32, &number, speed, &min, &max);
		if (ResetClicked())
		{
			number = defaultValue.As<uint32_t>();
			changed = true;
		}
		return changed ? std::optional<Value>(number) : std::nullopt;
	}
	case ValueType::Float:
	{
		float number = value.As<float>() * scale;
		const float speed = field.speed > 0.0f ? field.speed : 0.1f;
		bool changed = ImGui::DragFloat(label, &number, speed, field.hasRange ? field.min : 0.0f, field.hasRange ? field.max : 0.0f);
		if (ResetClicked())
		{
			number = defaultValue.As<float>() * scale;
			changed = true;
		}
		return changed ? std::optional<Value>(number / scale) : std::nullopt;
	}
	case ValueType::String:
	{
		std::string text = value.As<std::string>();
		if (field.choices)
		{
			std::optional<Value> chosen;
			if (ImGui::BeginCombo(label, text.c_str()))
			{
				for (const std::string& choice : field.choices(component))
				{
					if (ImGui::Selectable(choice.c_str(), choice == text))
						chosen = Value(choice);
				}
				ImGui::EndCombo();
			}
			return chosen;
		}
		if (InputString(label, text, field.Has(FieldFlags::AsText)))
			return Value(text);
		return std::nullopt;
	}
	case ValueType::Vec2:
	{
		Vector2f vector = value.As<Vector2f>();
		if (ImGui::Vector(label, vector, defaultValue.As<Vector2f>()))
			return Value(vector);
		return std::nullopt;
	}
	case ValueType::Vec3:
	{
		const Vector3f before = value.As<Vector3f>() * scale;
		Vector3f vector = before;

		const bool lockable = field.Has(FieldFlags::LockParts);
		const bool locked = lockable && m_Unlocked.find(&field) == m_Unlocked.end();
		const bool edited = ImGui::Vector(label, vector, defaultValue.As<Vector3f>() * scale);

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

		if (!edited)
			return std::nullopt;

		if (locked)
		{
			// Every part moves by the change made to one
			const float changes[3] = { vector.x - before.x, vector.y - before.y, vector.z - before.z };
			float change = changes[0];
			for (float part : changes)
			{
				if (std::fabs(part) > std::fabs(change))
					change = part;
			}
			vector = before + Vector3f(change, change, change);
		}
		return Value(vector / scale);
	}
	case ValueType::Quaternion:
		// No widget: a quaternion field is edited through its Euler form
		return std::nullopt;
	case ValueType::Colour:
	{
		Colour colour = value.As<Colour>();
		if (ImGui::ColorEdit4(label, &colour.r))
			return Value(colour);
		return std::nullopt;
	}
	case ValueType::Enum:
	{
		const EnumValue current = value.As<EnumValue>();
		const char* currentName = current.type->NameOf(current.value);
		std::optional<Value> chosen;
		if (ImGui::BeginCombo(label, currentName ? SplitWords(currentName).c_str() : ""))
		{
			for (const EnumType::Entry& entry : current.type->entries)
			{
				if (ImGui::Selectable(SplitWords(entry.name).c_str(), entry.value == current.value))
					chosen = Value(EnumValue{ current.type, entry.value });
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
			Ref<Texture2D> texture = std::dynamic_pointer_cast<Texture2D>(asset.asset);
			if (!texture && !asset.path.empty())
				texture = AssetManager::GetTexture(asset.path);
			if (ImGui::Texture2DEdit(label, texture))
				return Value(AssetValue{ texture ? texture->GetFilepath() : std::filesystem::path(), texture });
			return std::nullopt;
		}
		if (std::optional<AssetValue> picked = AssetPicker(label, asset, *kind))
			return Value(*picked);
		return std::nullopt;
	}
	case ValueType::Layer:
	{
		uint16_t layer = value.As<LayerValue>().bits;
		if (LayerEdit(label, layer))
			return Value(LayerValue{ layer });
		return std::nullopt;
	}
	case ValueType::Mask:
	{
		uint16_t mask = value.As<MaskValue>().bits;
		if (MaskEdit(label, mask))
			return Value(MaskValue{ mask });
		return std::nullopt;
	}
	case ValueType::List:
	{
		ValueList items = value.As<ValueList>();
		bool changed = false;
		if (ImGui::TreeNode(label))
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
				if (std::optional<Value> item = DrawWidget(itemField, items[i], items[i], component))
				{
					items[i] = *item;
					changed = true;
				}
				ImGui::PopID();
				++i;
			}
			ImGui::TreePop();
		}
		return changed ? std::optional<Value>(std::move(items)) : std::nullopt;
	}
	}
	return std::nullopt;
}

void ComponentInspector::DrawAddComponentMenu(Entity entity)
{
	ImGui::Dummy({ 0, 0 });
	const float width = ImGui::GetContentRegionAvail().x;
	const char* addLabel = ICON_FA_CIRCLE_PLUS " Add Component";
	ImGui::SameLine((width / 2.0f) - (ImGui::CalcTextSize(addLabel).x / 2.0f));
	if (ImGui::Button(addLabel))
		ImGui::OpenPopup("Components");

	if (!ImGui::BeginPopup("Components"))
		return;

	Scene& scene = *entity.GetScene();
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
			const bool has = type->tryGet(scene.GetRegistry(), entity.GetHandle()) != nullptr;
			const ComponentType* excluded = ExcludedBy(scene.GetRegistry(), entity.GetHandle(), *type);

			// A script is chosen as it's added
			if (type == &TypeOf<LuaScriptComponent>())
			{
				std::vector<std::filesystem::path> scripts = Directory::GetFilesRecursive(Application::GetOpenDocumentDirectory(), ViewerManager::GetExtensions(FileType::SCRIPT));
				if (ImGui::BeginMenu(Title(*type).c_str(), !has && !scripts.empty()))
				{
					for (std::filesystem::path& file : scripts)
					{
						if (ImGui::MenuItem(file.filename().string().c_str()))
						{
							entity.AddComponent<LuaScriptComponent>(FileUtils::RelativePath(file, Application::GetOpenDocumentDirectory()));
							break;
						}
					}
					ImGui::EndMenu();
				}
				continue;
			}

			const bool added = ImGui::MenuItem(Title(*type).c_str(), nullptr, false, !has && !excluded);
			if (excluded && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("Can't be on the same entity as a %s", SplitWords(excluded->name).c_str());
			if (added)
			{
				Scope<AddComponentCommand> command = CreateScope<AddComponentCommand>(entity, *type);
				type->emplace(scene.GetRegistry(), entity.GetHandle());
				scene.MakeDirty();
				if (m_History)
					m_History->Push(scene, std::move(command));
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndPopup();
}
