#include "ErrorListPanel.h"
#include "IconsFontAwesome6.h"
#include "imgui/imgui.h"

#include "Scripting/Lua/LuaManager.h"
#include "Scene/SceneManager.h"
#include "Scene/Components.h"
#include "Logging/Instrumentor.h"
#include "Core/Settings.h"

#include "MainDockSpace.h"
#include "Viewers/ViewerManager.h"
#include <Events/SceneEvent.h>

ErrorListPanel::ErrorListPanel(bool* show)
	: m_Show(show), Layer("ErrorList")
{
}

void ErrorListPanel::OnAttach()
{
}

void ErrorListPanel::OnUpdate(float deltaTime)
{
}

void ErrorListPanel::OnImGuiRender()
{
	PROFILE_FUNCTION();

	if (m_PauseRequested)
	{
		m_PauseRequested = false;
		if (SceneManager::GetSceneState() == SceneState::Play)
			SceneManager::ChangeSceneState(SceneState::Pause);
		else if (SceneManager::GetSceneState() == SceneState::Simulate)
			SceneManager::ChangeSceneState(SceneState::SimulatePause);
	}

	if (m_FocusRequested)
	{
		m_FocusRequested = false;
		*m_Show = true;
		ImGui::SetNextWindowFocus();
	}

	if (!*m_Show)
		return;

	ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(70, 50), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(ICON_FA_CIRCLE_XMARK" Error List", m_Show))
	{
		if (ImGui::IsWindowFocused())
		{
			MainDockSpace::SetFocussedWindow(this);

			if (m_NumberSelected > 0 && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C))
				CopyErrors(true);
		}

		ImGuiTableFlags table_flags = 
			ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
			| ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
			| ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoHostExtendY;

		ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns
			| ImGuiSelectableFlags_AllowItemOverlap
			| ImGuiSelectableFlags_AllowDoubleClick;

		if (ImGui::BeginTable("Error List", 3, table_flags))
		{
			ImGui::TableSetupColumn("Message");
			ImGui::TableSetupColumn("File");
			ImGui::TableSetupColumn("Line");
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < m_ErrorList.size(); i++)
			{
				auto& [error, selected] = m_ErrorList[i];
				// Errors can share a message, so give each row its own ID
				ImGui::PushID((int)i);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				if (m_NewestError == i)
				{
					// Pulses for the first few seconds, then stays tinted
					float t = (float)(ImGui::GetTime() - m_NewestErrorTime);
					float alpha = t < 3.0f ? 0.35f + 0.25f * cosf(t * 6.0f) : 0.35f;
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(ImVec4(0.9f, 0.1f, 0.1f, alpha)));
				}

				if (ImGui::Selectable(error.message.c_str(), selected, selectable_flags))
				{
					m_NewestError.reset();
					if (!ImGui::GetIO().KeyCtrl)
					{
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						{
							ViewerManager::OpenViewer(error.filepath, error.lineNumber, error.message);
						}

						for (auto&& [_, s] : m_ErrorList)
						{
							s = false;
						}
						selected = true;

						m_NumberSelected = 1;
					}
					else
					{
						m_NumberSelected -= selected;
						selected = !selected;
						m_NumberSelected += selected;

						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						{
							for (auto&& [e, s] : m_ErrorList)
							{
								if (s)
									ViewerManager::OpenViewer(e.filepath, e.lineNumber, e.message);
							}
						}
					}
				}

				if (ImGui::BeginPopupContextItem("ErrorContext"))
				{
					if (!selected)
					{
						for (auto&& [_, s] : m_ErrorList)
							s = false;
						selected = true;
						m_NumberSelected = 1;
					}
					if (ImGui::MenuItem(m_NumberSelected > 1 ? "Copy Errors" : "Copy Error"))
						CopyErrors(true);
					if (ImGui::MenuItem("Copy All Errors"))
						CopyErrors(false);
					ImGui::EndPopup();
				}

				if (m_NewestError == i && m_ScrollToNewest)
				{
					ImGui::SetScrollHereY();
					m_ScrollToNewest = false;
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(error.filepath.filename().string().c_str());
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%i", error.lineNumber);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void ErrorListPanel::OnEvent(Event& event)
{
	PROFILE_FUNCTION();
	EventDispatcher dispatcher(event);
	dispatcher.Dispatch<LuaErrorEvent>([this](LuaErrorEvent& luaErrorEvent)
		{
			auto error = Error(luaErrorEvent.GetFile(), luaErrorEvent.GetLine(), luaErrorEvent.GetErrorMessage());
			if (AddError(error))
			{
				m_FocusRequested = true;
				SceneState state = SceneManager::GetSceneState();
				if ((state == SceneState::Play || state == SceneState::Simulate)
					&& Settings::GetBool("Editor", "Pause_On_Script_Error"))
					m_PauseRequested = true;
			}
			return false;
		});
	dispatcher.Dispatch<SceneStateChangedEvent>([this](SceneStateChangedEvent& sceneStateChangedEvent)
		{
			if (sceneStateChangedEvent.GetSceneState() == SceneState::Play
				|| sceneStateChangedEvent.GetSceneState() == SceneState::Simulate)
			{
				ClearAllErrors();
			}
			return false;
		});
}

bool ErrorListPanel::AddError(Error& error)
{
	auto it = std::find_if(m_ErrorList.begin(), m_ErrorList.end(),
		[&error](const std::pair<Error, bool>& e) { return e.first.filepath == error.filepath && e.first.lineNumber == error.lineNumber; });
	if (it != m_ErrorList.end())
		return false;

	m_ErrorList.push_back(std::make_pair(error, false));
	m_NewestError = m_ErrorList.size() - 1;
	m_NewestErrorTime = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
	m_ScrollToNewest = true;
	return true;
}

void ErrorListPanel::ClearAllErrors()
{
	m_ErrorList.clear();
	m_NumberSelected = 0;
	m_NewestError.reset();
}

void ErrorListPanel::CopyErrors(bool selectedOnly)
{
	std::string text;
	for (auto&& [error, selected] : m_ErrorList)
	{
		if (selectedOnly && !selected)
			continue;
		text += error.filepath.filename().string() + ":" + std::to_string(error.lineNumber) + ": " + error.message + "\n";
	}
	ImGui::SetClipboardText(text.c_str());
}
