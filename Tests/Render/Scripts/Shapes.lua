-- Lines drawn through the debug pass: hairlines, outlines and poly lines
function OnDebugRender()
	Debug.DrawLine(Vec3.new(-15.5, -8.5, 0), Vec3.new(-8.5, -1.5, 0), Colour.new(1, 1, 1, 1))
	Debug.DrawRect(Vec3.new(-12, 4, 0), Vec2.new(6, 4), Colour.new(1, 0.8, 0.2, 1))
	Debug.DrawCircle(Vec3.new(-12, 4, 0), 1.5, 32, Colour.new(0.3, 0.8, 1, 1))
	Debug.DrawPolyline({ Vec3.new(-6, -7, 0), Vec3.new(-3, -2, 0), Vec3.new(0, -7, 0), Vec3.new(3, -2, 0) }, 6, Colour.new(0.9, 0.3, 0.6, 1), false)
	Debug.DrawPolyline({ Vec3.new(6, -7, 0), Vec3.new(14, -7, 0), Vec3.new(10, -2, 0) }, 4, Colour.new(0.4, 1, 0.4, 0.75), true)
end
