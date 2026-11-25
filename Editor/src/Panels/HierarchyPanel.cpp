#include "HierarchyPanel.h"

#include <algorithm>

#include "imgui/imgui.h"
#include "IconsFontAwesome6.h"
#include "History/HistoryCommands.h"
#include "History/PropertyCommands.h"
#include "Selection/SelectionOperations.h"

#include "Scene/SceneGraph.h"
#include "Scene/SceneSerializer.h"
#include "Scene/SceneManager.h"
#include "ImGui/ImGuiUtilities.h"

#include "MainDockSpace.h"
#include "ImGui/ImGuiShortcut.h"

namespace
{
	constexpr const char* s_ContextMenuName = "EntityContextMenu";
	constexpr const char* s_SceneMenuName = "SceneContextMenu";

	// True if entity is ancestor or one of its descendants
	bool IsSelfOrDescendant(entt::registry& registry, entt::entity entity, entt::entity ancestor)
	{
		while (entity != entt::null)
		{
			if (entity == ancestor)
				return true;
			const HierarchyComponent* hierarchyComp = registry.try_get<HierarchyComponent>(entity);
			entity = hierarchyComp ? hierarchyComp->parent : entt::null;
		}
		return false;
	}
}

HierarchyPanel::HierarchyPanel(bool* show, Ref<SceneHistory> history, Ref<EntitySelection> selection)
	:Layer("Hierarchy"), m_Show(show), m_History(history), m_Selection(selection), m_TextFilter(CreateScope<ImGuiTextFilter>())
{
}

HierarchyPanel::~HierarchyPanel()
{
}

/* ------------------------------------------------------------------------------------------------------------------ */

void HierarchyPanel::OnAttach()
{
}

void HierarchyPanel::OnDetach()
{
	m_TextFilter->Clear();
}

/* ------------------------------------------------------------------------------------------------------------------ */

void HierarchyPanel::OnFixedUpdate()
{
}

/* ------------------------------------------------------------------------------------------------------------------ */

void HierarchyPanel::OnImGuiRender()
{
	PROFILE_FUNCTION();

	if (!*m_Show)
	{
		m_TextFilter->Clear();
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(ICON_FA_SITEMAP" Hierarchy", m_Show))
	{
		if (ImGui::IsWindowFocused())
		{
			MainDockSpace::SetFocussedWindow(this);
		}

		if (!SceneManager::IsSceneLoaded())
		{
			ImGui::TextUnformatted("No scene loaded");
			ImGui::End();
			return;
		}

		// Right click on a blank space or the Scene node; rows have their own menu
		if (ImGui::BeginPopupContextWindow(s_SceneMenuName, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			m_Selection->Clear();
			m_CreateParent = Entity();
			CreateMenu();
			ImGui::EndPopup();
		}

		ImGui::TextUnformatted(ICON_FA_MAGNIFYING_GLASS);
		ImGui::SameLine();
		m_TextFilter->Draw("##Search", ImGui::GetContentRegionAvail().x);
		ImGui::Tooltip("Filter (\"incl,-excl\")");

		Scene* scene = SceneManager::CurrentScene();

		BuildRows(scene);

		// A double-click's second release would change the selection the double-click kept
		m_SkipSelectionRequests = m_SkipNextRelease && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
		if (m_SkipSelectionRequests)
			m_SkipNextRelease = false;
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().MouseClickedCount[ImGuiMouseButton_Left] == 1)
			m_SelectionBeforeClick = m_Selection->Capture();

		// At the window's own ID, so clicking empty space focuses the scope and box select can start there
		ImGuiMultiSelectIO* selectionIo = ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid
			| ImGuiMultiSelectFlags_BoxSelect1d | ImGuiMultiSelectFlags_ScopeWindow | ImGuiMultiSelectFlags_SelectOnClickRelease
			| ImGuiMultiSelectFlags_NoSelectAll, (int)m_Selection->Count(), (int)m_Rows.size());
		ApplySelectionRequests(*selectionIo, *scene);

		bool openSceneMenu = false;
		if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_Bullet
			| ImGuiTreeNodeFlags_OpenOnDoubleClick))
		{
			openSceneMenu = ImGui::IsItemClicked(ImGuiMouseButton_Right);
			DragDropTarget(Entity());

			ImGuiListClipper clipper;
			clipper.Begin((int)m_Rows.size());

			// The range's start must stay submitted
			if (selectionIo->RangeSrcItem >= 0 && selectionIo->RangeSrcItem < (ImGuiSelectionUserData)m_Rows.size())
				clipper.IncludeItemByIndex((int)selectionIo->RangeSrcItem);

			// Keep the dragged row submitted so its drag source stays alive while scrolled away
			const ImGuiPayload* payload = ImGui::GetDragDropPayload();
			if (payload && payload->IsDataType("Entities") && !m_DraggedRow.IsNil())
			{
				if (Entity dragged = scene->GetEntityByID(m_DraggedRow))
				{
					for (size_t i = 0; i < m_Rows.size(); ++i)
					{
						if (m_Rows[i].entity == dragged.GetHandle())
						{
							clipper.IncludeItemByIndex((int)i);
							break;
						}
					}
				}
			}
			else
				m_DraggedRow = Uuid::Nil();

			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
					DrawRow(scene, m_Rows[i], i);
			}

			EntityContextMenu();
			ImGui::TreePop();
		}
		else
			openSceneMenu = ImGui::IsItemClicked(ImGuiMouseButton_Right);

		selectionIo = ImGui::EndMultiSelect();
		ApplySelectionRequests(*selectionIo, *scene);

		if (openSceneMenu)
			ImGui::OpenPopup(s_SceneMenuName);

		// H hides the selection, or shows it if all of it is hidden; by hover, as the Viewport does, so only one panel acts
		if (ImGui::IsWindowHovered() && !ImGui::GetIO().WantTextInput && ImGui::IsKeyChordPressed(ImGuiKey_H))
			ToggleHidden();

		ImVec2 available = ImGui::GetContentRegionAvail();
		ImGui::Dummy(available);
		DragDropTarget(Entity());

		ApplyDrop();
	}
	ImGui::End();
}

/* ------------------------------------------------------------------------------------------------------------------ */

void HierarchyPanel::OnEvent(Event& event)
{
	EventDispatcher dispatcher(event);
	dispatcher.Dispatch<AppOpenDocumentChangedEvent>([=](AppOpenDocumentChangedEvent&)
		{
			m_ContextEntity = Entity();
			m_PendingDrop.reset();
			m_ClosedNodes.clear();
			return false;
		});
}

/* ------------------------------------------------------------------------------------------------------------------ */

void HierarchyPanel::BuildRows(Scene* scene)
{
	PROFILE_FUNCTION();

	m_Rows.clear();
	entt::registry& registry = scene->GetRegistry();

	// Filtered rows are every match, listed flat
	if (m_TextFilter->IsActive())
	{
		for (entt::entity entity : FilteredEntities(registry))
			m_Rows.push_back({ entity, 0, false });
		return;
	}

	for (Entity root : SceneGraph::GetRoots(*scene))
		AddRows(registry, root.GetHandle(), 0);
}

std::vector<entt::entity> HierarchyPanel::FilteredEntities(entt::registry& registry) const
{
	std::vector<entt::entity> matches;
	registry.each([&](entt::entity entity)
		{
			const NameComponent* nameComp = registry.try_get<NameComponent>(entity);
			if (nameComp && m_TextFilter->PassFilter(nameComp->name.c_str()))
				matches.push_back(entity);
		});
	return matches;
}

void HierarchyPanel::ApplySelectionRequests(const ImGuiMultiSelectIO& io, Scene& scene)
{
	if (m_SkipSelectionRequests)
		return;

	const int rowCount = (int)m_Rows.size();
	for (const ImGuiSelectionRequest& request : io.Requests)
	{
		if (request.Type == ImGuiSelectionRequestType_SetAll)
		{
			std::vector<Uuid> ids;
			if (request.Selected)
			{
				for (const Row& row : m_Rows)
					ids.push_back(Entity{ row.entity, &scene }.GetID());
			}
			m_Selection->SetAll(std::move(ids));
		}
		else if (request.Type == ImGuiSelectionRequestType_SetRange)
		{
			// First is never after last; the direction says which end was clicked, which becomes active
			const int first = (int)request.RangeFirstItem;
			const int last = (int)request.RangeLastItem;
			const bool backwards = request.RangeDirection < 0;
			for (int n = 0; n <= last - first; ++n)
			{
				const int i = backwards ? last - n : first + n;
				if (i < 0 || i >= rowCount)
					continue;
				const Uuid id = Entity{ m_Rows[i].entity, &scene }.GetID();
				if (request.Selected)
					m_Selection->Add(id);
				else
					m_Selection->Remove(id);
			}
		}
	}
}

void HierarchyPanel::AddRows(entt::registry& registry, entt::entity entity, int depth)
{
	const HierarchyComponent* hierarchyComp = registry.try_get<HierarchyComponent>(entity);
	bool hasChildren = hierarchyComp && hierarchyComp->firstChild != entt::null;
	m_Rows.push_back({ entity, depth, hasChildren });

	if (!hasChildren || m_ClosedNodes.count(entity))
		return;

	for (entt::entity child = hierarchyComp->firstChild; child != entt::null;)
	{
		AddRows(registry, child, depth + 1);
		const HierarchyComponent* childHierarchyComp = registry.try_get<HierarchyComponent>(child);
		ASSERT(childHierarchyComp != nullptr, "Child does not have a Hierarchy Component!");
		child = childHierarchyComp ? childHierarchyComp->nextSibling : entt::null;
	}
}

void HierarchyPanel::DrawRow(Scene* scene, const Row& row, int index)
{
	Entity entity{ row.entity, scene };
	const std::string& name = entity.GetName();
	float indent = row.depth * ImGui::GetStyle().IndentSpacing;

	ImGui::PushID((int)(uint32_t)row.entity);
	if (indent > 0.0f)
		ImGui::Indent(indent);

	HiddenComponent* hiddenComp = entity.TryGetComponent<HiddenComponent>();
	bool isHidden = hiddenComp && hiddenComp->hidden;
	if (ImGui::SmallButton(isHidden ? ICON_FA_EYE_SLASH : ICON_FA_EYE))
		HistoryCommands::SetHidden(*m_History, entity, !isHidden);
	ImGui::SameLine();

	bool isOpen = row.hasChildren && !m_ClosedNodes.count(row.entity);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen
		| (m_Selection->Contains(entity.GetID()) ? ImGuiTreeNodeFlags_Selected : 0)
		| (row.hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf);

	if (row.hasChildren)
		ImGui::SetNextItemOpen(isOpen);
	ImGui::SetNextItemSelectionUserData(index);
	bool opened = ImGui::TreeNodeEx("node", flags, "%s", name.c_str());

	if (row.hasChildren && opened != isOpen)
	{
		if (opened)
			m_ClosedNodes.erase(row.entity);
		else
			m_ClosedNodes.insert(row.entity);
	}

	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		// The top-level selection if this row is selected, otherwise this row alone
		const ImGuiPayload* current = ImGui::GetDragDropPayload();
		if (!current || !current->Data)
		{
			std::vector<Uuid> ids;
			if (m_Selection->Contains(entity.GetID()))
			{
				for (Entity selected : HistoryCommands::TopLevelInOrder(m_Selection->Resolve(*scene)))
					ids.push_back(selected.GetID());
			}
			else
				ids.push_back(entity.GetID());
			ImGui::SetDragDropPayload("Entities", ids.data(), ids.size() * sizeof(Uuid));
			current = ImGui::GetDragDropPayload();
		}
		m_DraggedRow = entity.GetID();
		const size_t count = current->DataSize / sizeof(Uuid);
		if (count > 1)
			ImGui::Text("%zu entities", count);
		else
			ImGui::TextUnformatted(name.c_str());
		ImGui::EndDragDropSource();
	}
	// Below an open node is its first child, so there's no drop after it
	RowDragDropTarget(entity, !isOpen);

	// ImGui selects an unselected row on right-click and keeps the selection on a selected one.
	// A double-click on a row that was selected keeps the selection from before its first click, to frame all of it.
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		const Uuid id = entity.GetID();
		const std::vector<Uuid>& before = m_SelectionBeforeClick.ids;
		if (std::find(before.begin(), before.end(), id) != before.end())
			m_Selection->SetAll(before, id);
		m_SkipNextRelease = true;
		m_Focused = true;
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
	{
		m_ContextEntity = entity;
		m_OpenContextMenu = true;
	}

	if (indent > 0.0f)
		ImGui::Unindent(indent);
	ImGui::PopID();
}

/* ------------------------------------------------------------------------------------------------------------------ */

void HierarchyPanel::RowDragDropTarget(Entity entity, bool canDropAfter)
{
	const ImGuiPayload* peek = ImGui::GetDragDropPayload();
	if (!peek || !peek->IsDataType("Entities"))
		return;

	if (!ImGui::BeginDragDropTarget())
		return;

	// No drops onto a dragged entity or its descendants
	Scene* scene = entity.GetScene();
	const Uuid* dragged = (const Uuid*)peek->Data;
	const size_t count = peek->DataSize / sizeof(Uuid);
	for (size_t i = 0; i < count; ++i)
	{
		Entity draggedEntity = scene->GetEntityByID(dragged[i]);
		if (!draggedEntity || IsSelfOrDescendant(scene->GetRegistry(), entity.GetHandle(), draggedEntity.GetHandle()))
		{
			ImGui::EndDragDropTarget();
			return;
		}
	}

	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entities", ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
	{
		// Top quarter drops before, bottom quarter after, the rest onto
		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();
		float t = (ImGui::GetMousePos().y - min.y) / std::max(max.y - min.y, 1.0f);
		DropPosition position = t < 0.25f ? DropPosition::Before
			: (canDropAfter && t > 0.75f) ? DropPosition::After : DropPosition::Onto;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImU32 colour = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
		if (position == DropPosition::Before)
			drawList->AddLine(min, ImVec2(max.x, min.y), colour, 2.0f);
		else if (position == DropPosition::After)
			drawList->AddLine(ImVec2(min.x, max.y), max, colour, 2.0f);
		else
			drawList->AddRect(min, max, colour, 0.0f, 0, 2.0f);

		if (payload->IsDelivery())
			m_PendingDrop = PendingDrop{ std::vector<Uuid>(dragged, dragged + count), entity, position };
	}
	ImGui::EndDragDropTarget();
}

void HierarchyPanel::DragDropTarget(Entity parent)
{
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entities", ImGuiDragDropFlags_None))
		{
			const Uuid* dragged = (const Uuid*)payload->Data;
			m_PendingDrop = PendingDrop{ std::vector<Uuid>(dragged, dragged + payload->DataSize / sizeof(Uuid)), parent, DropPosition::Onto };
		}
		ImGui::EndDragDropTarget();
	}
}

void HierarchyPanel::ApplyDrop()
{
	if (!m_PendingDrop)
		return;

	PendingDrop drop = *m_PendingDrop;
	m_PendingDrop.reset();

	if (Scene* scene = SceneManager::CurrentScene())
		SelectionOperations::Move(*m_History, *scene, drop.dragged, drop.target, drop.position);
}

/* ------------------------------------------------------------------------------------------------------------------ */

void HierarchyPanel::EntityContextMenu()
{
	if (m_OpenContextMenu)
	{
		ImGui::OpenPopup(s_ContextMenuName);
		m_OpenContextMenu = false;
	}

	if (!ImGui::BeginPopup(s_ContextMenuName))
		return;

	if (!m_ContextEntity.BelongsToScene(SceneManager::CurrentScene()))
	{
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	m_CreateParent = m_ContextEntity;
	if (ImGui::BeginMenu("Create")) {
		CreateMenu();
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem(ICON_FA_SCISSORS" Cut", ShortcutLabel("X").c_str(), nullptr, HasSelection()))
		Cut();
	if (ImGui::MenuItem(ICON_FA_COPY" Copy", ShortcutLabel("C").c_str(), nullptr, HasSelection()))
		Copy();
	if (ImGui::MenuItem(ICON_FA_PASTE" Paste", ShortcutLabel("V").c_str(), nullptr, ImGui::GetClipboardText() != nullptr))
		Paste();
	if (ImGui::MenuItem(ICON_FA_CLONE" Duplicate", ShortcutLabel("D").c_str(), nullptr, HasSelection()))
		Duplicate();
	if (ImGui::MenuItem(ICON_FA_TRASH_CAN" Delete", "Del", nullptr, HasSelection()))
		Delete();
	ImGui::Separator();
	if (ImGui::MenuItem(ICON_FA_EYE_SLASH" Hide or Show", "H", nullptr, HasSelection()))
		ToggleHidden();
	ImGui::EndPopup();
}

void HierarchyPanel::CreateMenu()
{
	if (ImGui::MenuItem("Create Empty Entity"))
	{
		Entity entity = SceneManager::CurrentScene()->CreateEntity("New Entity");//TODO: make all the entities have a unique name
		AddCreated(entity);
	}
	if (ImGui::MenuItem("Create World entity"))
	{
		Entity entity = SceneManager::CurrentScene()->CreateEntity("New Entity");
		entity.AddComponent<TransformComponent>();
		AddCreated(entity);
	}
	if (ImGui::BeginMenu("3D Object"))
	{
		if (ImGui::MenuItem("Cube"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Cube");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Cube);
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Sphere"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Sphere");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Sphere);
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Plane"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Plane");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Plane);
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Cylinder"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Cylinder");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Cylinder);
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Cone"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Cone");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Cone);
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Torus"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Torus");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Torus);
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Terrain", "", nullptr, false))
		{
			//TODO: Create terrain
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("2D Object"))
	{
		if (ImGui::MenuItem("Sprite"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Sprite");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<SpriteComponent>();
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Animated Sprite"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Animated Sprite");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<AnimatedSpriteComponent>();
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Circle"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Circle");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<CircleRendererComponent>();
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Tilemap"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Tilemap");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<TilemapComponent>();
			AddCreated(entity);
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Audio"))
	{
		if (ImGui::MenuItem("Audio Source"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Audio Source");
			entity.AddComponent<AudioSourceComponent>();
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Audio Listener"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Audio Listener");
			entity.AddComponent<AudioListenerComponent>();
			AddCreated(entity);
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Light"))
	{
		if (ImGui::MenuItem("Point Light"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Point Light");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PointLightComponent>();
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Directional Light", "", nullptr, false))
		{
			//TODO: Create Directional light
		}

		if (ImGui::MenuItem("Spot Light", "", nullptr, false))
		{
			//TODO: Create Spot light
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Particle Emitter", "", nullptr, false))
	{
		//TODO: create a particle emitter component
	}
	if (ImGui::MenuItem("Camera"))
	{
		Entity entity = SceneManager::CurrentScene()->CreateEntity("Camera");
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<CameraComponent>();
		AddCreated(entity);
	}
	if (ImGui::BeginMenu("UI"))
	{
		if (ImGui::MenuItem("Canvas"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Canvas");
			entity.AddComponent<CanvasComponent>();
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Button"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Button");
			entity.AddComponent<WidgetComponent>();
			entity.AddComponent<ButtonComponent>();
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Stack"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Stack");
			entity.AddComponent<WidgetComponent>();
			entity.AddComponent<StackLayoutComponent>();
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Grid"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Grid");
			entity.AddComponent<WidgetComponent>();
			entity.AddComponent<GridLayoutComponent>();
			AddCreated(entity);
		}
		if (ImGui::MenuItem("Scroll Box"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Scroll Box");
			entity.AddComponent<WidgetComponent>();
			entity.AddComponent<ScrollBoxComponent>();
			AddCreated(entity);
		}
		ImGui::EndMenu();
	}
}

void HierarchyPanel::AddCreated(Entity entity)
{
	Scene& scene = *entity.GetScene();
	const SelectionState before = m_Selection->Capture();
	if (m_CreateParent.BelongsToScene(&scene))
		m_CreateParent.AddChild(entity);
	m_Selection->Select(entity.GetID());
	m_History->Push(scene, CreateScope<CreateEntitiesCommand>(std::vector<Entity>{ entity }), &before);
}

void HierarchyPanel::Copy()
{
	Scene* scene = SceneManager::CurrentScene();
	const std::string text = scene ? SelectionOperations::Copy(*m_Selection, *scene) : std::string();
	if (text.empty())
		return;

	CLIENT_DEBUG("Copied");
	ImGui::SetClipboardText(text.c_str());
}

void HierarchyPanel::Cut()
{
	Copy();
	Delete();
}

void HierarchyPanel::Paste()
{
	const char* text = ImGui::GetClipboardText();
	Scene* scene = SceneManager::CurrentScene();
	if (text && scene)
		SelectionOperations::Paste(*m_History, *m_Selection, *scene, text);
}

void HierarchyPanel::Duplicate()
{
	if (Scene* scene = SceneManager::CurrentScene())
	{
		CLIENT_DEBUG("Duplicated");
		SelectionOperations::Duplicate(*m_History, *m_Selection, *scene);
	}
}

void HierarchyPanel::Delete()
{
	if (Scene* scene = SceneManager::CurrentScene())
		SelectionOperations::Delete(*m_History, *m_Selection, *scene);
}

void HierarchyPanel::ToggleHidden()
{
	if (Scene* scene = SceneManager::CurrentScene())
		SelectionOperations::ToggleHidden(*m_History, *m_Selection, *scene);
}

bool HierarchyPanel::HasSelection() const
{
	Scene* scene = SceneManager::CurrentScene();
	return scene && m_Selection->HasLive(*scene);
}

void HierarchyPanel::SelectAll()
{
	Scene* scene = SceneManager::CurrentScene();
	if (!scene)
		return;

	std::vector<Uuid> ids;
	if (m_TextFilter->IsActive() && MainDockSpace::IsFocussedWindow(this))
	{
		for (entt::entity entity : FilteredEntities(scene->GetRegistry()))
			ids.push_back(Entity{ entity, scene }.GetID());
	}
	else
	{
		for (Entity entity : HistoryCommands::AllInHierarchyOrder(*scene))
			ids.push_back(entity.GetID());
	}
	m_Selection->SetAll(std::move(ids));
}

void HierarchyPanel::SelectNone()
{
	m_Selection->Clear();
}

void HierarchyPanel::InvertSelection()
{
	Scene* scene = SceneManager::CurrentScene();
	if (!scene)
		return;

	std::vector<Uuid> all;
	for (Entity entity : HistoryCommands::AllInHierarchyOrder(*scene))
		all.push_back(entity.GetID());
	m_Selection->Invert(all);
}

bool HierarchyPanel::IsReadOnly() const
{
	return false;
}
