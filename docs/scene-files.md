# Scene Files

A scene is saved as an XML `.scene` file inside the project folder. The editor normally writes
these, but the format is simple enough to write by hand or generate with a tool. This page covers
the overall layout, and each [component](Components/index.md) page shows that component's own
element.

A prefab is a scene file too: spawn one from a script with
`CurrentScene:InstantiateScene(LoadScene("Scenes/Crate.scene"), position)` - see [Scene](LuaAPI/Scene.md).

## Layout

```xml
<Scene FormatVersion="2">
    <Gravity X="0" Y="-9.81"/>
    <Entity Name="Player" ID="1001-1">
        <Transform>...</Transform>
        <Sprite>...</Sprite>
        <Entity Name="Sword" ID="1001-2">
            ...
        </Entity>
    </Entity>
    <Entity Name="Ground" ID="1001-3">
        ...
    </Entity>
</Scene>
```

- **`<Scene>`** is the root. `FormatVersion` is the scene format version (currently `2`); A scene
  in the previous format is upgraded when it's loaded, and
  written in the current format when it's saved. A scene two or more formats behind is upgraded
  as far as possible, with a warning: open and save it in each version of Attercop in between to
  be sure. A scene from a newer version of Attercop loads as far as
  it can, with an error, and the Editor won't save over it.
- **`<Gravity>`** is the 2D physics gravity, in world units per second squared.
- **`<Entity>`** has a `Name` and an `ID` (see [ID](Components/core/IDComponent.md)), and holds
  one element per component.
- **Children** are written as `<Entity>` elements nested inside their parent, in order. This
  is the only way the parent and child relationship is stored.
- **Order:** entities are listed in the Hierarchy panel in the order they're written, top-level
  entities included.

An entity only gets an ID and a name automatically. Add a `<Transform>` to almost every entity,
because rendering, physics, cameras and audio all need one.

Each component element appears at most once per entity, and the order of components inside an
entity doesn't matter. The Editor writes every setting, defaults included. Attributes and elements
that are left out keep the component's default value. Ones the engine doesn't recognise are
ignored, with a warning in the console giving the file and line, so a misspelt name is noticed.

## Values

| Kind | Written as | Example |
| --- | --- | --- |
| Number | attribute | `Radius="0.5"` |
| Boolean | attribute, `true` or `false` | `IsTrigger="false"` |
| Enum | attribute, by name (each component page lists the names) | `Type="Dynamic"` |
| Collision layer | attribute, the layer's name from the project's [collision layers](collision-layers.md), or `Layer N` for an unnamed one | `Layer="Player"` |
| Collision mask | attribute, layer names separated by `, `, or `Everything` or `Nothing` | `Mask="Default, Enemy"` |
| 2D vector | child element with `X` and `Y` | `<Offset X="0" Y="0.5"/>` |
| 3D vector | child element with `X`, `Y` and `Z` | `<Position X="0" Y="0" Z="0"/>` |
| Colour | child element with `R`, `G`, `B`, `A` from 0 to 1 | `<Tint R="1" G="0.5" B="0" A="1"/>` |
| Asset | child element named after the setting, with a `Filepath` relative to the project folder and `/` separators; a component's main asset is a `Filepath` on its own element | `<Texture Filepath="Sprites/Player.png"/>`, `<LuaScript Filepath="Scripts/Player.lua"/>` |
| Entity | child element named after the setting, with the target's `ID`; `Name` is the target's name, written for people reading the file and ignored when loading | `<ConnectedEntity ID="6224437299739697637-3911424271900435661" Name="Floor"/>` |
| List | one child element per item | `<Vertex X="0" Y="1"/>` |

An asset or entity that isn't set is left out. An entity points at another entity in the same
scene by its `ID`, so a reference can be written by hand by copying the target's
`<Entity ID="...">`. If no entity has that ID the reference is kept and shows as Missing. Copying
entities (duplicate, paste, prefabs) points references between the copied entities at the copies. An unknown enum or layer name is reported in the console and
the default is used. Angles are in degrees: a transform's and a widget's `Rotation`, and a camera's
`Fov`. Lua uses radians for the same values.

## A complete example

A 2D scene with a camera, a player that falls under gravity onto the ground, and a pause button:

```xml
<Scene FormatVersion="2">
    <Gravity X="0" Y="-9.81"/>
    <Entity Name="Camera" ID="1-1">
        <Transform>
            <Position X="0" Y="0" Z="0"/>
            <Rotation X="0" Y="0" Z="0"/>
            <Scale X="1" Y="1" Z="1"/>
        </Transform>
        <Camera Primary="true" FixedAspectRatio="false" Projection="Orthographic" OrthoSize="10" OrthoNear="-1" OrthoFar="1"/>
        <AudioListener Primary="true"/>
    </Entity>
    <Entity Name="Player" ID="1-2">
        <Transform>
            <Position X="0" Y="3" Z="0"/>
            <Rotation X="0" Y="0" Z="0"/>
            <Scale X="1" Y="1" Z="1"/>
        </Transform>
        <Sprite TilingFactor="1" FilterMethod="Nearest" WrapMethod="Repeat">
            <Tint R="1" G="1" B="1" A="1"/>
            <Texture Filepath="Sprites/Player.png"/>
        </Sprite>
        <RigidBody2D Type="Dynamic" FixedRotation="true" GravityScale="1"/>
        <CapsuleCollider2D Direction="Vertical" Radius="0.4" Height="1" IsTrigger="false" Layer="Default" Mask="Everything">
            <Offset X="0" Y="0"/>
        </CapsuleCollider2D>
        <LuaScript Filepath="Scripts/Player.lua"/>
    </Entity>
    <Entity Name="Ground" ID="1-3">
        <Transform>
            <Position X="0" Y="-4" Z="0"/>
            <Rotation X="0" Y="0" Z="0"/>
            <Scale X="20" Y="1" Z="1"/>
        </Transform>
        <Sprite TilingFactor="1">
            <Tint R="0.3" G="0.6" B="0.3" A="1"/>
        </Sprite>
        <BoxCollider2D IsTrigger="false" Layer="Default" Mask="Everything">
            <Offset X="0" Y="0"/>
            <Size X="0.5" Y="0.5"/>
        </BoxCollider2D>
    </Entity>
    <Entity Name="HUD" ID="1-4">
        <Canvas PixelPerUnit="1"/>
        <Entity Name="Pause Button" ID="1-5">
            <Widget FixedWidth="true" FixedHeight="true"
                    AnchorLeft="1" AnchorRight="1" AnchorTop="0" AnchorBottom="0"
                    MarginLeft="-220" MarginRight="-20" MarginTop="20" MarginBottom="80">
                <Position X="1700" Y="20"/>
                <Size X="200" Y="60"/>
            </Widget>
            <Button>
                <NormalTint R="1" G="1" B="1" A="1"/>
                <HoveredTint R="0.9" G="0.9" B="0.9" A="1"/>
                <ClickedTint R="0.7" G="0.7" B="0.7" A="1"/>
                <DisabledTint R="0.5" G="0.5" B="0.5" A="1"/>
            </Button>
            <LuaScript Filepath="Scripts/PauseButton.lua"/>
        </Entity>
    </Entity>
</Scene>
```

The ground has no rigid body, so its collider holds still. The box collider's `Size` is half the
width and height, scaled by the transform, so 0.5 × 0.5 exactly fits the 20 × 1 sprite.
