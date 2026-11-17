#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace tinyxml2
{
class XMLDocument;
class XMLElement;
}

// Upgrades scene, state machine and project files from older formats, see design/project-migration.md
namespace Migration
{
	enum class FileType { Scene, StateMachine, Project };

	// What an upgrade did
	struct Report
	{
		std::vector<std::string> steps;	// descriptions of the steps applied, in order
		std::vector<std::string> warnings;
		std::vector<std::string> errors;
	};

	struct Step
	{
		FileType type;
		int from;	// upgrades from this version to from + 1
		const char* description;	// listed in the console, e.g. "Enums saved by name"
		bool (*apply)(tinyxml2::XMLDocument& document, Report& report);
	};

	// Each type keeps only the step from its previous format, so older files may not upgrade correctly
	struct StepList
	{
		int sceneVersion = 1;	// the current scene FormatVersion
		int stateMachineVersion = 1;
		std::vector<Step> steps;	// scene steps first, each type's in version order
	};

	const StepList& Steps();

	// A project's is its scene and state machine versions added, less 1, so it goes up with either
	int CurrentVersion(FileType type, const StepList& steps = Steps());

	// The root's FormatVersion, 1 when it has none
	int GetVersion(const tinyxml2::XMLElement* root);

	// Sets FormatVersion after EngineVersion when there is one, otherwise last
	void SetVersion(tinyxml2::XMLElement* root, int version);

	enum class Result { UpToDate, Upgraded, TooNew, Failed };

	// Runs every step from the document's version to the current one, in place. Scene documents have a
	// <Scene> root, or an <Entity> root when pasted. A document more than one format behind gets a warning
	// and the steps that are kept. Warnings and errors are logged naming the file.
	Result Upgrade(FileType type, tinyxml2::XMLDocument& document, const std::filesystem::path& file, Report& report,
		const StepList& steps = Steps());

	struct ProjectReport
	{
		struct File
		{
			std::filesystem::path path;	// relative to the project folder
			std::vector<std::string> steps;	// the steps applied, or why it failed
			std::vector<std::string> warnings;
		};

		std::vector<File> upgraded;
		std::vector<File> failed;
		bool projectRaised = false;	// the .proj FormatVersion was raised

		bool Succeeded() const { return failed.empty(); }
	};

	// Upgrades and writes back every .scene and .statemachine file in the project, then raises the .proj
	// FormatVersion if none failed. Does nothing when the .proj is current.
	ProjectReport UpgradeProject(const std::filesystem::path& projectFile, const StepList& steps = Steps());
}
