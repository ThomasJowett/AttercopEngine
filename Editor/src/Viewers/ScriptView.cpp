#include "ScriptView.h"
#include "Fonts/Fonts.h"
#include "ImGui/ImGuiTextEditor.h"
#include "MainDockSpace.h"
#include "IconsFontAwesome6.h"

#include "FileSystem/FileDialog.h"
#include "FileSystem/ExternalEditor.h"
#include "Core/Settings.h"
#include "ViewerManager.h"
#include "Scripting/Lua/LuaManager.h"
#include "Scene/Entity.h"
#include "Scene/Components/LuaScriptComponent.h"
#include "Scene/AssetManager.h"
#include "imgui.h"
#include <cstddef>
#include <cstring>
#include <string>
#include <unordered_set>
#include <regex>
#include "FileSystem/WebProject.h"
#include "ImGui/ImGuiUtilities.h"

// Functions get their signature above the description, e.g. "FindEntity(string) -> Entity"
static std::string LuaDescription(const LuaApiEntry& entry)
{
	if (entry.kind != LuaApiEntry::Kind::Function && entry.kind != LuaApiEntry::Kind::ComponentAccessor)
		return entry.description;
	return entry.name + entry.FormatParameters() + (entry.type.empty() ? "" : " -> " + entry.type) + "\n" + entry.description;
}

ScriptView::ScriptView(bool* show, const std::filesystem::path& filepath)
	:View("ScriptView"), m_Show(show), m_FilePath(filepath)
{
}

void ScriptView::OnAttach()
{
	std::filesystem::path absolutePath = std::filesystem::absolute(Application::GetOpenDocumentDirectory() / m_FilePath);
	if (!std::filesystem::exists(absolutePath))
	{
		ViewerManager::CloseViewer(m_FilePath);
		return;
	}

	m_Script = AssetManager::GetAsset<LuaScript>(m_FilePath);
	if (!m_Script || m_Script->IsMissing())
	{
		*m_Show = false;
		return;
	}

	m_WindowName = ICON_FA_FILE_CODE + std::string(" " + m_FilePath.filename().string());

	TextEditor::LanguageDefinition lang = DetermineLanguageDefinition();

	if (lang.mName == "Lua")
	{
		const auto& identifiers = LuaManager::GetIdentifiers();
		for (const LuaApiEntry& entry : identifiers) {
			TextEditor::Identifier id;
			id.mDeclaration = LuaDescription(entry);
			lang.mIdentifiers.insert(std::make_pair(entry.name, id));

			if (entry.kind == LuaApiEntry::Kind::Global)
			{
				if (!entry.type.empty())
					m_LuaGlobalTypes[entry.name] = entry.type;
			}
			else if (entry.kind == LuaApiEntry::Kind::ComponentAccessor)
				m_LuaMembers["Entity"].push_back(entry);	// called on the entity, not the component
			else
				m_LuaMembers[entry.component].push_back(entry);

			(entry.component.empty() ? m_LuaGlobalWords : m_LuaMemberWords).push_back({ entry.name, id.mDeclaration });
		}

		// The built-in identifiers mix globals with library members such as math.floor
		static const std::unordered_set<std::string> builtInGlobals = {
			"assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "load", "loadfile", "loadstring", "next",
			"pairs", "pcall", "print", "rawequal", "rawget", "rawlen", "rawset", "require", "select", "setmetatable", "tonumber",
			"tostring", "type", "unpack", "xpcall", "_G", "_VERSION", "coroutine", "debug", "io", "math", "os", "package", "string",
			"table", "utf8", "bit32"
		};
		for (const auto& [name, id] : TextEditor::LanguageDefinition::Lua().mIdentifiers)
			(builtInGlobals.count(name) ? m_LuaGlobalWords : m_LuaMemberWords).push_back({ name, id.mDeclaration });
		for (const std::string& keyword : lang.mKeywords)
			if (!keyword.empty())
				m_LuaGlobalWords.push_back({ keyword, "" });

		m_TextEditor.SetAutoCompleteProvider([this](const std::string& lineBefore, const std::string& word, std::vector<TextEditor::AutoCompleteItem>& items)
			{
				return CompleteLua(lineBefore, word, items);
			});
	}

	m_TextEditor.SetLanguageDefinition(lang);

	std::ifstream file(absolutePath);

	if (file.good())
	{
		std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		m_TextEditor.SetText(str);
		m_TextEditor.SetFilePath(absolutePath);
	}
	Settings::SetDefaultBool("TextEditor", "ShowWhiteSpace", true);
	m_TextEditor.SetShowWhitespaces(Settings::GetBool("TextEditor", "ShowWhiteSpace"));

	if (m_FilePath.extension() == ".lua")
	{
		ParseLuaScript();
	}
}

void ScriptView::OnDetach()
{
}

void ScriptView::OnImGuiRender()
{
	if (!*m_Show)
	{
		if (m_TextEditor.NeedsSaving())
		{
			ImGui::OpenPopup(("Save Prompt " + m_WindowName).c_str());
		}

		if (ImGui::BeginPopupModal(("Save Prompt" + m_WindowName).c_str()))
		{
			ImGui::TextUnformatted("Save unsaved changes?");
			if (ImGui::Button("Save"))
			{
				Save();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Don't Save"))
			{
				m_TextEditor.DiscardAllChanges();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				*m_Show = true;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		return;
	}

	ImGui::CentreNextWindowOnFirstUse(ImVec2(800, 600));

	ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar;

	if (m_TextEditor.NeedsSaving())
		flags |= ImGuiWindowFlags_UnsavedDocument;

	if (ImGui::Begin(m_WindowName.c_str(), m_Show, flags))
	{
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			MainDockSpace::SetFocussedWindow(this);

			bool ctrl = ImGui::GetIO().ConfigMacOSXBehaviors ? ImGui::GetIO().KeySuper : ImGui::GetIO().KeyCtrl;
			bool shift = ImGui::GetIO().KeyShift;

			if (ctrl && ImGui::IsKeyPressed(ImGuiKey_F))
			{
				m_ShowSearch = true;
				m_FocusSearch = true;
			}
			else if (!m_TextEditor.IsAutoCompleteOpen() && ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				m_ShowSearch = false;
			}
			else if (m_ShowSearch && !m_TextEditor.IsAutoCompleteOpen() && ImGui::IsKeyPressed(ImGuiKey_Enter))
			{
				if (shift)
					FindPrevious();
				else
				 	FindNext();
			}
		}

		bool readOnly = m_TextEditor.IsReadOnly();

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK" Save", "Ctrl + S", nullptr, !readOnly))
					Save();
				if (ImGui::MenuItem(ICON_FA_FILE_SIGNATURE" Save As", "Ctrl + Shift + S"))
					SaveAs();
				ImGui::Separator();//---------------------------------------------------------------
				ExternalEditor::MenuItem(std::filesystem::absolute(Application::GetOpenDocumentDirectory() / m_FilePath));
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem(ICON_FA_ARROW_ROTATE_LEFT" Undo", "Ctrl-Z", nullptr, !readOnly && m_TextEditor.CanUndo()))
					m_TextEditor.Undo();
				if (ImGui::MenuItem(ICON_FA_ARROW_ROTATE_RIGHT" Redo", "Ctrl-Y", nullptr, !readOnly && m_TextEditor.CanRedo()))
					m_TextEditor.Redo();
				ImGui::Separator();//---------------------------------------------------------------

				if (ImGui::MenuItem(ICON_FA_SCISSORS" Cut", "Ctrl-X", nullptr, m_TextEditor.HasSelection() && !readOnly))
					m_TextEditor.Cut();
				if (ImGui::MenuItem(ICON_FA_COPY" Copy", "Ctrl-C", nullptr, m_TextEditor.HasSelection()))
					m_TextEditor.Copy();
				if (ImGui::MenuItem(ICON_FA_PASTE" Paste", "Ctrl-V", nullptr, ImGui::GetClipboardText() != nullptr && !readOnly))
					m_TextEditor.Paste();
				if (ImGui::MenuItem(ICON_FA_CLONE" Duplicate", "Ctrl-D", nullptr, !readOnly))
					m_TextEditor.Duplicate();
				if (ImGui::MenuItem(ICON_FA_TRASH_CAN" Delete", "Del", nullptr, m_TextEditor.HasSelection() && !readOnly))
					m_TextEditor.Delete();
				ImGui::Separator();//---------------------------------------------------------------
				if (ImGui::MenuItem(ICON_FA_MAGNIFYING_GLASS " Find", "Ctrl-F"))
				{
					m_ShowSearch = true;
					m_FocusSearch = true;
				}
				ImGui::Separator();//---------------------------------------------------------------
				if (ImGui::MenuItem(ICON_FA_ARROW_POINTER" Select all", "Ctrl-A", nullptr))
					m_TextEditor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(m_TextEditor.GetTotalLines(), 0));
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				bool showWhiteSpace = m_TextEditor.IsShowingWhitespaces();
				if (ImGui::MenuItem("Show white Space", "", &showWhiteSpace))
				{
					m_TextEditor.SetShowWhitespaces(showWhiteSpace);
					Settings::SetBool("TextEditor", "ShowWhiteSpace", showWhiteSpace);
				}

				bool colourize = m_TextEditor.IsColorizerEnabled();
				if (ImGui::MenuItem("Colourize Text", "", &colourize))
				{
					m_TextEditor.SetColorizerEnable(colourize);
				}
				ImGui::EndMenu();
			}
		}
		ImGui::EndMenuBar();

		if (m_ShowSearch)
		{
			if (m_FocusSearch)
			{
				ImGui::SetKeyboardFocusHere();
				m_FocusSearch = false;
			}

			ImGui::PushItemWidth(300.0f);
			if (ImGui::InputTextWithHint("##SearchBox", "Find...", m_SearchBuffer, 256, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				FindNext();
			}
			ImGui::PopItemWidth();

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_ARROW_UP))
			{
				FindPrevious();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Find Previous (Shift+Enter)");

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_ARROW_DOWN))
			{
				FindNext();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Find Next (Enter)");

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_XMARK))
			{
				m_ShowSearch = false;
			}

			ImGui::Separator();
		}

		ImGui::PushFont(Fonts::Consolas);
		if (m_GoToLine > 0 && m_TextEditor.GetTotalLines() > 0)
		{
			int line = std::min(m_GoToLine, m_TextEditor.GetTotalLines()) - 1;
			m_TextEditor.SetSelection(TextEditor::Coordinates(line, 0), TextEditor::Coordinates(line + 1, 0));
			m_TextEditor.SetCursorPosition(TextEditor::Coordinates(line, 0));
			if (!m_GoToLineError.empty())
				m_TextEditor.SetErrorMarkers({ { line + 1, m_GoToLineError } });
			m_GoToLine = 0;
		}
		m_TextEditor.Render("TextEditor");
		ImGui::PopFont();

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			MainDockSpace::SetFocussedWindow(this);
		}
	}

	ImGui::End();
}

void ScriptView::OnEvent(Event& event)
{
	PROFILE_FUNCTION();
	EventDispatcher dispatcher(event);
	dispatcher.Dispatch<LuaErrorEvent>([this](LuaErrorEvent& luaErrorEvent)
		{
			if (m_Script && luaErrorEvent.GetFile() == m_Script->GetFilepath()) {
				TextEditor::ErrorMarkers errorMarkers;
				errorMarkers.emplace(luaErrorEvent.GetLine(), luaErrorEvent.GetErrorMessage());
				m_TextEditor.SetErrorMarkers(errorMarkers);
			}
			return false;
		});
}

void ScriptView::Save()
{
	if (!IsReadOnly()) {
		std::filesystem::path absolutePath = std::filesystem::absolute(Application::GetOpenDocumentDirectory() / m_FilePath);
		m_TextEditor.SaveTextToFile(absolutePath);
	}

	if (m_FilePath.extension() == ".lua")
	{
		ParseLuaScript();
	}

	m_Script->Load(m_FilePath);

	WebProject::Sync();
}

void ScriptView::SaveAs()
{
	auto ext = m_FilePath.extension();
	std::optional<std::wstring> dialogPath = FileDialog::SaveAs(L"Save As...", { {L"Script File", std::wstring(L"*") + ConvertToWideChar(m_FilePath.extension().string())} });
	if (dialogPath)
	{
		m_FilePath = dialogPath.value();
		if (!m_FilePath.has_extension())
			m_FilePath.replace_extension(ext);
		Save();
	}
}

TextEditor::LanguageDefinition ScriptView::DetermineLanguageDefinition()
{
	auto ext = m_FilePath.extension();

	if (ext == ".lua")
		return TextEditor::LanguageDefinition::Lua();
	else if (ext == ".cpp" || ext == ".hpp")
		return TextEditor::LanguageDefinition::CPlusPlus();
	else if (ext == ".h" || ext == ".c" || ext == ".cs")
		return TextEditor::LanguageDefinition::C();
	else if (ext == ".hlsl" || ext == ".fx")
		return TextEditor::LanguageDefinition::HLSL();
	else if (ext == ".glsl" || ext == ".frag" || ext == ".vert" || ext == ".tesc" || ext == ".tese" || ext == ".geom" || ext == ".comp")
		return TextEditor::LanguageDefinition::GLSL();
	else if (ext == ".sql")
		return TextEditor::LanguageDefinition::SQL();
	else if (ext == ".as")
		return TextEditor::LanguageDefinition::AngelScript();

	return TextEditor::LanguageDefinition();
}

void ScriptView::ParseLuaScript()
{
	TextEditor::ErrorMarkers errorMarkers;
	m_TextEditor.SetErrorMarkers(errorMarkers);
	Scene testScene("");
	
	Entity entity = testScene.CreateEntity("lua script");
	
	LuaScriptComponent& luaComp = entity.AddComponent<LuaScriptComponent>(m_FilePath);
	luaComp.ParseScript(entity);
}

// Globals at the start of an expression, members after a "." or ":"
bool ScriptView::CompleteLua(const std::string& lineBefore, const std::string& word, std::vector<TextEditor::AutoCompleteItem>& items) const
{
	bool afterSeparator = !lineBefore.empty() && (lineBefore.back() == '.' || lineBefore.back() == ':');
	if (afterSeparator && CompleteLuaMember(lineBefore, items))
		return true;

	// Nothing typed yet and nothing known to list, e.g. after a number or an unknown local
	if (word.empty())
		return false;

	items = afterSeparator ? m_LuaMemberWords : m_LuaGlobalWords;
	return true;
}

static bool IsLuaWordChar(char c)
{
	return std::isalnum((unsigned char)c) || c == '_';
}

// Reads back the chain ending just before end, e.g. CurrentEntity:GetRigidBody2DComponent(); begin is where it starts
bool ScriptView::ParseLuaChain(const std::string& text, size_t end, std::vector<LuaStep>& steps, size_t& begin)
{
	while (true)
	{
		bool call = false;
		if (end > 0 && text[end - 1] == ')')
		{
			int depth = 0;
			size_t i = end;
			while (i > 0)
			{
				char c = text[--i];
				if (c == ')')
					depth++;
				else if (c == '(' && --depth == 0)
					break;
			}
			if (depth != 0)
				return false;
			end = i;
			call = true;
		}

		begin = end;
		while (begin > 0 && IsLuaWordChar(text[begin - 1]))
			--begin;
		if (begin == end || std::isdigit((unsigned char)text[begin]))
			return false;

		steps.insert(steps.begin(), { text.substr(begin, end - begin), call });

		if (begin > 0 && (text[begin - 1] == '.' || text[begin - 1] == ':'))
			end = begin - 1;
		else
			return true;
	}
}

// The Lua type a chain evaluates to, or empty if a step isn't known; names that aren't globals are looked up as variables above line
std::string ScriptView::ResolveLuaType(const std::vector<LuaStep>& steps, const std::vector<std::string>& lines, int line, int depth) const
{
	std::string type;
	if (steps[0].call)
		return "";
	if (auto it = m_LuaGlobalTypes.find(steps[0].name); it != m_LuaGlobalTypes.end())
		type = it->second;
	else if (m_LuaMembers.count(steps[0].name))
		type = steps[0].name;
	else
		type = FindLuaVariableType(steps[0].name, lines, line, depth + 1);

	for (size_t i = 1; i < steps.size() && !type.empty(); ++i)
	{
		auto members = m_LuaMembers.find(type);
		if (members == m_LuaMembers.end())
			return "";

		auto member = std::find_if(members->second.begin(), members->second.end(), [&](const LuaApiEntry& entry) { return entry.name == steps[i].name; });
		if (member == members->second.end())
			return "";

		bool isFunction = member->kind == LuaApiEntry::Kind::Function || member->kind == LuaApiEntry::Kind::ComponentAccessor;
		if (isFunction != steps[i].call)
			return "";

		// Constructors such as Vec2.new don't record a return type
		type = member->type.empty() && member->name == "new" ? type : member->type;
	}
	return type;
}

// The type of the nearest assignment to name above line, e.g. "local rb = CurrentEntity:GetRigidBody2DComponent()"
std::string ScriptView::FindLuaVariableType(const std::string& name, const std::vector<std::string>& lines, int line, int depth) const
{
	if (depth > 8)
		return "";

	const std::regex assignment("(^|[^\\w.:])(local\\s+)?" + name + "\\s*=(?!=)\\s*(.*)$");
	for (int i = std::min(line, (int)lines.size()) - 1; i >= 0; --i)
	{
		std::string text = lines[i].substr(0, lines[i].find("--"));
		std::smatch match;
		if (!std::regex_search(text, match, assignment))
			continue;

		std::string value = match[3].str();
		while (!value.empty() && (std::isspace((unsigned char)value.back()) || value.back() == ';'))
			value.pop_back();

		std::vector<LuaStep> steps;
		size_t begin = 0;
		if (value.empty() || !ParseLuaChain(value, value.size(), steps, begin) || begin != 0)
			return "";
		return ResolveLuaType(steps, lines, i, depth);
	}
	return "";
}

// Lists the members after "a.b:c()." when each step's type is known from the Lua API entries or a variable assigned above
bool ScriptView::CompleteLuaMember(const std::string& lineBefore, std::vector<TextEditor::AutoCompleteItem>& items) const
{
	if (lineBefore.empty() || (lineBefore.back() != '.' && lineBefore.back() != ':'))
		return false;
	const bool methodCall = lineBefore.back() == ':';

	std::vector<LuaStep> steps;
	size_t begin = 0;
	if (!ParseLuaChain(lineBefore, lineBefore.size() - 1, steps, begin))
		return false;

	std::string type = ResolveLuaType(steps, m_TextEditor.GetTextLines(), m_TextEditor.GetCursorPosition().mLine, 0);
	auto members = m_LuaMembers.find(type);
	if (members == m_LuaMembers.end())
		return false;

	for (const LuaApiEntry& entry : members->second)
	{
		bool isFunction = entry.kind == LuaApiEntry::Kind::Function || entry.kind == LuaApiEntry::Kind::ComponentAccessor;
		if (!methodCall || isFunction)
			items.push_back({ entry.name, LuaDescription(entry) });
	}
	return !items.empty();
}

void ScriptView::FindNext() {
	if (strlen(m_SearchBuffer) == 0)
		return;

	std::string searchStr = m_SearchBuffer;
	auto lines = m_TextEditor.GetTextLines();
	auto cursor = m_TextEditor.GetCursorPosition();

	for (size_t i = cursor.mLine; i < lines.size(); ++i)
	{
		size_t startPos = (i == cursor.mLine) ? cursor.mColumn : 0;
		size_t pos = lines[i].find(searchStr, startPos);

		if (pos != std::string::npos)
		{
			m_TextEditor.SetSelection(TextEditor::Coordinates((int)i, (int)pos), TextEditor::Coordinates((int)i, (int)(pos + searchStr.length())));
			m_TextEditor.SetCursorPosition(TextEditor::Coordinates((int)i, (int)(pos + searchStr.length())));
			return;
		}
	}

	// wrap around
	for (size_t i = 0; i <= cursor.mLine && i < lines.size(); ++i)
	{
		size_t pos = lines[i].find(searchStr, 0);

		if (pos != std::string::npos)
		{
			m_TextEditor.SetSelection(TextEditor::Coordinates((int)i, (int)pos), TextEditor::Coordinates((int)i, (int)(pos + searchStr.length())));
			m_TextEditor.SetCursorPosition(TextEditor::Coordinates((int)i, (int)(pos + searchStr.length())));
			return;
		}
	}
}

void ScriptView::FindPrevious() {
	if (strlen(m_SearchBuffer) == 0)
		return;

	std::string searchStr = m_SearchBuffer;
	auto lines = m_TextEditor.GetTextLines();
	auto cursor = m_TextEditor.GetCursorPosition();

	if (m_TextEditor.HasSelection())
	{
		cursor.mColumn -= (int)searchStr.length();
		if (cursor.mColumn < 0)
			cursor.mColumn = 0;
	}

	if (cursor.mLine < lines.size())
	{
		std::string linePrefix = lines[cursor.mLine].substr(0, std::max(0, cursor.mColumn));
		size_t pos = linePrefix.rfind(searchStr);
		if (pos != std::string::npos)
		{
			m_TextEditor.SetSelection(TextEditor::Coordinates(cursor.mLine, (int)pos), TextEditor::Coordinates(cursor.mLine, (int)(pos + searchStr.length())));
			m_TextEditor.SetCursorPosition(TextEditor::Coordinates(cursor.mLine, (int)(pos + searchStr.length())));
			return;
		}
	}

	for (int i = cursor.mLine - 1; i >= 0; --i)
	{
		size_t pos = lines[i].rfind(searchStr);
		if (pos != std::string::npos)
		{
			m_TextEditor.SetSelection(TextEditor::Coordinates((int)i, (int)pos), TextEditor::Coordinates((int)i, (int)(pos + searchStr.length())));
			m_TextEditor.SetCursorPosition(TextEditor::Coordinates((int)i, (int)(pos + searchStr.length())));
			return;
		}
	}

	// Wrap around to end
	for (int i = (int)lines.size() - 1; i >= cursor.mLine; --i)
	{
		size_t pos = lines[i].rfind(searchStr);
		if (pos != std::string::npos)
		{
			m_TextEditor.SetSelection(TextEditor::Coordinates((int)i, (int)pos), TextEditor::Coordinates((int)i, (int)(pos + searchStr.length())));
			m_TextEditor.SetCursorPosition(TextEditor::Coordinates((int)i, (int)(pos + searchStr.length())));
			return;
		}
	}
}
