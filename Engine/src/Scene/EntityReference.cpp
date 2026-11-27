#include "EntityReference.h"

#include "Scene/Entity.h"
#include "Scene/Scene.h"

Entity EntityReference::Resolve(Scene* scene) const
{
	if (!scene || !IsSet())
		return Entity();
	return scene->GetEntityByID(id);
}
