#include "vendor/doctest.h"
#include "ComponentFixture.h"
#include "TestHelpers.h"

#include "Core/Application.h"
#include "Core/Version.h"
#include "Migration/Migration.h"
#include "Physics/CollisionLayers.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include "TinyXml2/tinyxml2.h"

#include <fstream>
#include <sstream>
#include <string>

namespace MigrationTests
{
	using Migration::FileType;
	using Migration::Result;

	const char* k_SceneVersion1 = R"xml(<Scene EngineVersion="1">
    <Gravity X="0" Y="-9.8100004"/>
    <Entity Name="Player" ID="1001-1">
        <Transform>
            <Position X="1" Y="2" Z="0"/>
            <Rotation X="0" Y="0" Z="0"/>
            <Scale X="1" Y="1" Z="1"/>
        </Transform>
        <Entity Name="Sword" ID="1001-2"/>
    </Entity>
</Scene>
)xml";

	const char* k_SceneTooNew = R"xml(<Scene EngineVersion="900" FormatVersion="99">
    <Gravity X="0" Y="-9.8100004"/>
    <Entity Name="Player" ID="1001-1">
        <FutureComponent Value="1"/>
    </Entity>
</Scene>
)xml";

	const char* k_StateMachineVersion1 = R"xml(<StateMachine EngineVersion="1" InitialState="Off">
    <Entry x="0" y="0"/>
    <State Name="Off" Filepath="Scripts/Off.lua" x="200" y="0"/>
    <State Name="On" Filepath="Scripts/On.lua" x="200" y="100"/>
    <Transition From="Off" To="On"/>
</StateMachine>
)xml";

	const char* k_Project = R"xml(<Project EngineVersion="1">
    <DefaultScene Filepath="Scenes/Good.scene"/>
    <Description>Migration test</Description>
    <SpriteAtlasPageSize>2048</SpriteAtlasPageSize>
</Project>
)xml";

	std::string Print(const tinyxml2::XMLDocument& document)
	{
		tinyxml2::XMLPrinter printer;
		document.Print(&printer);
		return printer.CStr();
	}

	std::string ReadFile(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary);
		std::stringstream contents;
		contents << file.rdbuf();
		return contents.str();
	}

	void WriteFile(const std::filesystem::path& path, const std::string& contents)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream(path, std::ios::binary) << contents;
	}

	// Test steps: the first marks every entity, the second needs the first's mark
	bool MarkEntities(tinyxml2::XMLDocument& document, Migration::Report&)
	{
		tinyxml2::XMLElement* root = document.RootElement();
		std::vector<tinyxml2::XMLElement*> pending = { root };
		while (!pending.empty())
		{
			tinyxml2::XMLElement* element = pending.back();
			pending.pop_back();
			if (std::string(element->Name()) == "Entity")
				element->SetAttribute("Marked", "1");
			for (tinyxml2::XMLElement* child = element->FirstChildElement(); child; child = child->NextSiblingElement())
				pending.push_back(child);
		}
		return true;
	}

	bool CountMarks(tinyxml2::XMLDocument& document, Migration::Report& report)
	{
		tinyxml2::XMLElement* root = document.RootElement();
		tinyxml2::XMLElement* entity = std::string(root->Name()) == "Entity" ? root : root->FirstChildElement("Entity");
		if (!entity || !entity->Attribute("Marked"))
		{
			report.errors.push_back("not marked");
			return false;
		}
		entity->SetAttribute("Counted", entity->IntAttribute("Counted") + 1);
		return true;
	}

	bool RenameInitialState(tinyxml2::XMLDocument& document, Migration::Report&)
	{
		tinyxml2::XMLElement* root = document.RootElement();
		root->SetAttribute("Initial", root->Attribute("InitialState"));
		root->DeleteAttribute("InitialState");
		return true;
	}

	bool AlwaysFails(tinyxml2::XMLDocument&, Migration::Report& report)
	{
		report.errors.push_back("broken on purpose");
		return false;
	}

	// No steps: every type at format 1
	const Migration::StepList k_NoSteps = { 1, 1, {} };

	const Migration::StepList k_TestSteps = { 3, 2, {
		{ FileType::Scene, 1, "Entities marked", &MarkEntities },
		{ FileType::Scene, 2, "Marks counted", &CountMarks },
		{ FileType::StateMachine, 1, "InitialState renamed", &RenameInitialState },
	} };

	// A project folder under the temporary directory, removed afterwards
	struct TemporaryProject
	{
		std::filesystem::path directory = std::filesystem::temp_directory_path() / "AttercopMigrationTests";

		TemporaryProject() { std::filesystem::remove_all(directory); }
		~TemporaryProject() { std::filesystem::remove_all(directory); }
	};

	// A format 1 scene upgraded by the engine's steps, printed
	std::string UpgradeScene(const std::string& before, Migration::Report& report)
	{
		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse(before.c_str()) == tinyxml2::XML_SUCCESS);
		REQUIRE(Migration::Upgrade(FileType::Scene, document, "Step.scene", report) == Result::Upgraded);
		return Print(document);
	}

	std::string Printed(const std::string& xml)
	{
		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
		return Print(document);
	}

	// One entity's components, before and after the step from format 1
	void CheckStep(const std::string& before, const std::string& after, size_t warnings = 0)
	{
		Migration::Report report;
		CHECK(UpgradeScene("<Scene EngineVersion=\"3\"><Entity Name=\"E\" ID=\"1-2\">" + before + "</Entity></Scene>", report)
			== Printed("<Scene FormatVersion=\"2\"><Entity Name=\"E\" ID=\"1-2\">" + after + "</Entity></Scene>"));
		CHECK(report.warnings.size() == warnings);
	}

	TEST_CASE("Scenes are at format 2 and the project number follows")
	{
		CHECK(Migration::CurrentVersion(FileType::Scene) == 2);
		CHECK(Migration::CurrentVersion(FileType::StateMachine) == 1);
		CHECK(Migration::CurrentVersion(FileType::Project) == 2);
		CHECK(Migration::CurrentVersion(FileType::Project, k_NoSteps) == 1);
	}

	TEST_CASE("A document at the current format is up to date and unchanged")
	{
		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse(k_SceneVersion1) == tinyxml2::XML_SUCCESS);
		const std::string before = Print(document);

		Migration::Report report;
		CHECK(Migration::Upgrade(FileType::Scene, document, "Version1.scene", report, k_NoSteps) == Result::UpToDate);
		CHECK(Print(document) == before);
		CHECK(report.steps.empty());
		CHECK(report.errors.empty());
	}

	TEST_CASE("A document newer than the engine is too new and left alone")
	{
		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse(k_SceneTooNew) == tinyxml2::XML_SUCCESS);
		const std::string before = Print(document);

		Migration::Report report;
		CHECK(Migration::Upgrade(FileType::Scene, document, "TooNew.scene", report) == Result::TooNew);
		CHECK(Print(document) == before);
		REQUIRE(report.errors.size() == 1);
		CHECK(report.errors[0].find("TooNew.scene") != std::string::npos);
	}

	TEST_CASE("A missing FormatVersion means 1")
	{
		tinyxml2::XMLDocument withoutVersion;
		REQUIRE(withoutVersion.Parse(k_SceneVersion1) == tinyxml2::XML_SUCCESS);
		CHECK(Migration::GetVersion(withoutVersion.RootElement()) == 1);

		// So the chain starts from the first step
		Migration::Report report;
		CHECK(Migration::Upgrade(FileType::Scene, withoutVersion, "Version1.scene", report, k_TestSteps) == Result::Upgraded);
		CHECK(report.steps == std::vector<std::string>{ "Entities marked", "Marks counted" });
	}

	TEST_CASE("Upgrading runs the chain from the document's version")
	{
		SUBCASE("from version 1, replacing EngineVersion with FormatVersion")
		{
			tinyxml2::XMLDocument document;
			REQUIRE(document.Parse(k_SceneVersion1) == tinyxml2::XML_SUCCESS);

			Migration::Report report;
			CHECK(Migration::Upgrade(FileType::Scene, document, "Version1.scene", report, k_TestSteps) == Result::Upgraded);
			CHECK(Migration::CurrentVersion(FileType::Scene, k_TestSteps) == 3);
			CHECK(Migration::CurrentVersion(FileType::Project, k_TestSteps) == 4);
			CHECK(report.warnings.size() == 1);

			const tinyxml2::XMLElement* root = document.RootElement();
			const tinyxml2::XMLAttribute* first = root->FirstAttribute();
			CHECK(std::string(first->Name()) == "FormatVersion");
			CHECK(first->IntValue() == 3);
			CHECK_FALSE(root->Attribute("EngineVersion"));

			const tinyxml2::XMLElement* player = root->FirstChildElement("Entity");
			CHECK(player->IntAttribute("Counted") == 1);
			CHECK(player->FirstChildElement("Entity")->Attribute("Marked"));
		}

		SUBCASE("from version 2, skipping the first step")
		{
			tinyxml2::XMLDocument document;
			REQUIRE(document.Parse(R"xml(<Scene EngineVersion="3" FormatVersion="2"><Entity Name="A" ID="1" Marked="1"/></Scene>)xml") == tinyxml2::XML_SUCCESS);

			Migration::Report report;
			CHECK(Migration::Upgrade(FileType::Scene, document, "Version2.scene", report, k_TestSteps) == Result::Upgraded);
			CHECK(report.steps == std::vector<std::string>{ "Marks counted" });
			CHECK(report.warnings.empty());
			CHECK(Migration::GetVersion(document.RootElement()) == 3);
		}

		SUBCASE("a pasted entity, with FormatVersion after the attributes it had")
		{
			tinyxml2::XMLDocument document;
			REQUIRE(document.Parse(R"xml(<Entity Name="A" ID="1"/>)xml") == tinyxml2::XML_SUCCESS);

			Migration::Report report;
			CHECK(Migration::Upgrade(FileType::Scene, document, "the pasted entity", report, k_TestSteps) == Result::Upgraded);
			CHECK(Print(document) == "<Entity Name=\"A\" ID=\"1\" Marked=\"1\" FormatVersion=\"3\" Counted=\"1\"/>\n");
		}

		SUBCASE("a state machine, with FormatVersion before its other attributes")
		{
			tinyxml2::XMLDocument document;
			REQUIRE(document.Parse(k_StateMachineVersion1) == tinyxml2::XML_SUCCESS);

			Migration::Report report;
			CHECK(Migration::Upgrade(FileType::StateMachine, document, "Switch.statemachine", report, k_TestSteps) == Result::Upgraded);
			const tinyxml2::XMLAttribute* attribute = document.RootElement()->FirstAttribute();
			CHECK(std::string(attribute->Name()) == "FormatVersion");
			CHECK(attribute->IntValue() == 2);
			CHECK(std::string(attribute->Next()->Name()) == "Initial");
			CHECK_FALSE(attribute->Next()->Next());
		}

		SUBCASE("a project, keeping EngineVersion as this engine's")
		{
			tinyxml2::XMLDocument document;
			REQUIRE(document.Parse(k_Project) == tinyxml2::XML_SUCCESS);

			Migration::Report report;
			CHECK(Migration::Upgrade(FileType::Project, document, "Project.proj", report, k_TestSteps) == Result::Upgraded);
			const tinyxml2::XMLAttribute* attribute = document.RootElement()->FirstAttribute();
			CHECK(std::string(attribute->Name()) == "EngineVersion");
			CHECK(attribute->IntValue() == VERSION);
			CHECK(std::string(attribute->Next()->Name()) == "FormatVersion");
			CHECK(attribute->Next()->IntValue() == 4);
		}
	}

	TEST_CASE("A document more than one format behind is upgraded with a warning")
	{
		// At scene format 3, so the step from 1 was removed
		const Migration::StepList steps = { 3, 1, { { FileType::Scene, 2, "Entities marked", &MarkEntities } } };
		CHECK(Migration::CurrentVersion(FileType::Project, steps) == 3);

		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse(k_SceneVersion1) == tinyxml2::XML_SUCCESS);

		Migration::Report report;
		CHECK(Migration::Upgrade(FileType::Scene, document, "Version1.scene", report, steps) == Result::Upgraded);
		CHECK(report.steps == std::vector<std::string>{ "Entities marked" });
		REQUIRE(report.warnings.size() == 1);
		CHECK(report.warnings[0].find("Version1.scene") != std::string::npos);
		CHECK(report.errors.empty());
		CHECK(Migration::GetVersion(document.RootElement()) == 3);
	}

	TEST_CASE("A format raised without a step from the previous one fails")
	{
		const Migration::StepList steps = { 2, 1, {} };

		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse(k_SceneVersion1) == tinyxml2::XML_SUCCESS);

		Migration::Report report;
		CHECK(Migration::Upgrade(FileType::Scene, document, "Version1.scene", report, steps) == Result::Failed);
		CHECK(Migration::GetVersion(document.RootElement()) == 1);
	}

	TEST_CASE("Upgrading an upgraded document changes nothing")
	{
		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse(k_SceneVersion1) == tinyxml2::XML_SUCCESS);

		Migration::Report first;
		REQUIRE(Migration::Upgrade(FileType::Scene, document, "Version1.scene", first, k_TestSteps) == Result::Upgraded);
		const std::string upgraded = Print(document);

		Migration::Report second;
		CHECK(Migration::Upgrade(FileType::Scene, document, "Version1.scene", second, k_TestSteps) == Result::UpToDate);
		CHECK(Print(document) == upgraded);
		CHECK(second.steps.empty());
	}

	TEST_CASE("A failing step fails the upgrade with its reason")
	{
		const Migration::StepList steps = { 2, 1, { { FileType::Scene, 1, "Breaks", &AlwaysFails } } };

		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse(k_SceneVersion1) == tinyxml2::XML_SUCCESS);

		Migration::Report report;
		CHECK(Migration::Upgrade(FileType::Scene, document, "Version1.scene", report, steps) == Result::Failed);
		REQUIRE(report.errors.size() == 1);
		CHECK(report.errors[0].find("broken on purpose") != std::string::npos);
		CHECK(Migration::GetVersion(document.RootElement()) == 1);
	}

	TEST_CASE("Upgrading a project reports broken files, upgrades the rest and raises its number last")
	{
		TemporaryProject project;
		const std::filesystem::path& directory = project.directory;
		const std::filesystem::path projectFile = directory / "Test.proj";

		WriteFile(projectFile, k_Project);
		WriteFile(directory / "Scenes/Good.scene", k_SceneVersion1);
		WriteFile(directory / "Scenes/Current.scene", R"xml(<Scene EngineVersion="3" FormatVersion="3"/>)xml");
		WriteFile(directory / "Scenes/Broken.scene", "<Scene EngineVersion=\"1\"><Entity></Scene>");
		WriteFile(directory / "StateMachines/Switch.statemachine", k_StateMachineVersion1);
		WriteFile(directory / ".hidden/Ignored.scene", k_SceneVersion1);

		const std::string brokenBefore = ReadFile(directory / "Scenes/Broken.scene");
		const std::string currentBefore = ReadFile(directory / "Scenes/Current.scene");

		Migration::ProjectReport first = Migration::UpgradeProject(projectFile, k_TestSteps);

		REQUIRE(first.failed.size() == 1);
		CHECK(first.failed[0].path == std::filesystem::path("Scenes/Broken.scene"));
		CHECK(ReadFile(directory / "Scenes/Broken.scene") == brokenBefore);

		REQUIRE(first.upgraded.size() == 2);
		CHECK(first.upgraded[0].path == std::filesystem::path("Scenes/Good.scene"));
		CHECK(first.upgraded[0].steps == std::vector<std::string>{ "Entities marked", "Marks counted" });
		CHECK(first.upgraded[1].path == std::filesystem::path("StateMachines/Switch.statemachine"));
		CHECK(ReadFile(directory / "Scenes/Current.scene") == currentBefore);
		CHECK(ReadFile(directory / ".hidden/Ignored.scene") == k_SceneVersion1);

		CHECK_FALSE(first.projectRaised);
		CHECK(ReadFile(projectFile) == k_Project);

		// Again: the broken file is tried again, the upgraded ones are left as they are
		const std::string good = ReadFile(directory / "Scenes/Good.scene");
		const std::string switchMachine = ReadFile(directory / "StateMachines/Switch.statemachine");
		Migration::ProjectReport second = Migration::UpgradeProject(projectFile, k_TestSteps);
		CHECK(second.failed.size() == 1);
		CHECK(second.upgraded.empty());
		CHECK(ReadFile(directory / "Scenes/Good.scene") == good);
		CHECK(ReadFile(directory / "StateMachines/Switch.statemachine") == switchMachine);
		CHECK(ReadFile(projectFile) == k_Project);

		// Once it's fixed, the project's number is raised
		std::filesystem::remove(directory / "Scenes/Broken.scene");
		Migration::ProjectReport third = Migration::UpgradeProject(projectFile, k_TestSteps);
		CHECK(third.Succeeded());
		CHECK(third.upgraded.empty());
		CHECK(third.projectRaised);
		tinyxml2::XMLDocument proj;
		REQUIRE(proj.LoadFile(projectFile.string().c_str()) == tinyxml2::XML_SUCCESS);
		CHECK(Migration::GetVersion(proj.RootElement()) == 4);

		// And then nothing is read at all
		const std::string raised = ReadFile(projectFile);
		WriteFile(directory / "Scenes/Broken.scene", brokenBefore);
		Migration::ProjectReport fourth = Migration::UpgradeProject(projectFile, k_TestSteps);
		CHECK(fourth.Succeeded());
		CHECK_FALSE(fourth.projectRaised);
		CHECK(ReadFile(projectFile) == raised);
	}

	TEST_CASE("Upgrading a project with no steps changes nothing")
	{
		TemporaryProject project;
		const std::filesystem::path projectFile = project.directory / "Test.proj";
		WriteFile(projectFile, k_Project);
		WriteFile(project.directory / "Scenes/Good.scene", k_SceneVersion1);

		Migration::ProjectReport report = Migration::UpgradeProject(projectFile, k_NoSteps);
		CHECK(report.Succeeded());
		CHECK(report.upgraded.empty());
		CHECK_FALSE(report.projectRaised);
		CHECK(ReadFile(projectFile) == k_Project);
		CHECK(ReadFile(project.directory / "Scenes/Good.scene") == k_SceneVersion1);
	}

	TEST_CASE("Saving over a scene from a newer engine is refused")
	{
		TemporaryProject project;
		const std::filesystem::path path = project.directory / "TooNew.scene";
		WriteFile(path, k_SceneTooNew);

		Scene scene(path);
		REQUIRE(scene.Load());
		scene.Save();
		CHECK(ReadFile(path) == k_SceneTooNew);

		// Saving it somewhere else is allowed, in this engine's format
		const std::filesystem::path copy = project.directory / "Copy.scene";
		scene.Save(copy);
		REQUIRE(std::filesystem::exists(copy));
		tinyxml2::XMLDocument document;
		REQUIRE(document.LoadFile(copy.string().c_str()) == tinyxml2::XML_SUCCESS);
		CHECK(Migration::GetVersion(document.RootElement()) == Migration::CurrentVersion(FileType::Scene));
		CHECK_FALSE(document.RootElement()->Attribute("EngineVersion"));
	}

	TEST_CASE("Copied entities carry their format version")
	{
		Scene scene("");
		std::string xml = SceneSerializer::SerializeEntity(scene.CreateEntity("Copied"));
		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
		CHECK(document.RootElement()->Attribute("FormatVersion"));
		CHECK(Migration::GetVersion(document.RootElement()) == Migration::CurrentVersion(FileType::Scene));
	}

	TEST_CASE("Files kept from format 1 upgrade to the repository's files")
	{
		const std::filesystem::path format1 = RepositoryPath("Tests/UnitTests/Data/Format1");
		const std::filesystem::path counterparts[] = {
			RepositoryPath("Tests/Scenes/Scenes"),
			RepositoryPath("Tests/Scenes/StateMachines"),
		};

		int compared = 0;
		for (const auto& entry : std::filesystem::directory_iterator(format1))
		{
			const std::filesystem::path& path = entry.path();
			if (path.filename() == "AllComponents.scene")
				continue;
			const std::string name = path.filename().string();
			CAPTURE(name);
			const FileType type = path.extension() == ".scene" ? FileType::Scene : FileType::StateMachine;

			std::filesystem::path counterpart;
			for (const std::filesystem::path& directory : counterparts)
				if (std::filesystem::exists(directory / path.filename()))
					counterpart = directory / path.filename();
			REQUIRE_FALSE(counterpart.empty());

			// The project's layer names, as SceneTest --upgrade-project loads them
			CollisionLayers::LoadNames(counterpart.parent_path().parent_path() / CollisionLayers::FilePath);

			tinyxml2::XMLDocument old, current;
			REQUIRE(old.LoadFile(path.string().c_str()) == tinyxml2::XML_SUCCESS);
			REQUIRE(current.LoadFile(counterpart.string().c_str()) == tinyxml2::XML_SUCCESS);
			Migration::Report oldReport, currentReport;
			CHECK(Migration::Upgrade(type, old, path, oldReport) != Result::Failed);
			CHECK(Migration::Upgrade(type, current, counterpart, currentReport) == Result::UpToDate);
			CHECK(Print(old) == Print(current));
			++compared;
		}
		CollisionLayers::SetNames(CollisionLayers::DefaultNames());
		CHECK(compared == 9);
	}

	TEST_CASE("The format 1 entity with every component upgrades and loads")
	{
		const std::filesystem::path path = RepositoryPath("Tests/UnitTests/Data/Format1/AllComponents.scene");
		tinyxml2::XMLDocument document;
		REQUIRE(document.LoadFile(path.string().c_str()) == tinyxml2::XML_SUCCESS);

		Migration::Report report;
		CHECK(Migration::Upgrade(FileType::Scene, document, path, report) != Result::Failed);

		Scene scene("");
		tinyxml2::XMLElement* entity = document.RootElement()->FirstChildElement("Entity");
		REQUIRE(entity);
		ComponentFixture::Check(SceneSerializer::DeserializeEntity(&scene, entity));
	}

	TEST_CASE("The scene step from format 1 rewrites each difference in the table")
	{
		SUBCASE("Camera: <SceneCamera> folded in, ProjectionType as Projection by name, FOV as Fov in degrees")
		{
			CheckStep(R"(<Camera Primary="true" FixedAspectRatio="true"><SceneCamera ProjectionType="1" OrthoSize="10" OrthoNear="-1" OrthoFar="1" PerspectiveNear="0.0099999998" PerspectiveFar="1000" FOV="0.78539819" AspectRatio="2"/></Camera>)",
				R"(<Camera Primary="true" FixedAspectRatio="true" Projection="Orthographic" OrthoSize="10" OrthoNear="-1" OrthoFar="1" PerspectiveNear="0.0099999998" PerspectiveFar="1000" Fov="45" AspectRatio="2"/>)");
			CheckStep(R"(<Camera Primary="false" FixedAspectRatio="false"><SceneCamera ProjectionType="0" OrthoSize="10" OrthoNear="-1" OrthoFar="1" PerspectiveNear="0.5" PerspectiveFar="100" FOV="1"/></Camera>)",
				R"(<Camera Primary="false" FixedAspectRatio="false" Projection="Perspective" OrthoSize="10" OrthoNear="-1" OrthoFar="1" PerspectiveNear="0.5" PerspectiveFar="100" Fov="57.29578"/>)");
		}

		SUBCASE("Sprite: filter and wrap moved onto <Sprite> as names")
		{
			CheckStep(R"(<Sprite TilingFactor="1"><Texture Filepath="Sprites/Player.png" FilterMethod="1" WrapMethod="2"/><Tint R="1" G="1" B="1" A="1"/></Sprite>)",
				R"(<Sprite TilingFactor="1" FilterMethod="Nearest" WrapMethod="Repeat"><Texture Filepath="Sprites/Player.png"/><Tint R="1" G="1" B="1" A="1"/></Sprite>)");
			CheckStep(R"(<Sprite TilingFactor="2"><Texture Filepath="a.png" FilterMethod="0" WrapMethod="1"/></Sprite>)",
				R"(<Sprite TilingFactor="2" FilterMethod="Linear" WrapMethod="Mirror"><Texture Filepath="a.png"/></Sprite>)");
			CheckStep(R"(<Sprite TilingFactor="1"><Tint R="1" G="1" B="1" A="1"/></Sprite>)",
				R"(<Sprite TilingFactor="1"><Tint R="1" G="1" B="1" A="1"/></Sprite>)");
		}

		SUBCASE("RigidBody2D: BodyType as Type, by name")
		{
			CheckStep(R"(<RigidBody2D BodyType="2" FixedRotation="true"/>)", R"(<RigidBody2D Type="Dynamic" FixedRotation="true"/>)");
			CheckStep(R"(<RigidBody2D BodyType="0"/>)", R"(<RigidBody2D Type="Static"/>)");
			CheckStep(R"(<RigidBody2D BodyType="1"/>)", R"(<RigidBody2D Type="Kinematic"/>)");
		}

		SUBCASE("Primitive: Type by name")
		{
			const char* shapes[] = { "Cube", "Sphere", "Plane", "Cylinder", "Cone", "Torus" };
			for (int i = 0; i < 6; ++i)
				CheckStep("<Primitive Type=\"" + std::to_string(i) + "\"/>", std::string("<Primitive Type=\"") + shapes[i] + "\"/>");
		}

		SUBCASE("CapsuleCollider2D: Direction by name")
		{
			CheckStep(R"(<CapsuleCollider2D Direction="0" Radius="0.5"/>)", R"(<CapsuleCollider2D Direction="Vertical" Radius="0.5"/>)");
			CheckStep(R"(<CapsuleCollider2D Direction="1"/>)", R"(<CapsuleCollider2D Direction="Horizontal"/>)");
		}

		SUBCASE("Transform: rotation from radians to degrees that read back to the same floats")
		{
			CheckStep(R"(<Transform><Position X="1" Y="2" Z="3"/><Rotation X="0.25" Y="-1.5707964" Z="7"/><Scale X="1" Y="1" Z="1"/></Transform>)",
				R"(<Transform><Position X="1" Y="2" Z="3"/><Rotation X="14.323945" Y="-90" Z="401.07046"/><Scale X="1" Y="1" Z="1"/></Transform>)");
			CheckStep(R"(<Transform><Rotation Z="0"/></Transform>)", R"(<Transform><Rotation Z="0"/></Transform>)");
			CheckStep(R"(<Transform><Rotation X="up"/></Transform>)", R"(<Transform><Rotation X="up"/></Transform>)", 1);
		}

		SUBCASE("Widget: Rotation in degrees")
		{
			CheckStep(R"(<Widget Disabled="false" Rotation="0.5"/>)", R"(<Widget Disabled="false" Rotation="28.64789"/>)");
			CheckStep(R"(<Widget Rotation="0"/>)", R"(<Widget Rotation="0"/>)");
		}

		SUBCASE("Billboard: Orientation by name")
		{
			CheckStep(R"(<Billboard Orientation="0"/>)", R"(<Billboard Orientation="WorldUp"/>)");
			CheckStep(R"(<Billboard Orientation="1"/>)", R"(<Billboard Orientation="Camera"/>)");
		}

		SUBCASE("Button: each texture's filter and wrap moved onto <Button> as names")
		{
			CheckStep(R"(<Button><NormalTexture Filepath="n.png" FilterMethod="1" WrapMethod="0"/><ClickedTexture Filepath="c.png" FilterMethod="0" WrapMethod="2"/><NormalTint R="1" G="1" B="1" A="1"/></Button>)",
				R"(<Button NormalFilterMethod="Nearest" ClickedFilterMethod="Linear" NormalWrapMethod="Clamp" ClickedWrapMethod="Repeat"><NormalTexture Filepath="n.png"/><ClickedTexture Filepath="c.png"/><NormalTint R="1" G="1" B="1" A="1"/></Button>)");
		}

		SUBCASE("BehaviourTree: an absolute path made relative to the project")
		{
			// No project is open in the unit tests, so paths are relative to the current directory
			const std::filesystem::path project = Application::GetOpenDocumentDirectory();
			const std::filesystem::path absolute = (project.empty() ? std::filesystem::current_path() : std::filesystem::absolute(project)) / "AI" / "Enemy.behaviourtree";
			CheckStep("<BehaviourTree Filepath=\"" + absolute.generic_string() + "\"/>", R"(<BehaviourTree Filepath="AI/Enemy.behaviourtree"/>)");
			CheckStep(R"(<BehaviourTree Filepath="AI/Enemy.behaviourtree"/>)", R"(<BehaviourTree Filepath="AI/Enemy.behaviourtree"/>)");
		}

		SUBCASE("Colliders and tilemaps: Layer and Mask by name")
		{
			CollisionLayers::Names names = CollisionLayers::DefaultNames();
			names[1] = "Player";
			names[3] = "Enemy";
			CollisionLayers::SetNames(names);

			for (const char* component : { "BoxCollider2D", "CircleCollider2D", "CapsuleCollider2D", "PolygonCollider2D", "Tilemap" })
			{
				CAPTURE(component);
				const std::string name = component;
				CheckStep("<" + name + R"( IsTrigger="false" Layer="2" Mask="13"/>)", "<" + name + R"( IsTrigger="false" Layer="Player" Mask="Default, Layer 2, Enemy"/>)");
				CheckStep("<" + name + R"( Layer="1" Mask="65535"/>)", "<" + name + R"( Layer="Default" Mask="Everything"/>)");
				CheckStep("<" + name + R"( Layer="32" Mask="0"/>)", "<" + name + R"( Layer="Layer 5" Mask="Nothing"/>)");
			}
			// Not one layer: the lowest, or Default for none, with a warning
			CheckStep(R"(<BoxCollider2D Layer="10" Mask="2"/>)", R"(<BoxCollider2D Layer="Player" Mask="Player"/>)", 1);
			CheckStep(R"(<BoxCollider2D Layer="0"/>)", R"(<BoxCollider2D Layer="Default"/>)", 1);

			CollisionLayers::SetNames(CollisionLayers::DefaultNames());
		}

		SUBCASE("StaticMesh: 0.0.1's <MaterialOverride> per submesh as slots, with a warning")
		{
			CheckStep(R"(<StaticMesh Filepath="m.staticmesh"><MaterialOverride Filepath="a.material"/><MaterialOverride Filepath="b.material"/></StaticMesh>)",
				R"(<StaticMesh Filepath="m.staticmesh"><Material Slot="0" Filepath="a.material"/><Material Slot="1" Filepath="b.material"/></StaticMesh>)", 1);
			CheckStep(R"(<StaticMesh Filepath="m.staticmesh"><Material Slot="2" Filepath="a.material"/></StaticMesh>)",
				R"(<StaticMesh Filepath="m.staticmesh"><Material Slot="2" Filepath="a.material"/></StaticMesh>)");
		}

		SUBCASE("An unknown number is kept, with a warning, for the loader to report")
		{
			CheckStep(R"(<RigidBody2D BodyType="7"/>)", R"(<RigidBody2D Type="7"/>)", 1);
		}

		SUBCASE("Child entities and pasted entities are upgraded too")
		{
			CheckStep(R"(<Entity Name="Child" ID="3-4"><Billboard Orientation="1"/></Entity>)",
				R"(<Entity Name="Child" ID="3-4"><Billboard Orientation="Camera"/></Entity>)");

			tinyxml2::XMLDocument document;
			REQUIRE(document.Parse(R"(<Entity Name="E" ID="1-2"><Billboard Orientation="1"/></Entity>)") == tinyxml2::XML_SUCCESS);
			Migration::Report report;
			CHECK(Migration::Upgrade(FileType::Scene, document, "the pasted entity", report) == Result::Upgraded);
			CHECK(Print(document) == Printed(R"(<Entity Name="E" ID="1-2" FormatVersion="2"><Billboard Orientation="Camera"/></Entity>)"));
		}

		SUBCASE("Everything else is left as it was")
		{
			const std::string same = R"(<Transform><Position X="1" Y="2" Z="3"/></Transform><Text MaxWidth="5">Hi<Colour R="1" G="1" B="1" A="1"/></Text><Tilemap Orientation="Isometric" TilesWide="2" TilesHigh="1">1,2</Tilemap><AudioSource Filepath="a.wav" Volume="0.5"/>)";
			CheckStep(same, same);
		}
	}

	TEST_CASE("An unknown collision layer name loads as Default with a warning naming the file and line")
	{
		CollisionLayers::SetNames(CollisionLayers::DefaultNames());
		WarningCapture warnings;

		Scene scene("");
		Entity entity = SceneSerializer::DeserializeEntity(&scene, R"(<Entity Name="E" ID="1-2" FormatVersion="2">
<BoxCollider2D Layer="Nope" Mask="Default, Gone"/>
</Entity>)");
		REQUIRE(entity.HasComponent<BoxCollider2DComponent>());
		CHECK(entity.GetComponent<BoxCollider2DComponent>().layer == CollisionLayers::DefaultLayer);
		CHECK(entity.GetComponent<BoxCollider2DComponent>().mask == CollisionLayers::DefaultLayer);

		const std::string text = warnings.Text();
		CHECK(text.find("Unknown collision layer \"Nope\" (the pasted entity, line 2), using Default") != std::string::npos);
		CHECK(text.find("Unknown collision layer \"Gone\" in a mask (the pasted entity, line 2), left out") != std::string::npos);
	}

	TEST_CASE("Unknown components, attributes and elements are warned about with the file and line")
	{
		WarningCapture warnings;

		Scene scene("");
		Entity entity = SceneSerializer::DeserializeEntity(&scene, R"(<Entity Name="E" ID="1-2" FormatVersion="2" Colour="red">
<Sprite TilingFactr="2">
<Tint R="0.5" Q="1"/>
<Shadow/>
</Sprite>
<Sparkles/>
</Entity>)");
		REQUIRE(entity.HasComponent<SpriteComponent>());
		CHECK(entity.GetComponent<SpriteComponent>().tint.r == 0.5f);
		CHECK(entity.GetComponent<SpriteComponent>().tilingFactor == 1.0f);

		const std::string text = warnings.Text();
		for (const char* expected : {
			"Unknown attribute Colour on <Entity>, ignored (the pasted entity, line 1)",
			"Unknown attribute TilingFactr on <Sprite>, ignored (the pasted entity, line 2)",
			"Unknown attribute Q on <Tint>, ignored (the pasted entity, line 3)",
			"Unknown element <Shadow> in <Sprite>, ignored (the pasted entity, line 4)",
			"Unknown component <Sparkles>, ignored (the pasted entity, line 6)" })
		{
			INFO(expected);
			CHECK(text.find(expected) != std::string::npos);
		}
	}
}
