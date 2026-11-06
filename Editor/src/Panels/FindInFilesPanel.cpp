#include "FindInFilesPanel.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>

#include "IconsFontAwesome6.h"
#include "imgui/imgui.h"

#include "Core/Application.h"
#include "Events/ApplicationEvent.h"
#include "Logging/Instrumentor.h"
#include "Utilities/AssetReferenceUtils.h"
#include "Utilities/FileUtils.h"

#include "MainDockSpace.h"
#include "Viewers/ViewerManager.h"

bool FindInFilesPanel::s_FocusRequested = false;
std::filesystem::path FindInFilesPanel::s_PendingReferences;

namespace
{
	constexpr size_t s_MaxMatches = 2000;
	constexpr size_t s_MaxLineLength = 200;

	bool IsSearchable(const std::filesystem::path& filepath)
	{
		switch (ViewerManager::GetFileType(filepath))
		{
		case FileType::TEXT:
		case FileType::SCRIPT:
		case FileType::SCENE:
		case FileType::MATERIAL:
		case FileType::TILESET:
		case FileType::SPRITESHEET:
		case FileType::PHYSICSMATERIAL:
		case FileType::BEHAVIOURTREE:
		case FileType::STATEMACHINE:
			return true;
		default:
			return filepath.extension() == ".proj";
		}
	}

	bool IsWordCharacter(char character)
	{
		return std::isalnum((unsigned char)character) || character == '_';
	}

	std::string ToLower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return text;
	}

	// Calls visit with the project-relative path of every file, skipping hidden folders such as .git
	template<typename Visitor>
	void ForEachProjectFile(const Visitor& visit)
	{
		const std::filesystem::path& projectDirectory = Application::GetOpenDocumentDirectory();
		std::error_code errorCode;
		for (auto it = std::filesystem::recursive_directory_iterator(projectDirectory, errorCode);
			it != std::filesystem::recursive_directory_iterator(); it.increment(errorCode))
		{
			if (errorCode)
				break;

			if (it->is_directory(errorCode))
			{
				if (it->path().filename().string().rfind('.', 0) == 0)
					it.disable_recursion_pending();
				continue;
			}

			if (it->is_regular_file(errorCode) && !visit(FileUtils::RelativePath(it->path(), projectDirectory)))
				break;
		}
	}
}

FindInFilesPanel::FindInFilesPanel(bool* show)
	: m_Show(show), Layer("FindInFiles")
{
}

void FindInFilesPanel::OnEvent(Event& event)
{
	EventDispatcher dispatcher(event);
	dispatcher.Dispatch<AppOpenDocumentChangedEvent>([this](AppOpenDocumentChangedEvent&)
		{
			m_Results.clear();
			m_MatchCount = 0;
			m_Truncated = false;
			m_Summary.clear();
			return false;
		});
}

void FindInFilesPanel::OnImGuiRender()
{
	PROFILE_FUNCTION();

	if (!s_PendingReferences.empty())
	{
		FindReferences(s_PendingReferences);
		s_PendingReferences.clear();
		s_FocusRequested = true;
	}

	if (s_FocusRequested)
	{
		s_FocusRequested = false;
		*m_Show = true;
		m_FocusSearch = true;
		ImGui::SetNextWindowFocus();
	}

	if (!*m_Show)
		return;

	ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(ICON_FA_MAGNIFYING_GLASS" Find in Files", m_Show))
	{
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			MainDockSpace::SetFocussedWindow(this);

		if (m_FocusSearch)
		{
			ImGui::SetKeyboardFocusHere();
			m_FocusSearch = false;
		}

		ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Match case Whole word Search").x - ImGui::GetFrameHeight() * 2.0f - ImGui::GetStyle().ItemSpacing.x * 6.0f);
		bool search = ImGui::InputTextWithHint("##Find", "Find in project...", m_SearchBuffer, sizeof(m_SearchBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		ImGui::Checkbox("Match case", &m_MatchCase);
		ImGui::SameLine();
		ImGui::Checkbox("Whole word", &m_WholeWord);
		ImGui::SameLine();
		search |= ImGui::Button("Search");

		if (search)
		{
			FindText();
			m_FocusSearch = true;
		}

		if (!m_Summary.empty())
			ImGui::TextDisabled("%s", m_Summary.c_str());

		ImGui::Separator();

		if (ImGui::BeginChild("##Results"))
			DrawResults();
		ImGui::EndChild();
	}
	ImGui::End();
}

void FindInFilesPanel::FindText()
{
	PROFILE_FUNCTION();

	m_Results.clear();
	m_MatchCount = 0;
	m_Truncated = false;

	std::string query = m_SearchBuffer;
	if (query.empty())
	{
		m_Summary.clear();
		return;
	}

	ForEachProjectFile([&](const std::filesystem::path& filepath)
		{
			if (IsSearchable(filepath))
			{
				FileResult result{ filepath };
				SearchFile(filepath, result, query, m_MatchCase, m_WholeWord, false);
				AddResult(std::move(result));
			}
			return !m_Truncated;
		});

	m_Summary = std::to_string(m_MatchCount) + (m_MatchCount == 1 ? " match" : " matches") + " in "
		+ std::to_string(m_Results.size()) + (m_Results.size() == 1 ? " file" : " files")
		+ (m_Truncated ? " (stopped at " + std::to_string(s_MaxMatches) + ")" : "");
}

void FindInFilesPanel::FindReferences(const std::filesystem::path& assetPath)
{
	PROFILE_FUNCTION();

	m_Results.clear();
	m_MatchCount = 0;
	m_Truncated = false;

	std::string pathString = assetPath.generic_string();
	const std::filesystem::path& projectDirectory = Application::GetOpenDocumentDirectory();

	for (const std::filesystem::path& file : AssetReferenceUtils::FindReferences(assetPath))
	{
		FileResult result{ FileUtils::RelativePath(file, projectDirectory) };
		if (result.filepath.extension() != ".staticmesh")
			SearchFile(result.filepath, result, pathString, true, false, true);
		if (result.matches.empty())
			result.matches.push_back({ 0, "", 0, 0 });
		AddResult(std::move(result));
	}

	ForEachProjectFile([&](const std::filesystem::path& filepath)
		{
			if (ViewerManager::GetFileType(filepath) == FileType::SCRIPT)
			{
				FileResult result{ filepath };
				SearchFile(filepath, result, pathString, true, false, true);
				AddResult(std::move(result));
			}
			return !m_Truncated;
		});

	m_Summary = std::to_string(m_Results.size()) + (m_Results.size() == 1 ? " file references " : " files reference ") + pathString;
	std::snprintf(m_SearchBuffer, sizeof(m_SearchBuffer), "%s", pathString.c_str());
}

void FindInFilesPanel::SearchFile(const std::filesystem::path& filepath, FileResult& result,
	const std::string& query, bool matchCase, bool wholeWord, bool quotedPath)
{
	std::string text;
	if (std::optional<std::string> unsavedText = ViewerManager::GetUnsavedText(filepath))
	{
		text = std::move(*unsavedText);
	}
	else
	{
		std::ifstream file(Application::GetOpenDocumentDirectory() / filepath, std::ios::binary);
		if (!file)
			return;
		std::stringstream buffer;
		buffer << file.rdbuf();
		text = buffer.str();
	}

	std::string needle = matchCase ? query : ToLower(query);

	std::istringstream lines(text);
	std::string line;
	uint32_t lineNumber = 0;
	while (std::getline(lines, line))
	{
		lineNumber++;
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		std::string haystack = matchCase ? line : ToLower(line);

		for (size_t position = haystack.find(needle); position != std::string::npos; position = haystack.find(needle, position + 1))
		{
			size_t end = position + needle.size();
			char before = position > 0 ? line[position - 1] : '\0';
			char after = end < line.size() ? line[end] : '\0';

			if (wholeWord && (IsWordCharacter(before) || IsWordCharacter(after)))
				continue;
			// A path in a string: "Textures/Rope.png", or the start of one inside a referenced folder
			if (quotedPath && !((before == '"' || before == '\'') && (after == before || after == '/')))
				continue;

			// Trim leading whitespace and long lines, keeping the match in view
			size_t first = std::min(line.find_first_not_of(" \t"), position);
			if (position - first > s_MaxLineLength / 2)
				first = position - s_MaxLineLength / 2;
			std::string excerpt = line.substr(first, s_MaxLineLength);
			result.matches.push_back({ lineNumber, excerpt, position - first, std::min(needle.size(), excerpt.size() - (position - first)) });
			break;
		}
	}
}

void FindInFilesPanel::AddResult(FileResult&& result)
{
	if (result.matches.empty() || m_Truncated)
		return;

	m_MatchCount += result.matches.size();
	if (m_MatchCount >= s_MaxMatches)
	{
		result.matches.resize(result.matches.size() - (m_MatchCount - s_MaxMatches));
		m_MatchCount = s_MaxMatches;
		m_Truncated = true;
	}
	result.fileType = ViewerManager::GetFileType(result.filepath);
	m_Results.push_back(std::move(result));
}

void FindInFilesPanel::DrawResults()
{
	// Scripts and scenes first, then the other asset types
	static const FileType sectionOrder[] = {
		FileType::SCRIPT, FileType::SCENE, FileType::MATERIAL, FileType::PHYSICSMATERIAL, FileType::SPRITESHEET,
		FileType::TILESET, FileType::MESH, FileType::BEHAVIOURTREE, FileType::STATEMACHINE, FileType::IMAGE,
		FileType::AUDIO, FileType::FONT, FileType::TEXT, FileType::UNKNOWN };

	for (FileType fileType : sectionOrder)
	{
		size_t count = std::count_if(m_Results.begin(), m_Results.end(),
			[fileType](const FileResult& result) { return result.fileType == fileType; });
		if (count == 0)
			continue;

		std::string header = std::string(ViewerManager::GetIcon(fileType)) + " " + ViewerManager::GetTypeName(fileType)
			+ " (" + std::to_string(count) + ")###" + ViewerManager::GetTypeName(fileType);
		if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			continue;

		for (size_t i = 0; i < m_Results.size(); i++)
		{
			if (m_Results[i].fileType == fileType)
				DrawFileResult(i);
		}
	}
}

void FindInFilesPanel::DrawFileResult(size_t i)
{
	const FileResult& result = m_Results[i];
	ImGui::PushID((int)i);

	std::string label = result.filepath.filename().string();
	std::string folder = result.filepath.parent_path().generic_string();

	bool open = ImGui::TreeNodeEx("##File", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", label.c_str());
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		ViewerManager::OpenViewer(result.filepath);
	ImGui::SameLine();
	ImGui::TextDisabled("%s", folder.c_str());

	if (open)
	{
		for (size_t j = 0; j < result.matches.size(); j++)
		{
			const Match& match = result.matches[j];
			ImGui::PushID((int)j);

			if (ImGui::Selectable("##Match", false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowItemOverlap)
				&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				ViewerManager::OpenViewer(result.filepath, (int)match.lineNumber);
			}
			ImGui::SameLine();

			if (match.lineNumber == 0)
			{
				ImGui::TextDisabled("References it");
			}
			else
			{
				ImGui::TextDisabled("%u", match.lineNumber);
				ImGui::SameLine();

				const char* text = match.text.c_str();
				ImGui::TextUnformatted(text, text + match.start);
				ImGui::SameLine(0.0f, 0.0f);

				// Highlight the match with the selected text colour
				ImVec2 position = ImGui::GetCursorScreenPos();
				ImVec2 size = ImGui::CalcTextSize(text + match.start, text + match.start + match.length);
				ImGui::GetWindowDrawList()->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y), ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
				ImGui::TextUnformatted(text + match.start, text + match.start + match.length);
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextUnformatted(text + match.start + match.length, text + match.text.size());
			}

			ImGui::PopID();
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}
