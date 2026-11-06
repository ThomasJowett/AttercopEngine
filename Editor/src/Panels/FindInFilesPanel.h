#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Core/Layer.h"
#include "Viewers/ViewerManager.h"

class FindInFilesPanel :
	public Layer
{
public:
	explicit FindInFilesPanel(bool* show);
	~FindInFilesPanel() = default;

	void OnImGuiRender() override;
	void OnEvent(Event& event) override;

	static void RequestFocus() { s_FocusRequested = true; }
	// Lists the files that reference a project-relative asset path or folder
	static void RequestFindReferences(const std::filesystem::path& assetPath) { s_PendingReferences = assetPath; }

private:
	struct Match
	{
		uint32_t lineNumber; // 0 when the file references the asset but no line could be found
		std::string text;
		size_t start;
		size_t length;
	};

	struct FileResult
	{
		std::filesystem::path filepath; // relative to the project
		std::vector<Match> matches;
		FileType fileType = FileType::UNKNOWN;
	};

	void FindText();
	void FindReferences(const std::filesystem::path& assetPath);

	void SearchFile(const std::filesystem::path& filepath, FileResult& result,
		const std::string& query, bool matchCase, bool wholeWord, bool quotedPath);
	void AddResult(FileResult&& result);

	void DrawResults();
	void DrawFileResult(size_t i);

private:
	bool* m_Show;

	char m_SearchBuffer[256] = { 0 };
	bool m_MatchCase = false;
	bool m_WholeWord = false;
	bool m_FocusSearch = false;

	std::vector<FileResult> m_Results;
	size_t m_MatchCount = 0;
	bool m_Truncated = false;
	std::string m_Summary;

	static bool s_FocusRequested;
	static std::filesystem::path s_PendingReferences;
};
