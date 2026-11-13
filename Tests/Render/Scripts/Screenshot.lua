-- Takes a screenshot named after this entity once the scene has settled, then ends the test
local frame = 0

function OnUpdate(deltaTime)
	frame = frame + 1
	if frame == 5 then
		local name = CurrentEntity:GetName()
		Test.Screenshot(name)
		Test.Pass("screenshot " .. name)
	end
end
