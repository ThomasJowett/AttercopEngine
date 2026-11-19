#pragma once

#include "Core/Colour.h"

#include "Asset/Texture.h"
#include "Asset/Material.h"
#include "Scene/AssetManager.h"
#include "Utilities/SerializationUtils.h"
#include "Reflection/ReflectionFwd.h"

struct SpriteComponent
{
	Colour tint{ 1.0f, 1.0f,1.0f,1.0f };
	Ref<Texture2D> texture;
	float tilingFactor = 1.0f;

	std::filesystem::path texturePath;
	Texture::FilterMethod filterMethod = Texture::FilterMethod::Nearest;
	Texture::WrapMethod wrapMethod = Texture::WrapMethod::Repeat;

	SpriteComponent() = default;
	SpriteComponent(const SpriteComponent&) = default;

	// Loads the texture from texturePath on first use; the scene loader leaves it unloaded when the sprite atlas covers it
	const Ref<Texture2D>& GetTexture()
	{
		if (!texture && !texturePath.empty())
			texture = AssetManager::GetTexture(texturePath);
		return texture;
	}

	static void Describe(Reflection::Builder<SpriteComponent>& c);
};
