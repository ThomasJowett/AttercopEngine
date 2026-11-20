#include "SpriteComponent.h"

#include "Reflection/Describe.h"
#include "Renderer/Renderer2D.h"

namespace SpriteComponentReflection
{
	// The path alone while the texture is unloaded, so reading it doesn't load it
	Reflection::AssetValue GetTextureValue(const SpriteComponent& c)
	{
		if (c.texture)
			return { c.texture->GetFilepath(), c.texture };
		return { c.texturePath, nullptr };
	}

	// A path is loaded on first use, by GetTexture
	bool SetTextureValue(SpriteComponent& c, const Reflection::AssetValue& value)
	{
		Ref<Texture2D> texture;
		if (value.asset)
		{
			texture = std::dynamic_pointer_cast<Texture2D>(value.asset);
			if (!texture)
				return false;
		}
		c.texture = texture;
		c.texturePath = texture ? texture->GetFilepath() : value.path;
		return true;
	}

	// The texture is loaded unless the sprite atlas draws it
	void LoadTexture(Entity, SpriteComponent& c)
	{
		Ref<SpriteAtlas> atlas = Renderer2D::GetSpriteAtlas();
		if (!atlas || c.tilingFactor != 1.0f || !atlas->GetRegion(c.texturePath))
			c.GetTexture();
	}

	bool HasTexture(const SpriteComponent& c) { return c.texture || !c.texturePath.empty(); }
}

void SpriteComponent::Describe(Reflection::Builder<SpriteComponent>& c)
{
	using namespace SpriteComponentReflection;

	Reflection::Enum<Texture::FilterMethod>("FilterMethod", "Texture filtering, for SpriteComponent.FilterMethod", {
		{ "Linear", Texture::FilterMethod::Linear },
		{ "Nearest", Texture::FilterMethod::Nearest },
	});
	Reflection::Enum<Texture::WrapMethod>("WrapMethod", "Texture wrapping outside 0-1, for SpriteComponent.WrapMethod", {
		{ "Clamp", Texture::WrapMethod::Clamp },
		{ "Mirror", Texture::WrapMethod::Mirror },
		{ "Repeat", Texture::WrapMethod::Repeat },
	});

	c.Name("Sprite").Category("Rendering 2D")
		.Description("Draws a textured, tinted quad");

	c.Field<&SpriteComponent::tilingFactor>("TilingFactor", "How many times the texture repeats across the sprite")
		.Range(0.0f, 100.0f).ShowIf(&HasTexture).OnChanged(&LoadTexture);
	c.Field<&SpriteComponent::filterMethod>("FilterMethod", "FilterMethod.Linear or FilterMethod.Nearest").ShowIf(&HasTexture);
	c.Field<&SpriteComponent::wrapMethod>("WrapMethod", "WrapMethod.Clamp, WrapMethod.Mirror or WrapMethod.Repeat").ShowIf(&HasTexture);
	c.Field<&SpriteComponent::tint>("Tint", "Colour multiplied over the texture");
	c.Property<&GetTextureValue, &SetTextureValue>("Texture", "The texture drawn on the sprite, or nil for a plain coloured quad")
		.AssetType("Texture2D").OnChanged(&LoadTexture);
}
