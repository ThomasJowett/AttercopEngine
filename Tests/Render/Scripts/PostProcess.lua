-- Blurs the whole scene with the Gaussian blur chain
function OnCreate()
	PostProcess.AddEffect(PostProcess.CreateEffect("GaussianBlur", 2))
end
