#pragma once

#include "imgui/imgui.h"

#include "Asset/Material.h"
#include "Scene/AssetManager.h"
#include "Viewers/ViewerManager.h"
#include "FileSystem/Directory.h"
#include "ImGui/ImGuiUtilities.h"
#include "IconsFontAwesome6.h"

namespace ImGui
{
// Opens an asset drop-down, previewed in red with a warning icon when the asset failed to load
inline bool BeginAssetCombo(const char* id, const std::string& preview, const Asset* asset)
{
	bool missing = asset && asset->IsMissing();
	if (missing)
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));

	std::string shownPreview = missing ? std::string(ICON_FA_TRIANGLE_EXCLAMATION " ") + preview : preview;
	bool open = ImGui::BeginCombo(id, shownPreview.c_str());

	if (missing)
	{
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Missing: %s couldn't be loaded", asset->GetFilepath().string().c_str());
	}
	return open;
}

template<typename T>
bool AssetEdit(const char* label, Ref<T>& asset, Ref<T> defaultAsset, FileType type)
{
	bool edited = false;
	ImGui::TextUnformatted(label);

	std::string assetName;
	if (asset && !asset->GetFilepath().empty())
		assetName = asset->GetFilepath().filename().string();

	if (assetName.empty())
		assetName = "Default";

	std::string comboId = std::string("##Combo") + label;

	if (BeginAssetCombo(comboId.c_str(), assetName, asset.get()))
	{
		for (std::filesystem::path& file : Directory::GetFilesRecursive(Application::GetOpenDocumentDirectory(), ViewerManager::GetExtensions(type)))
		{
			auto relativePath = FileUtils::RelativePath(file, Application::GetOpenDocumentDirectory());
			ImGui::PushID(relativePath.string().c_str());
			bool clicked = ImGui::Selectable(file.filename().string().c_str());
			ImGui::Tooltip(relativePath.string().c_str());
			ImGui::PopID();
			if (clicked)
			{
				asset = AssetManager::GetAsset<T>(relativePath);
				edited = true;
				break;
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_AcceptPeekOnly))
		{
			std::filesystem::path* file = (std::filesystem::path*)payload->Data;

			for (std::string& ext : ViewerManager::GetExtensions(type))
			{
				if (file->extension().string() == ext)
				{
					if (ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_None))
					{
						auto relativePath = FileUtils::RelativePath(*file, Application::GetOpenDocumentDirectory());
						asset = AssetManager::GetAsset<T>(relativePath);
					}
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (assetName != "Default")
	{
		ImGui::SameLine();
		std::string editId = std::string(ICON_FA_PEN_TO_SQUARE "##EditAsset") + label;
		if (ImGui::Button(editId.c_str()))
		{
			ViewerManager::OpenViewer(asset->GetFilepath());
		}
		ImGui::Tooltip("Open Asset Viewer");

		ImGui::SameLine();
		std::string clearId = std::string(ICON_FA_XMARK "##ClearAsset") + label;
		if (ImGui::Button(clearId.c_str()))
		{
			asset = defaultAsset;
		}
	}
	return edited;
}
}

