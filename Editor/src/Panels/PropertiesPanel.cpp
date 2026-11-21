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
			inspector.Set(entity, *path, scale);
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

PropertiesPanel::PropertiesPanel(bool* show, Ref<HierarchyPanel> hierarchyPanel, Ref<TilemapEditor> tilemapEditor, Ref<SceneHistory> history)
	:Layer("Properties"), m_Show(show), m_HierarchyPanel(hierarchyPanel), m_TilemapEditor(tilemapEditor), m_History(history),
	m_Inspector(history.get())
{
	RegisterOverrides();
}

void PropertiesPanel::RegisterOverrides()
{
	using namespace PropertiesPanelOverrides;

	// The tile painting UI: the fields, kept rebuilt as the tileset changes, and the Tilemap Editor
	m_Inspector.ReplaceComponent("Tilemap", [this](ComponentInspector& inspector, Entity entity)
		{
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

			inspector.DrawFields(entity, Reflection::TypeOf<TilemapComponent>());

			if (ImGui::Button("Edit Tilemap"))
				m_TilemapEditor->Show();
		});

	// One row per material slot of the mesh; clearing a row goes back to the mesh's own material
	m_Inspector.ReplaceField("StaticMesh.Materials", [](ComponentInspector& inspector, Entity entity, const Reflection::FieldInfo&)
		{
			StaticMeshComponent& staticMesh = entity.GetComponent<StaticMeshComponent>();
			for (size_t slot = 0; slot < staticMesh.GetMaterialSlotCount(); ++slot)
			{
				Ref<Material> material = staticMesh.GetMaterial(slot);
				const Ref<Material> previous = material;
				ImGui::AssetEdit<Material>(("Material " + std::to_string(slot)).c_str(), material, staticMesh.GetMeshMaterial(slot), FileType::MATERIAL);
				if (material != previous)
				{
					inspector.GetEdits().RecordComponentEdit(entity, Reflection::TypeOf<StaticMeshComponent>());
					staticMesh.SetMaterialOverride(slot, material);
					entity.GetScene()->MakeDirty();
				}
			}
		});

	// The script drop-down, with a button that opens it
	m_Inspector.ReplaceField("LuaScript.Script", [](ComponentInspector& inspector, Entity entity, const Reflection::FieldInfo&)
		{
			const Reflection::ComponentType& type = Reflection::TypeOf<LuaScriptComponent>();
			LuaScriptComponent& luaScript = entity.GetComponent<LuaScriptComponent>();
			std::string scriptName = luaScript.script ? luaScript.script->GetFilepath().filename().string() : "";

			if (ImGui::BeginAssetCombo("##luaScript", scriptName, luaScript.script.get()))
			{
				for (std::filesystem::path& file : Directory::GetFilesRecursive(Application::GetOpenDocumentDirectory(), ViewerManager::GetExtensions(FileType::SCRIPT)))
				{
					if (ImGui::Selectable(file.filename().string().c_str()))
					{
						const Reflection::AssetValue script{ FileUtils::RelativePath(file, Application::GetOpenDocumentDirectory()), nullptr };
						inspector.Set(entity, Reflection::PropertyPath{ &type, type.FindField("Script") }, script);
						break;
					}
					ImGui::Tooltip(file.string().c_str());
				}
				ImGui::EndCombo();
			}
			if (luaScript.script)
			{
				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_PEN_TO_SQUARE "##LuaScript"))
					ViewerManager::OpenViewer(luaScript.script->GetFilepath());
				ImGui::Tooltip("Edit script");
			}
		});

	m_Inspector.AppendToComponent("Sprite", [this](ComponentInspector& inspector, Entity entity)
		{
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

	m_Inspector.AppendToComponent("AnimatedSprite", [this](ComponentInspector& inspector, Entity entity)
		{
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

	m_Inspector.AppendToComponent("Button", [this](ComponentInspector& inspector, Entity entity)
		{
			ButtonComponent& button = entity.GetComponent<ButtonComponent>();
			if (!button.normalTexture)
				return;
			if (ImGui::Button(ICON_FA_EXPAND " Pixel Perfect"))
			{
				if (WidgetComponent* widget = entity.TryGetComponent<WidgetComponent>())
				{
					HistoryGroup group(*m_History, *entity.GetScene(), "Pixel Perfect");
					inspector.GetEdits().RecordComponentEdit(entity, Reflection::TypeOf<WidgetComponent>());
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

		Entity entity = m_HierarchyPanel->GetSelectedEntity();
		if (entity)
		{
			ImGui::BeginGroup();
			m_Inspector.DrawField(entity, Reflection::TypeOf<NameComponent>(), 0);
			m_Inspector.Draw(entity);
			ImGui::Separator();
			m_Inspector.DrawAddComponentMenu(entity);
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
						{
							StaticMeshComponent comp = entity.GetOrAddComponent<StaticMeshComponent>();
							comp.mesh->Load(FileUtils::RelativePath(*file, Application::GetOpenDocumentDirectory()));
						}
					}
					else if (file->extension() == ".lua")
					{
						if (ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_None))
						{
							LuaScriptComponent& comp = entity.GetOrAddComponent<LuaScriptComponent>();
							comp.script = AssetManager::GetAsset<LuaScript>(FileUtils::RelativePath(*file, Application::GetOpenDocumentDirectory()));
							comp.ParseScript(entity);
						}
					}

					CLIENT_DEBUG(file->string());
				}
				ImGui::EndDragDropTarget();
			}
		}
		else if (SceneManager::IsSceneLoaded())
		{
			Vector2f gravity = SceneManager::CurrentScene()->GetGravity();
			if (ImGui::Vector("Gravity Scale", gravity, Vector2f(0.0f, -9.81f)))
			{
				SceneManager::CurrentScene()->SetGravity(gravity);
				SceneManager::CurrentScene()->MakeDirty();
			}
			int pixels = SceneManager::CurrentScene()->GetPixelsPerUnit();
			if (ImGui::InputInt("Pixels per unit", &pixels, 1, 16, ImGuiInputTextFlags_CharsDecimal))
			{
				SceneManager::CurrentScene()->SetPixelsPerUnit(pixels);
				SceneManager::CurrentScene()->MakeDirty();
			}
		}
	}
	ImGui::End();
}
