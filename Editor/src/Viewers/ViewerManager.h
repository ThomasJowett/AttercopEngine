#pragma once

#ifdef DEBUG
#define MAX_ASSET_VIEWERS 0
#else
#define MAX_ASSET_VIEWERS 10
#endif

#include <map>
#include <filesystem>
#include <optional>

#include "Core/Layer.h"

enum class FileType
{
	UNKNOWN,
	TEXT,
	IMAGE,
	MESH,
	SCENE,
	SCRIPT,
	AUDIO,
	MATERIAL,
	TILESET,
	SPRITESHEET,
	PHYSICSMATERIAL,
	FONT,
	BEHAVIOURTREE,
	STATEMACHINE
};

class View : public Layer
{
public:
	View(std::string name)
		:Layer(name) {}
	virtual ~View() = default;
	const std::string& GetWindowName() { return m_WindowName; }
protected:
	std::string m_WindowName;
};

class ViewerManager
{
public:

	static void OpenViewer(const std::filesystem::path& assetPath);
	// Opens a text file with the cursor on the given line, starting at 1, marked as an error if there's a message
	static void OpenViewer(const std::filesystem::path& assetPath, int line, const std::string& errorMessage = {});
	static void CloseViewer(const std::filesystem::path& assetPath);
	static void CloseAll();

	// The text of an open script with unsaved edits, so searches can match what's on screen
	static std::optional<std::string> GetUnsavedText(const std::filesystem::path& assetPath);

	static FileType GetFileType(const std::filesystem::path& assetPath);
	static const char* GetIcon(FileType fileType);
	static const char* GetTypeName(FileType fileType);

	static std::vector<std::string> GetExtensions(FileType fileType);
	static std::vector<std::string> GetAllExtensions();

	static void SaveAll();
};

