#pragma once

#include "Asset/Texture.h"

#include "WidgetComponent.h"

#include "Utilities/SerializationUtils.h"
#include "Reflection/ReflectionFwd.h"

struct ButtonComponent
{
	Ref<Texture2D> icon;
	Ref<Texture2D> normalTexture;
	Ref<Texture2D> hoveredTexture;
	Ref<Texture2D> clickedTexture;
	Ref<Texture2D> disabledTexture;

	Colour normalTint = Colours::WHITE;
	Colour hoveredTint = Colours::WHITE;
	Colour clickedTint = Colours::WHITE;
	Colour disabledTint = Colours::WHITE;

	Texture::FilterMethod normalFilterMethod = Texture::FilterMethod::Nearest;
	Texture::FilterMethod hoveredFilterMethod = Texture::FilterMethod::Nearest;
	Texture::FilterMethod clickedFilterMethod = Texture::FilterMethod::Nearest;
	Texture::FilterMethod disabledFilterMethod = Texture::FilterMethod::Nearest;
	Texture::WrapMethod normalWrapMethod = Texture::WrapMethod::Repeat;
	Texture::WrapMethod hoveredWrapMethod = Texture::WrapMethod::Repeat;
	Texture::WrapMethod clickedWrapMethod = Texture::WrapMethod::Repeat;
	Texture::WrapMethod disabledWrapMethod = Texture::WrapMethod::Repeat;

	static void Describe(Reflection::Builder<ButtonComponent>& c);
};
