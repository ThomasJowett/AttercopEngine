# Sprite

Draws a textured or plain coloured quad.

The quad is 1×1 world units, sized by the entity's transform scale. With no texture it draws as
a solid block of `Tint`.

## Scene file

Saved as a `<Sprite>` element inside the entity's `<Entity>` element. Attributes that are left out keep their default. See [Scene Files](../../scene-files.md) for how values like vectors, colours and file paths are written.

```xml
<Sprite TilingFactor="1" FilterMethod="Nearest" WrapMethod="Repeat">
    <Tint R="1" G="1" B="1" A="1"/>
    <Texture Filepath="Sprites/Player.png"/>
</Sprite>
```

| Name | Where | Type | Default |
| --- | --- | --- | --- |
| `TilingFactor` | attribute | float, how many times the texture repeats | `1` |
| `FilterMethod` | attribute | `Linear` or `Nearest` | `Nearest` |
| `WrapMethod` | attribute | `Clamp`, `Mirror` or `Repeat` | `Repeat` |
| `Tint` | child `R` `G` `B` `A` | float, 0-1 | 1, 1, 1, 1 |
| `Texture` | child `Filepath`, optional | path to an image | none |

--8<-- "LuaAPI/_fragments/SpriteComponent.md"
