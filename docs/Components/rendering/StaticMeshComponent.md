# Static Mesh

Draws an imported 3D model.

Point it at a `.staticmesh` asset, made by importing a glTF file in the Content
Explorer. Each submesh uses the material the model came with, unless a material override
replaces it.

## Scene file

Saved as a `<StaticMesh>` element inside the entity's `<Entity>` element. Attributes that are left out keep their default. See [Scene Files](../../scene-files.md) for how values like vectors, colours and file paths are written.

```xml
<StaticMesh Filepath="Meshes/Crate.staticmesh">
    <Material Slot="2" Filepath="Materials/Wood.material"/>
</StaticMesh>
```

| Name | Where | Type | Default |
| --- | --- | --- | --- |
| `Filepath` | attribute: project-relative path | path to a `.staticmesh` | none |
| `Material` | child `Slot` `Filepath`, one per overridden slot | the mesh's material slot, and the path to a `.material` that replaces it | the model's own materials |

Slots without an override aren't written.

--8<-- "LuaAPI/_fragments/StaticMeshComponent.md"
