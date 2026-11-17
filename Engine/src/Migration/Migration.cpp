#include "Migration/Migration.h"
#include "Migration/MigrationSteps.h"

#include "Core/Version.h"
#include "Logging/Logger.h"

#include "TinyXml2/tinyxml2.h"

#include <algorithm>

namespace Migration
{
	namespace
	{
		Result Fail(Report& report, const std::filesystem::path& file, const std::string& reason)
		{
			report.errors.push_back(reason);
			ENGINE_ERROR("Could not upgrade {0}: {1}", file.string(), reason);
			return Result::Failed;
		}

		std::string Join(const std::vector<std::string>& items)
		{
			std::string joined;
			for (const std::string& item : items)
				joined += (joined.empty() ? "" : ", ") + item;
			return joined;
		}

		// Every .scene and .statemachine below the folder, skipping hidden folders, in path order
		std::vector<std::filesystem::path> FindFiles(const std::filesystem::path& directory)
		{
			std::vector<std::filesystem::path> files;
			std::error_code error;
			std::filesystem::recursive_directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, error);
			for (; !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
			{
				const std::filesystem::path& path = it->path();
				if (it->is_directory(error))
				{
					if (path.filename().string().rfind('.', 0) == 0)
						it.disable_recursion_pending();
				}
				else if (path.extension() == ".scene" || path.extension() == ".statemachine")
					files.push_back(path);
			}
			std::sort(files.begin(), files.end());
			return files;
		}
	}

	/* -------------------------------------------------------------------------------------------------------------- */

	const StepList& Steps()
	{
		static const StepList s_Steps = []()
			{
				StepList list{ k_SceneVersion, k_StateMachineVersion, SceneSteps() };
				std::vector<Step> stateMachineSteps = StateMachineSteps();
				list.steps.insert(list.steps.end(), stateMachineSteps.begin(), stateMachineSteps.end());
				return list;
			}();
		return s_Steps;
	}

	int CurrentVersion(FileType type, const StepList& steps)
	{
		switch (type)
		{
		case FileType::Scene: return steps.sceneVersion;
		case FileType::StateMachine: return steps.stateMachineVersion;
		case FileType::Project: return steps.sceneVersion + steps.stateMachineVersion - 1;
		}
		return 1;
	}

	int GetVersion(const tinyxml2::XMLElement* root)
	{
		return root->IntAttribute("FormatVersion", 1);
	}

	void SetVersion(tinyxml2::XMLElement* root, int version)
	{
		const tinyxml2::XMLAttribute* engineVersion = root->FindAttribute("EngineVersion");
		if (root->FindAttribute("FormatVersion") || !engineVersion)
		{
			root->SetAttribute("FormatVersion", version);
			return;
		}

		// tinyxml2 adds attributes last, so the ones after EngineVersion are added again after FormatVersion
		std::vector<std::pair<std::string, std::string>> after;
		for (const tinyxml2::XMLAttribute* attribute = engineVersion->Next(); attribute; attribute = attribute->Next())
			after.emplace_back(attribute->Name(), attribute->Value());
		for (const auto& [name, value] : after)
			root->DeleteAttribute(name.c_str());
		root->SetAttribute("FormatVersion", version);
		for (const auto& [name, value] : after)
			root->SetAttribute(name.c_str(), value.c_str());
	}

	/* -------------------------------------------------------------------------------------------------------------- */

	Result Upgrade(FileType type, tinyxml2::XMLDocument& document, const std::filesystem::path& file, Report& report,
		const StepList& steps)
	{
		tinyxml2::XMLElement* root = document.RootElement();
		if (!root)
			return Fail(report, file, "it has no root element");

		const int version = GetVersion(root);
		const int current = CurrentVersion(type, steps);

		if (version == current)
			return Result::UpToDate;

		if (version > current)
		{
			std::string message = file.string() + " was saved by a newer version of Attercop (format "
				+ std::to_string(version) + ", this engine reads up to " + std::to_string(current)
				+ "). It loads as far as it can, and can't be saved over";
			report.errors.push_back(message);
			ENGINE_ERROR("{0}", message);
			return Result::TooNew;
		}

		if (version < 1)
			return Fail(report, file, "FormatVersion " + std::to_string(version) + " is not a format version");

		// A project's number goes up with each scene and state machine format, so only its files are warned about
		if (type != FileType::Project && version < current - 1)
		{
			std::string message = file.string() + " is format " + std::to_string(version) + ", more than one behind "
				+ std::to_string(current) + ", so it may not upgrade correctly."
				+ " Open and save it in each version of Attercop in between to be sure";
			report.warnings.push_back(message);
			ENGINE_WARN("{0}", message);
		}

		for (int from = version; from < current; ++from)
		{
			auto step = std::find_if(steps.steps.begin(), steps.steps.end(),
				[type, from](const Step& candidate) { return candidate.type == type && candidate.from == from; });

			// Steps from older formats are removed, and the .proj has no steps of its own
			if (step == steps.steps.end())
			{
				if (type == FileType::Project || from < current - 1)
					continue;
				return Fail(report, file, "there's no step from format " + std::to_string(from));
			}

			const size_t errorCount = report.errors.size();
			if (!step->apply(document, report))
			{
				std::vector<std::string> reasons(report.errors.begin() + (std::ptrdiff_t)errorCount, report.errors.end());
				report.errors.resize(errorCount);
				return Fail(report, file, std::string("\"") + step->description + "\" failed"
					+ (reasons.empty() ? "" : ": " + Join(reasons)));
			}
			report.steps.push_back(step->description);

			root = document.RootElement();
			if (!root)
				return Fail(report, file, std::string("\"") + step->description + "\" removed the root element");
			SetVersion(root, from + 1);
		}

		SetVersion(root, current);
		if (root->FindAttribute("EngineVersion"))
			root->SetAttribute("EngineVersion", VERSION);
		return Result::Upgraded;
	}

	/* -------------------------------------------------------------------------------------------------------------- */

	ProjectReport UpgradeProject(const std::filesystem::path& projectFile, const StepList& steps)
	{
		ProjectReport result;
		const std::filesystem::path directory = projectFile.parent_path();
		const std::filesystem::path projectName = projectFile.filename();

		tinyxml2::XMLDocument project;
		if (project.LoadFile(projectFile.string().c_str()) != tinyxml2::XML_SUCCESS)
		{
			std::string reason = std::string(project.ErrorName()) + " on line " + std::to_string(project.ErrorLineNum());
			ENGINE_ERROR("Could not upgrade {0}: {1}", projectFile.string(), reason);
			result.failed.push_back({ projectName, { reason } });
			return result;
		}

		const tinyxml2::XMLElement* projectRoot = project.RootElement();
		const int projectVersion = projectRoot ? GetVersion(projectRoot) : 1;
		const int current = CurrentVersion(FileType::Project, steps);

		if (projectVersion == current)
		{
			ENGINE_INFO("{0} is up to date (format {1}), nothing to upgrade", projectName.string(), current);
			return result;
		}

		if (projectVersion > current)
		{
			Report report;
			Upgrade(FileType::Project, project, projectFile, report, steps);
			result.failed.push_back({ projectName, report.errors });
			return result;
		}

		for (const std::filesystem::path& path : FindFiles(directory))
		{
			const std::filesystem::path relative = path.lexically_relative(directory);
			const FileType type = path.extension() == ".scene" ? FileType::Scene : FileType::StateMachine;

			tinyxml2::XMLDocument document;
			if (document.LoadFile(path.string().c_str()) != tinyxml2::XML_SUCCESS)
			{
				std::string reason = std::string(document.ErrorName()) + " on line " + std::to_string(document.ErrorLineNum());
				ENGINE_ERROR("Could not upgrade {0}: {1}", path.string(), reason);
				result.failed.push_back({ relative, { reason } });
				continue;
			}

			Report report;
			switch (Upgrade(type, document, path, report, steps))
			{
			case Result::UpToDate:
				break;
			case Result::Upgraded:
				if (document.SaveFile(path.string().c_str()) != tinyxml2::XML_SUCCESS)
				{
					ENGINE_ERROR("Could not write {0}: {1}", path.string(), document.ErrorName());
					result.failed.push_back({ relative, { std::string("could not write it: ") + document.ErrorName() } });
					break;
				}
				ENGINE_INFO("Upgraded {0}: {1}", relative.generic_string(), Join(report.steps));
				result.upgraded.push_back({ relative, report.steps, report.warnings });
				break;
			case Result::TooNew:
			case Result::Failed:
				result.failed.push_back({ relative, report.errors });
				break;
			}
		}

		if (!result.failed.empty())
		{
			ENGINE_ERROR("{0} file(s) in {1} could not be upgraded and were left as they were. The project stays at format {2}, so they're tried again next time",
				result.failed.size(), projectName.string(), projectVersion);
			return result;
		}

		// Raised last, so an interrupted upgrade is tried again
		Report report;
		if (Upgrade(FileType::Project, project, projectFile, report, steps) == Result::Upgraded)
		{
			if (project.SaveFile(projectFile.string().c_str()) == tinyxml2::XML_SUCCESS)
				result.projectRaised = true;
			else
			{
				ENGINE_ERROR("Could not write {0}: {1}", projectFile.string(), project.ErrorName());
				result.failed.push_back({ projectName, { std::string("could not write it: ") + project.ErrorName() } });
				return result;
			}
		}
		else
		{
			result.failed.push_back({ projectName, report.errors });
			return result;
		}

		ENGINE_INFO("Upgraded {0} to format {1}: {2} file(s) changed", projectName.string(), current, result.upgraded.size());
		return result;
	}
}
