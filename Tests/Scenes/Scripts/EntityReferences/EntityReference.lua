-- A weld to a body that isn't the entity's parent, ConnectedEntity from Lua, a copied welded pair and a destroyed target

local Epsilon = 0.05
local frame = 0
local held, holder, startY

local function Near(actual, expected, message)
	Test.Expect(math.abs(actual - expected) <= Epsilon, message .. " (expected " .. expected .. ", got " .. actual .. ")")
end

local function Y(entity)
	return entity:GetTransformComponent().Position.y
end

function OnUpdate(deltaTime)
	frame = frame + 1
	if frame == 2 then
		local weld = CurrentScene:FindEntity("Hanging"):GetWeldJoint2DComponent()
		Test.Expect(weld.ConnectedEntity ~= nil and weld.ConnectedEntity:GetName() == "Anchor", "the weld reads its target from Lua")

		-- Set from Lua and read back, then put back
		weld.ConnectedEntity = CurrentScene:FindEntity("Spare")
		Test.Expect(weld.ConnectedEntity:GetName() == "Spare", "ConnectedEntity set from Lua reads back")
		weld.ConnectedEntity = nil
		Test.Expect(weld.ConnectedEntity == nil, "setting nil clears ConnectedEntity")
		weld.ConnectedEntity = CurrentScene:FindEntity("Anchor")

		-- Rigid bodies are unparented once running, so the pair comes from a scene that isn't
		local pair = LoadScene("Scenes/EntityReferencePrefab.scene"):FindEntity("Pair")
		CurrentScene:InstantiateEntity(pair, Vec3.new(10, 5, 0))
		held = CurrentScene:FindEntity("Held")
		holder = CurrentScene:FindEntity("Holder")
		local copyWeld = held:GetWeldJoint2DComponent()
		Test.Expect(copyWeld.ConnectedEntity ~= nil and copyWeld.ConnectedEntity:GetID() == holder:GetID(),
			"the copied weld points at the copied Holder")
		Test.Expect(copyWeld.ConnectedEntity:GetID() ~= pair:GetChild():GetID(), "not at the prefab's Holder")
		startY = Y(held)
	elseif frame == 40 then
		Near(Y(CurrentScene:FindEntity("Hanging")), 5, "Hanging is held up by Anchor, which isn't its parent")
		Near(Y(held), startY, "the copied Held is held up by the copied Holder")

		Log.Info("EntityReference: destroying the copied Holder")
		holder:Destroy()
	elseif frame == 42 then
		Test.Expect(held:GetWeldJoint2DComponent().ConnectedEntity == nil, "ConnectedEntity is nil once its target is destroyed")
	elseif frame == 80 then
		Test.Expect(Y(held) < startY - 0.5, "Held falls once its weld's target is gone (y " .. Y(held) .. ")")
		Test.Pass("EntityReference: checked welds through entity references")
	end
end
