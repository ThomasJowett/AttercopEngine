# Camera

The point of view the scene is rendered from.

At runtime the scene is drawn from the camera marked `Primary`. A camera is either
orthographic, which suits 2D games, or perspective, which suits 3D. The camera looks along the
entity's transform.

## Scene file

Saved as a `<Camera>` element inside the entity's `<Entity>` element. Attributes that are left out keep their default. See [Scene Files](../../scene-files.md) for how values like vectors, colours and file paths are written.

```xml
<Camera Primary="true" FixedAspectRatio="false" Projection="Orthographic"
        OrthoSize="10" OrthoNear="-1" OrthoFar="1"
        PerspectiveNear="1" PerspectiveFar="1000" Fov="1.5707964"/>
```

| Name | Where | Type | Default |
| --- | --- | --- | --- |
| `Primary` | attribute | bool | `true` |
| `FixedAspectRatio` | attribute | bool | `false` |
| `Projection` | attribute | `Perspective` or `Orthographic` | `Perspective` |
| `OrthoSize` | attribute | float, visible height in world units | `10` |
| `OrthoNear` / `OrthoFar` | attributes | float | `-1` / `1` |
| `PerspectiveNear` / `PerspectiveFar` | attributes | float | `1` / `1000` |
| `Fov` | attribute | float, vertical field of view in radians | `1.5707964` (90°) |
| `AspectRatio` | attribute | float, width / height; only written and used when `FixedAspectRatio` is true | `1.7777778` |

In Lua, `camera.Camera` returns a copy of the projection settings. Change the copy and then
assign it back:

```lua
local cameraComp = CurrentEntity:GetCameraComponent()
local camera = cameraComp.Camera
camera:SetOrthoSize(20)
cameraComp.Camera = camera
```

See [Camera](../../LuaAPI/Camera.md) for its functions.

--8<-- "LuaAPI/_fragments/CameraComponent.md"
