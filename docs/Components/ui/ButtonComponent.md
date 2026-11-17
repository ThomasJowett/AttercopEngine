# Button

A clickable UI element, with a different look for each state.

!!! warning "Work in progress"
    In game UI is still in development

A button needs a [Widget](WidgetComponent.md) on the same entity, under a
[Canvas](CanvasComponent.md). Each state (normal, hovered, clicked and disabled) can have its
own texture and tint. A [Lua Script](../scripting-ai/LuaScriptComponent.md) on the entity
receives `OnPressed()`, `OnReleased()`, `OnHovered()` and `OnUnHovered()`.

## Scene file

Saved as a `<Button>` element inside the entity's `<Entity>` element. Attributes that are left out keep their default. See [Scene Files](../../scene-files.md) for how values like vectors, colours and file paths are written.

```xml
<Button NormalFilterMethod="Linear" HoveredFilterMethod="Linear" ClickedFilterMethod="Nearest" DisabledFilterMethod="Nearest"
        NormalWrapMethod="Clamp" HoveredWrapMethod="Clamp" ClickedWrapMethod="Repeat" DisabledWrapMethod="Repeat">
    <NormalTexture Filepath="UI/Button.png"/>
    <HoveredTexture Filepath="UI/ButtonHover.png"/>
    <NormalTint R="1" G="1" B="1" A="1"/>
    <HoveredTint R="1" G="1" B="1" A="1"/>
    <ClickedTint R="0.8" G="0.8" B="0.8" A="1"/>
    <DisabledTint R="0.5" G="0.5" B="0.5" A="1"/>
</Button>
```

| Name | Where | Type | Default |
| --- | --- | --- | --- |
| `NormalFilterMethod` `HoveredFilterMethod` `ClickedFilterMethod` `DisabledFilterMethod` | attributes | `Linear` or `Nearest`, for that state's texture | `Nearest` |
| `NormalWrapMethod` `HoveredWrapMethod` `ClickedWrapMethod` `DisabledWrapMethod` | attributes | `Clamp`, `Mirror` or `Repeat`, for that state's texture | `Repeat` |
| `NormalTexture` `HoveredTexture` `ClickedTexture` `DisabledTexture` | children `Filepath`, optional | path to an image | none |
| `NormalTint` `HoveredTint` `ClickedTint` `DisabledTint` | child `R` `G` `B` `A` | float, 0-1 | white |

--8<-- "LuaAPI/_fragments/ButtonComponent.md"
