#pragma once

#include <filesystem>
#include <optional>
#include "Core/Layer.h"

struct Error
{
	std::filesystem::path filepath;
	uint32_t lineNumber;
	std::string message;

	Error(const std::filesystem::path& path, uint32_t lineNumber, const std::string& message)
		:filepath(path), lineNumber(lineNumber), message(message) {}
};

class ErrorListPanel :
	public Layer
{
public:
	explicit ErrorListPanel(bool* show);
	~ErrorListPanel() = default;

	void OnAttach() override;
	void OnUpdate(float deltaTime) override;
	void OnImGuiRender() override;

	void OnEvent(Event& event) override;

	bool AddError(Error& error);
	void ClearAllErrors();
private:
	void CopyErrors(bool selectedOnly);

private:
	bool* m_Show;

	std::vector<std::pair<Error, bool>> m_ErrorList;
	uint32_t m_NumberSelected = 0;

	// Index of the most recent error, highlighted until a row is clicked
	std::optional<size_t> m_NewestError;
	double m_NewestErrorTime = 0.0;
	bool m_ScrollToNewest = false;
	bool m_FocusRequested = false;
	bool m_PauseRequested = false;
};
