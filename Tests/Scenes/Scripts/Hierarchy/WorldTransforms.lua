-- A child's world position is its parent's world transform applied to its local position, through any number of
-- levels. Parent sits at (10, 0), turned a quarter turn and doubled in size, so a child's local (1, 0) lands at
-- (10, 2): the local offset is scaled to (2, 0), then rotated up to (0, 2). Entities are built in frame 1; most
-- checks run in later frames, and the last ones in the same frame as the change.

local Epsilon = 0.01
local frame = 0
local parent, child, grandchild, other

local function Near(entity, x, y, message)
	local world = entity:GetTransformComponent():GetWorldPosition()
	Test.Expect(math.abs(world.x - x) <= Epsilon and math.abs(world.y - y) <= Epsilon,
		message .. " (expected " .. x .. ", " .. y .. " but got " .. string.format("%.2f, %.2f", world.x, world.y) .. ")")
end

local function New(name, x, y)
	local entity = CurrentScene:CreateEntity(name)
	entity:AddTransformComponent().Position = Vec3.new(x, y, 0)
	return entity
end

function OnUpdate(deltaTime)
	frame = frame + 1
	if frame == 1 then
		parent = New("Parent", 10, 0)
		child = New("Child", 1, 0)
		grandchild = New("Grandchild", 1, 0)
		other = New("Other", -5, 0)
		parent:AddChild(child)
		child:AddChild(grandchild)

		local transform = parent:GetTransformComponent()
		transform.Rotation = Vec3.new(0, 0, math.pi / 2)
		transform.Scale = Vec3.new(2, 2, 1)
	elseif frame == 3 then
		Near(parent, 10, 0, "a parent with no parent sits at its own position")
		Near(child, 10, 2, "the child is scaled and rotated by its parent")
		Near(grandchild, 10, 4, "the grandchild is scaled and rotated by both ancestors")
		Test.Equal(child:GetParent():GetName(), "Parent", "the child's parent is Parent")
		Test.Equal(grandchild:GetParent():GetName(), "Child", "the grandchild's parent is Child")

		-- Moving the parent moves everything below it
		parent:GetTransformComponent().Position = Vec3.new(0, 5, 0)
	elseif frame == 5 then
		Near(child, 0, 7, "the child follows its parent when the parent moves")
		Near(grandchild, 0, 9, "the grandchild follows too")

		-- Turning the parent back removes the rotation but keeps the scale
		parent:GetTransformComponent().Rotation = Vec3.new(0, 0, 0)
	elseif frame == 7 then
		Near(child, 2, 5, "the child's offset stays scaled once the rotation is removed")

		-- Giving the grandchild a new parent re-bases it onto that parent's transform
		other:AddChild(grandchild)
	elseif frame == 9 then
		Test.Equal(grandchild:GetParent():GetName(), "Other", "the grandchild's parent is now Other")
		Near(grandchild, -4, 0, "the grandchild is positioned relative to its new parent")
		Near(child, 2, 5, "the old parent's child is unaffected by the move")

		-- World positions are up to date as soon as a script changes something, without waiting a frame
		parent:GetTransformComponent().Position = Vec3.new(1, 1, 0)
		Near(child, 3, 1, "the child follows its parent in the same frame")
		child:AddChild(grandchild)
		Near(grandchild, 5, 1, "a reparented entity is placed under its new parent in the same frame")

		-- Moving the camera's parent shifts screen conversions in the same frame too
		local rig = New("Rig", 0, 0)
		rig:AddChild(CurrentScene:FindEntity("Main Camera"))
		local before = CurrentScene:WorldToScreenPoint(Vec3.new(0, 0, 0))
		rig:GetTransformComponent().Position = Vec3.new(5, 0, 0)
		local after = CurrentScene:WorldToScreenPoint(Vec3.new(5, 0, 0))
		Test.Expect(math.abs(after.x - before.x) <= 0.5 and math.abs(after.y - before.y) <= 0.5,
			"a point moving with the camera stays at the same screen position (was "
			.. string.format("%.1f, %.1f, now %.1f, %.1f", before.x, before.y, after.x, after.y) .. ")")
		Test.Pass("WorldTransforms: checked the hierarchy")
	end
end
