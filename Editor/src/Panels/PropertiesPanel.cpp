#include "PropertiesPanel.h"

#include "IconsFontAwesome6.h"
#include "MainDockSpace.h"

#include "ImGui/ImGuiAssetEdit.h"
#include "ImGui/ImGuiVectorEdit.h"

#include "Viewers/ViewerManager.h"
#include "FileSystem/Directory.h"
#include "Scene/Components.h"
#include "Scene/SceneManager.h"
#include "ImGui/ImGuiUtilities.h"

#include <algorithm>
#include <optional>

namespace PropertiesPanelOverrides
{
	// Scales the transform so the texture's pixels match the scene's pixels per unit
	void PixelPerfect(ComponentInspector& inspector, Entity entity, uint32_t width, uint32_t height)
	{
		TransformComponent* transform = entity.TryGetComponent<TransformComponent>();
		if (!transform)
			return;
		const float pixelsPerUnit = (float)entity.GetScene()->GetPixelsPerUnit();
		const Vector3f scale((float)width / pixelsPerUnit, (float)height / pixelsPerUnit, transform->GetScale().z);
		if (std::optional<Reflection::PropertyPath> path = Reflection::PropertyPath::Parse("Transform.Scale"))
			inspector.Set({ entity }, *path, scale);
	}

	// Reloads an asset when its file changes on disk
	template<typename A>
	void ReloadIfChanged(const Ref<A>& asset, std::filesystem::file_time_type& lastWriteTime, bool& reloaded)
	{
		reloaded = false;
		std::error_code error;
		const std::filesystem::path path = std::filesystem::absolute(Application::GetOpenDocumentDirectory() / asset->GetFilepath());
		const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, error);
		if (!error && writeTime != lastWriteTime)
		{
			asset->Reload();
			lastWriteTime = writeTime;
			reloaded = true;
		}
	}
}

PropertiesPanel::PropertiesPanel(bool* show, Ref<EntitySelection> selection, Ref<TilemapEditor> tilemapEditor, Ref<SceneHistory> history)
	:Layer("Properties"), m_Show(show), m_Selection(selection), m_TilemapEditor(tilemapEditor), m_History(history),
	m_Inspector(history.get())
{
	m_Inspector.SetSelectEntity([this](const Uuid& id) { m_Selection->Select(id); });
	RegisterOverrides();
}

void PropertiesPanel::RegisterOverrides()
{
	using namespace PropertiesPanelOverrides;
	using Entities = ComponentInspector::Entities;
	using Support = ComponentInspector::Support;

	// The tile painting UI: the fields, kept rebuilt as the tileset changes, and the Tilemap Editor
	m_Inspector.ReplaceComponent("Tilemap", Support::OneEntity, [this](ComponentInspector& inspector, const Entities& entities)
		{
			Entity entity = entities.front();
			TilemapComponent& tilemap = entity.GetComponent<TilemapComponent>();
			tilemap.UpdateRebuild();
			if (tilemap.tileset)
			{
				static std::filesystem::file_time_type tilesetWriteTime;
				bool reloaded = false;
				ReloadIfChanged(tilemap.tileset, tilesetWriteTime, reloaded);
				if (reloaded)
					tilemap.Rebuild();
			}

			inspector.DrawFields(entities, Reflection::TypeOf<TilemapComponent>());

			if (ImGui::Button("Edit Tilemap"))
				m_TilemapEditor->Show();
		});

	// One row per material slot of the mesh, when every entity has the same mesh; clearing a row goes back to the mesh's own material
	m_Inspector.ReplaceField("StaticMesh.Materials", Support::ManyEntities, [](ComponentInspector& inspector, const Entities& entities, const Reflection::FieldInfo&)
		{
			Entity active = entities.front();
			const StaticMeshComponent& first = active.GetComponent<StaticMeshComponent>();
			for (Entity entity : entities)
			{
				if (entity.GetComponent<StaticMeshComponent>().mesh != first.mesh)
				{
					ImGui::TextDisabled("Material slots show when every entity has the same mesh");
					return;
				}
			}

			const Reflection::ComponentType& type = Reflection::TypeOf<StaticMeshComponent>();
			for (size_t slot = 0; slot < first.GetMaterialSlotCount(); ++slot)
			{
				Ref<Material> material = first.GetMaterial(slot);
				bool mixed = false;
				for (Entity entity : entities)
					mixed = mixed || entity.GetComponent<StaticMeshComponent>().GetMaterial(slot) != material;

				if (ImGui::AssetEdit<Material>(("Material " + std::to_string(slot)).c_str(), material, first.GetMeshMaterial(slot), FileType::MATERIAL, mixed))
				{
					inspector.GetEdits().Record(entities, Reflection::PropertyPath{ &type, type.FindField("Materials") });
					for (Entity entity : entities)
						entity.GetComponent<StaticMeshComponent>().SetMaterialOverride(slot, material);
					active.GetScene()->MakeDirty();
				}
			}
		});

	// The script drop-down, with a button that opens it; a dash when the entities' scripts differ
	m_Inspector.ReplaceField("LuaScript.Script", Support::ManyEntities, [](ComponentInspector& inspector, const Entities& entities, const Reflection::FieldInfo&)
		{
			const Reflection::ComponentType& type = Reflection::TypeOf<LuaScriptComponent>();
			Entity active = entities.front();
			const Ref<LuaScript>& script = active.GetComponent<LuaScriptComponent>().script;
			bool mixed = false;
			for (Entity entity : entities)
			{
				const Ref<LuaScript>& other = entity.GetComponent<LuaScriptComponent>().script;
				mixed = mixed || (script ? script->GetFilepath() : std::filesystem::path()) != (other ? other->GetFilepath() : std::filesystem::path());
			}

			const std::string scriptName = mixed ? ComponentInspector::MixedText : script ? script->GetFilepath().filename().string() : "";
			if (ImGui::BeginAssetCombo("##luaScript", scriptName, mixed ? nullptr : script.get()))
			{
				for (std::filesystem::path& file : Directory::GetFilesRecursive(Application::GetOpenDocumentDirectory(), ViewerManager::GetExtensions(FileType::SCRIPT)))
				{
					if (ImGui::Selectable(file.filename().string().c_str()))
					{
						const Reflection::AssetValue picked{ FileUtils::RelativePath(file, Application::GetOpenDocumentDirectory()), nullptr };
						inspector.Set(entities, Reflection::PropertyPath{ &type, type.FindField("Script") }, picked);
						break;
					}
					ImGui::Tooltip(file.string().c_str());
				}
				ImGui::EndCombo();
			}
			if (script && !mixed)
			{
				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_PEN_TO_SQUARE "##LuaScript"))
					ViewerManager::OpenViewer(script->GetFilepath());
				ImGui::Tooltip("Edit script");
			}
		});

	m_Inspector.AppendToComponent("Sprite", Support::OneEntity, [this](ComponentInspector& inspector, const Entities& entities)
		{
			Entity entity = entities.front();
			SpriteComponent& sprite = entity.GetComponent<SpriteComponent>();
			if (!sprite.texture && sprite.texturePath.empty())
				return;
			if (const Ref<Texture2D>& texture = sprite.GetTexture())
			{
				if (ImGui::Button(ICON_FA_EXPAND " Pixel Perfect"))
				{
					HistoryGroup group(*m_History, *entity.GetScene(), "Pixel Perfect");
					PixelPerfect(inspector, entity, texture->GetWidth(), texture->GetHeight());
				}
				ImGui::Tooltip("Set Scale to pixel perfect scaling");
			}
		});

	m_Inspector.AppendToComponent("AnimatedSprite", Support::OneEntity, [this](ComponentInspector& inspector, const Entities& entities)
		{
			Entity entity = entities.front();
			AnimatedSpriteComponent& sprite = entity.GetComponent<AnimatedSpriteComponent>();
			if (!sprite.spriteSheet)
				return;
			static std::filesystem::file_time_type spriteSheetWriteTime;
			bool reloaded = false;
			ReloadIfChanged(sprite.spriteSheet, spriteSheetWriteTime, reloaded);

			if (ImGui::Button(ICON_FA_EXPAND " Pixel Perfect"))
			{
				HistoryGroup group(*m_History, *entity.GetScene(), "Pixel Perfect");
				const Ref<SubTexture2D> frame = sprite.spriteSheet->GetSubTexture();
				PixelPerfect(inspector, entity, frame->GetSpriteWidth(), frame->GetSpriteHeight());
			}
			ImGui::Tooltip("Set Scale to pixel perfect scaling");
		});

	m_Inspector.AppendToComponent("Button", Support::OneEntity, [this](ComponentInspector& inspector, const Entities& entities)
		{
			Entity entity = entities.front();
			ButtonComponent& button = entity.GetComponent<ButtonComponent>();
			if (!button.normalTexture)
				return;
			if (ImGui::Button(ICON_FA_EXPAND " Pixel Perfect"))
			{
				if (WidgetComponent* widget = entity.TryGetComponent<WidgetComponent>())
				{
					HistoryGroup group(*m_History, *entity.GetScene(), "Pixel Perfect");
					inspector.GetEdits().RecordComponent({ entity }, Reflection::TypeOf<WidgetComponent>());
					// A fixed size is what makes the size below take effect
					widget->fixedWidth = true;
					widget->fixedHeight = true;
					widget->SetSizeX((float)button.normalTexture->GetWidth());
					widget->SetSizeY((float)button.normalTexture->GetHeight());
					entity.GetScene()->MakeDirty();
				}
			}
			ImGui::Tooltip("Set the Widget's size to the Normal texture's pixel dimensions");
		});
}

void PropertiesPanel::OnImGuiRender()
{
	PROFILE_FUNCTION();

	if (!*m_Show)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(ICON_FA_SCREWDRIVER_WRENCH" Properties", m_Show))
	{
		if (ImGui::IsWindowFocused())
		{
			MainDockSpace::SetFocussedWindow(this);
		}

		Scene* selectionScene = SceneManager::CurrentScene();
		const std::vector<Entity> entities = selectionScene ? SelectedEntities(*selectionScene) : std::vector<Entity>();
		if (!entities.empty())
		{
			ImGui::BeginGroup();
			m_Inspector.DrawSummary(entities);
			m_Inspector.DrawField(entities, Reflection::TypeOf<NameComponent>(), 0);
			m_Inspector.Draw(entities);
			ImGui::Separator();
			m_Inspector.DrawAddComponentMenu(entities);
			ImVec2 available = ImGui::GetContentRegionAvail();
			ImGui::Dummy(available);
			ImGui::EndGroup();
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_AcceptPeekOnly))
				{
					std::filesystem::path* file = (std::filesystem::path*)payload->Data;

					if (ViewerManager::GetFileType(*file) == FileType::MESH)
					{
						if (ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_None))
							SetOrAdd(entities, Reflection::TypeOf<StaticMeshComponent>(), "Mesh", *file);
					}
					else if (file->extension() == ".lua")
					{
						if (ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_None))
							SetOrAdd(entities, Reflection::TypeOf<LuaScriptComponent>(), "Script", *file);
					}

					CLIENT_DEBUG(file->string());
				}
				ImGui::EndDragDropTarget();
			}
		}
		else if (SceneManager::IsSceneLoaded())
		{
			using Setting = SetSceneSettingCommand::Setting;
			Scene& scene = *SceneManager::CurrentScene();
			Vector2f gravity = scene.GetGravity();
			if (ImGui::Vector("Gravity Scale", gravity, Vector2f(0.0f, -9.81f)))
				SetSceneSetting(scene, Setting::Gravity, gravity);
			int pixels = scene.GetPixelsPerUnit();
			if (ImGui::InputInt("Pixels per unit", &pixels, 1, 16, ImGuiInputTextFlags_CharsDecimal))
				SetSceneSetting(scene, Setting::PixelsPerUnit, (uint32_t)std::max(pixels, 1));
		}
	}
	ImGui::End();
}

std::vector<Entity> PropertiesPanel::SelectedEntities(Scene& scene) const
{
	std::vector<Entity> entities = m_Selection->Resolve(scene);
	const Uuid active = m_Selection->GetActive();
	auto found = std::find_if(entities.begin(), entities.end(), [&](Entity entity) { return entity.GetID() == active; });
	if (found != entities.end())
		std::rotate(entities.begin(), found, found + 1);
	return entities;
}

void PropertiesPanel::SetOrAdd(const std::vector<Entity>& entities, const Reflection::ComponentType& type, const char* field, const std::filesystem::path& file)
{
	Entity active = entities.front();
	Scene& scene = *active.GetScene();
	const Reflection::PropertyPath path{ &type, type.FindField(field) };
	const Reflection::AssetValue asset{ FileUtils::RelativePath(file, Application::GetOpenDocumentDirectory()), nullptr };

	std::vector<Entity> having;
	for (Entity entity : entities)
	{
		if (type.tryGet(scene.GetRegistry(), entity.GetHandle()))
			having.push_back(entity);
	}

	// One step when it both sets and adds
	std::optional<HistoryGroup> group;
	if (!having.empty() && having.size() < entities.size())
		group.emplace(*m_History, scene, "Set " + std::string(field));
	if (!having.empty())
		m_Inspector.Set(having, path, asset);
	m_Inspector.AddComponent(entities, type, [&](Entity entity)
		{
			type.emplace(scene.GetRegistry(), entity.GetHandle());
			Reflection::Set(entity, path, asset, Reflection::ChangeSource::Editor);
		});
}

void PropertiesPanel::SetSceneSetting(Scene& scene, SetSceneSettingCommand::Setting setting, const Reflection::Value& value)
{
	const EditKey key = SetSceneSettingCommand::KeyOf(setting);
	if (m_History->IsEnabled() && !m_History->IsOpen(key))
		m_History->Begin(scene, key, CreateScope<SetSceneSettingCommand>(scene, setting));
	SetSceneSettingCommand::Set(scene, setting, value);
	scene.MakeDirty();
}
