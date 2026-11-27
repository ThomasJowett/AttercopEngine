# Weld Joint 2D

Holds this entity's physics body fixed to another body.

The joint connects this entity's body to the rigid body or tilemap of its `ConnectedEntity`, which
can be any entity in the scene. With no `ConnectedEntity`, it connects to its parent's rigid body
or tilemap, or to the world if it has no parent. Use it to attach physics objects together, such
as a weapon to a character. A `Stiffness` of 0 holds them rigidly together, and higher values let
the joint flex like a spring.

The joint is made when Play starts. If the `ConnectedEntity` isn't in the scene, is this entity
itself, or has no rigid body or tilemap body, there's no joint, and the console says why. Changing
`ConnectedEntity` while the scene is playing, from a script or the Properties panel, replaces the
joint with one to the new body, holding the two bodies where they are at that moment.

## Scene file

Saved as a `<WeldJoint2D>` element inside the entity's `<Entity>` element. Attributes that are left out keep their default. See [Scene Files](../../scene-files.md) for how values like vectors, colours and file paths are written.

```xml
<WeldJoint2D CollideConnected="false" Damping="0" Stiffness="0">
    <ConnectedEntity ID="6224437299739697637-3911424271900435661" Name="Floor"/>
</WeldJoint2D>
```

| Name | Where | Type | Default |
| --- | --- | --- | --- |
| `ConnectedEntity` | child element | entity, see [Scene Files](../../scene-files.md#values) | none: the parent, or the world |
| `CollideConnected` | attribute | bool, whether the two bodies collide with each other | `false` |
| `Damping` | attribute | float | `0` |
| `Stiffness` | attribute | float | `0` |

The entity also needs a [Rigid Body 2D](RigidBody2DComponent.md) or a collider for the joint to
attach to.

--8<-- "LuaAPI/_fragments/WeldJoint2DComponent.md"
