#include "HierarchyPanel.h"

#include "imgui/imgui.h"
#include "IconsFontAwesome6.h"
#include "History/HistoryCommands.h"

#include "Scene/SceneGraph.h"
#include "Scene/SceneSerializer.h"
#include "Scene/SceneManager.h"
#include "ImGui/ImGuiUtilities.h"

#include "MainDockSpace.h"

namespace
{
	constexpr const char* s_ContextMenuName = "EntityContextMenu";

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

HierarchyPanel::HierarchyPanel(bool* show)
	:Layer("Hierarchy"), m_Show(show), m_TextFilter(CreateScope<ImGuiTextFilter>())
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

		if ((ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()) || !m_SelectedEntity.BelongsToScene(SceneManager::CurrentScene()))
		{
			m_SelectedEntity = {};
		}

		if (!SceneManager::IsSceneLoaded())
		{
			ImGui::TextUnformatted("No scene loaded");
			ImGui::End();
			return;
		}

		// right click on a blank space
		if (ImGui::BeginPopupContextWindow())
		{
			m_SelectedEntity = {};
			CreateMenu();
			ImGui::EndPopup();
		}

		ImGui::TextUnformatted(ICON_FA_MAGNIFYING_GLASS);
		ImGui::SameLine();
		m_TextFilter->Draw("##Search", ImGui::GetContentRegionAvail().x);
		ImGui::Tooltip("Filter (\"incl,-excl\")");

		Scene* scene = SceneManager::CurrentScene();

		if (m_SelectedEntity && !m_SelectedEntity.IsSceneValid())
			m_SelectedEntity = Entity();

		if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_Bullet
			| ImGuiTreeNodeFlags_OpenOnDoubleClick))
		{
			DragDropTarget(Entity());

			BuildRows(scene);

			ImGuiListClipper clipper;
			clipper.Begin((int)m_Rows.size());

			// Keep the dragged row submitted so its drag source stays alive while scrolled away
			if (const ImGuiPayload* payload = ImGui::GetDragDropPayload(); payload && payload->IsDataType("Entity"))
			{
				entt::entity dragged = ((const Entity*)payload->Data)->GetHandle();
				for (size_t i = 0; i < m_Rows.size(); ++i)
				{
					if (m_Rows[i].entity == dragged)
					{
						clipper.IncludeItemByIndex((int)i);
						break;
					}
				}
			}

			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
					DrawRow(scene, m_Rows[i]);
			}

			EntityContextMenu();
			ImGui::TreePop();
		}

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
			// Clear the selected entity when project changes
			m_SelectedEntity = Entity();
			m_ContextEntity = Entity();
			m_PendingDrop.reset();
			m_ClosedNodes.clear();
			return false;
		});
}

/* ------------------------------------------------------------------------------------------------------------------ */

Entity HierarchyPanel::GetSelectedEntity()
{
	return m_SelectedEntity;
}

void HierarchyPanel::SetSelectedEntity(Entity entity)
{
	m_SelectedEntity = entity;
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
		registry.each([&](entt::entity entity)
			{
				const NameComponent* nameComp = registry.try_get<NameComponent>(entity);
				if (nameComp && m_TextFilter->PassFilter(nameComp->name.c_str()))
					m_Rows.push_back({ entity, 0, false });
			});
		return;
	}

	registry.each([&](entt::entity entity)
		{
			const HierarchyComponent* hierarchyComp = registry.try_get<HierarchyComponent>(entity);
			if (!hierarchyComp || hierarchyComp->parent == entt::null)
				AddRows(registry, entity, 0);
		});
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

void HierarchyPanel::DrawRow(Scene* scene, const Row& row)
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
	{
		entity.GetOrAddComponent<HiddenComponent>().hidden = !isHidden;
		scene->MakeDirty();
	}
	ImGui::SameLine();

	bool isOpen = row.hasChildren && !m_ClosedNodes.count(row.entity);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen
		| ((m_SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0)
		| (row.hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf);

	if (row.hasChildren)
		ImGui::SetNextItemOpen(isOpen);
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
		ImGui::SetDragDropPayload("Entity", &entity, sizeof(Entity));
		ImGui::TextUnformatted(name.c_str());
		ImGui::EndDragDropSource();
	}
	// Below an open node is its first child, so there's no drop after it
	RowDragDropTarget(entity, !isOpen);

	if (ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		m_SelectedEntity = entity;
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			m_Focused = true;
		}
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
	if (!peek || !peek->IsDataType("Entity"))
		return;

	// No drops onto the dragged entity or its descendants
	const Entity* dragged = (const Entity*)peek->Data;
	if (!dragged->BelongsToScene(entity.GetScene())
		|| IsSelfOrDescendant(entity.GetScene()->GetRegistry(), entity.GetHandle(), dragged->GetHandle()))
		return;

	if (!ImGui::BeginDragDropTarget())
		return;

	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity", ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
	{
		ASSERT(payload->DataSize == sizeof(Entity), "Drag-drop entity data not the correct size");

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
			m_PendingDrop = PendingDrop{ *(const Entity*)payload->Data, entity, position };
	}
	ImGui::EndDragDropTarget();
}

void HierarchyPanel::DragDropTarget(Entity parent)
{
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity", ImGuiDragDropFlags_None))
		{
			ASSERT(payload->DataSize == sizeof(Entity), "Drag-drop entity data not the correct size");
			m_PendingDrop = PendingDrop{ *(const Entity*)payload->Data, parent, DropPosition::Onto };
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

	Scene* scene = SceneManager::CurrentScene();
	if (!drop.dragged.BelongsToScene(scene) || (drop.target && !drop.target.BelongsToScene(scene)))
		return;

	entt::registry& registry = scene->GetRegistry();
	if (drop.target && IsSelfOrDescendant(registry, drop.target.GetHandle(), drop.dragged.GetHandle()))
		return;

	const HierarchyComponent* targetHierarchyComp = drop.target ? drop.target.TryGetComponent<HierarchyComponent>() : nullptr;
	bool targetIsRoot = !targetHierarchyComp || targetHierarchyComp->parent == entt::null;

	if (drop.position != DropPosition::Onto && !targetIsRoot)
	{
		bool before = drop.position == DropPosition::Before;
		Ref<MoveSiblingCommand> moveCommand = CreateRef<MoveSiblingCommand>(drop.dragged, drop.target, before);
		if (before)
			SceneGraph::MoveBefore(drop.dragged, drop.target);
		else
			SceneGraph::MoveAfter(drop.dragged, drop.target);
		HistoryManager::AddHistoryRecord(moveCommand);
		scene->MakeDirty();
		return;
	}

	// Roots are listed in the registry's order, so a drop beside one moves to the root
	Entity parent = drop.position == DropPosition::Onto ? drop.target : Entity();

	const HierarchyComponent* draggedHierarchyComp = drop.dragged.TryGetComponent<HierarchyComponent>();
	entt::entity currentParent = draggedHierarchyComp ? draggedHierarchyComp->parent : entt::null;
	if (currentParent == parent.GetHandle())
		return;

	Ref<ReparentEntityCommand> reparentCommand = CreateRef<ReparentEntityCommand>(drop.dragged, parent);
	if (parent)
		SceneGraph::Reparent(drop.dragged, parent);
	else
		SceneGraph::Unparent(drop.dragged);
	HistoryManager::AddHistoryRecord(reparentCommand);
	scene->MakeDirty();
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

	m_SelectedEntity = m_ContextEntity;
	if (ImGui::BeginMenu("Create")) {
		CreateMenu();
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem(ICON_FA_SCISSORS" Cut", "Ctrl + X", nullptr, HasSelection()))
		Cut();
	if (ImGui::MenuItem(ICON_FA_COPY" Copy", "Ctrl + C", nullptr, HasSelection()))
		Copy();
	if (ImGui::MenuItem(ICON_FA_PASTE" Paste", "Ctrl + V", nullptr, ImGui::GetClipboardText() != nullptr))
		Paste();
	if (ImGui::MenuItem(ICON_FA_CLONE" Duplicate", "Ctrl + D", nullptr, HasSelection()))
		Duplicate();
	if (ImGui::MenuItem(ICON_FA_TRASH_CAN" Delete", "Del", nullptr, HasSelection()))
		Delete();
	ImGui::EndPopup();
}

void HierarchyPanel::CreateMenu()
{
	if (ImGui::MenuItem("Create Empty Entity"))
	{
		Entity entity = SceneManager::CurrentScene()->CreateEntity("New Entity");//TODO: make all the entities have a unique name
		HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
		if (m_SelectedEntity) {
			m_SelectedEntity.AddChild(entity);
		}
		m_SelectedEntity = entity;
	}
	if (ImGui::MenuItem("Create World entity"))
	{
		Entity entity = SceneManager::CurrentScene()->CreateEntity("New Entity");
		entity.AddComponent<TransformComponent>();
		HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
		if (m_SelectedEntity) {
			m_SelectedEntity.AddChild(entity);
		}
		m_SelectedEntity = entity;
	}
	if (ImGui::BeginMenu("3D Object"))
	{
		if (ImGui::MenuItem("Cube"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Cube");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Cube);
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Sphere"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Sphere");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Sphere);
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Plane"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Plane");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Plane);
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Cylinder"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Cylinder");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Cylinder);
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Cone"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Cone");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Cone);
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Torus"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Torus");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<PrimitiveComponent>(PrimitiveComponent::Shape::Torus);
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
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
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Animated Sprite"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Animated Sprite");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<AnimatedSpriteComponent>();
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Circle"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Circle");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<CircleRendererComponent>();
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Tilemap"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Tilemap");
			entity.AddComponent<TransformComponent>();
			entity.AddComponent<TilemapComponent>();
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Audio"))
	{
		if (ImGui::MenuItem("Audio Source"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Audio Source");
			entity.AddComponent<AudioSourceComponent>();
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Audio Listener"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Audio Listener");
			entity.AddComponent<AudioListenerComponent>();
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity)
				m_SelectedEntity.AddChild(entity);
			m_SelectedEntity = entity;
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
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
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
		HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
		if (m_SelectedEntity) {
			m_SelectedEntity.AddChild(entity);
		}
		m_SelectedEntity = entity;
	}
	if (ImGui::BeginMenu("UI"))
	{
		if (ImGui::MenuItem("Canvas"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Canvas");
			entity.AddComponent<CanvasComponent>();
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Button"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Button");
			entity.AddComponent<WidgetComponent>();
			entity.AddComponent<ButtonComponent>();
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Stack"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Stack");
			entity.AddComponent<WidgetComponent>();
			entity.AddComponent<StackLayoutComponent>();
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Grid"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Grid");
			entity.AddComponent<WidgetComponent>();
			entity.AddComponent<GridLayoutComponent>();
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		if (ImGui::MenuItem("Scroll Box"))
		{
			Entity entity = SceneManager::CurrentScene()->CreateEntity("Scroll Box");
			entity.AddComponent<WidgetComponent>();
			entity.AddComponent<ScrollBoxComponent>();
			HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(entity));
			if (m_SelectedEntity) {
				m_SelectedEntity.AddChild(entity);
			}
			m_SelectedEntity = entity;
		}
		ImGui::EndMenu();
	}
}

void HierarchyPanel::Undo(int asteps)
{
	HistoryManager::Undo(asteps);
}

void HierarchyPanel::Redo(int asteps)
{
	HistoryManager::Redo(asteps);
}

bool HierarchyPanel::CanUndo() const
{
	return HistoryManager::CanUndo();
}

bool HierarchyPanel::CanRedo() const
{
	return HistoryManager::CanRedo();
}

void HierarchyPanel::Copy()
{
	CLIENT_DEBUG("Copied");
	ImGui::SetClipboardText(SceneSerializer::SerializeEntity(m_SelectedEntity).c_str());
}

void HierarchyPanel::Cut()
{
	Copy();
	Delete();
}

void HierarchyPanel::Paste()
{
	m_SelectedEntity = SceneSerializer::DeserializeEntity(SceneManager::CurrentScene(), ImGui::GetClipboardText(), true);
	if (m_SelectedEntity)
		HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(m_SelectedEntity));
}

void HierarchyPanel::Duplicate()
{
	CLIENT_DEBUG("Duplicated");
	m_SelectedEntity = SceneManager::CurrentScene()->DuplicateEntity(m_SelectedEntity, Entity());
	m_SelectedEntity.GetName() += "_copy";
	HistoryManager::AddHistoryRecord(CreateRef<AddEntityCommand>(m_SelectedEntity));
}

void HierarchyPanel::Delete()
{
	HistoryManager::AddHistoryRecord(CreateRef<RemoveEntityCommand>(m_SelectedEntity));
	m_SelectedEntity.Destroy();
	m_SelectedEntity = {};
}

bool HierarchyPanel::HasSelection() const
{
	return m_SelectedEntity;
}

void HierarchyPanel::SelectAll()
{
	CLIENT_ERROR("Multi select entities not implemented yet!");
}

bool HierarchyPanel::IsReadOnly() const
{
	return false;
}