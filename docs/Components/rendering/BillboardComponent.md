# Billboard

Keeps an entity facing the camera.

Use it for sprites, text and models in a 3D scene that should always face the viewer. Each view
(the game camera, the editor viewport, the camera preview) turns it towards its own camera.

To keep something at a fixed place in front of the camera, make it a child of the camera entity
instead. For screen-anchored interfaces, use the in-game UI.

- **Orientation:** Camera turns it to point at the camera; World Up only turns it around the
  vertical axis, so it stays upright.
- **Rotation:** the entity's own rotation is applied on top of facing the camera, so a rotation of
  180° around Y shows its back. Its saved rotation is never changed, and the gizmo edits it as
  usual.

## Scene file

Saved as a `<Billboard>` element inside the entity's `<Entity>` element. Attributes that are left out keep their default. See [Scene Files](../../scene-files.md) for how values like vectors, colours and file paths are written.

```xml
<Billboard Orientation="1"/>
```

| Name | Where | Type | Default |
| --- | --- | --- | --- |
| `Orientation` | attribute | int: `0` stays upright (turns around world up), `1` points at the camera | `1` |

--8<-- "LuaAPI/_fragments/BillboardComponent.md"
