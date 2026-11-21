#pragma once

#include "Asset/Font.h"
#include "Core/Colour.h"
#include "Scene/AssetManager.h"
#include "Reflection/ReflectionFwd.h"

#include <cstdint>

struct TextComponent
{
	TextComponent() = default;
	TextComponent(const TextComponent&) = default;

	std::string text;
	Ref<Font> font = Font::GetDefaultFont();
	float maxWidth = 10.0f;
	Colour colour{ Colours::WHITE };

	// Local bounds of the drawn text, laid out again only when the text, font or max width change
	const BoundingBox& GetBounds() const
	{
		const Ref<Font>& drawFont = Font::OrDefault(font);
		uint32_t fontVersion = drawFont ? drawFont->GetVersion() : 0;
		if (!m_BoundsCached || fontVersion != m_BoundsFontVersion || maxWidth != m_BoundsMaxWidth || text != m_BoundsText)
		{
			m_Bounds = drawFont ? drawFont->MeasureString(text, maxWidth) : BoundingBox();
			m_BoundsText = text;
			m_BoundsFontVersion = fontVersion;
			m_BoundsMaxWidth = maxWidth;
			m_BoundsCached = true;
		}
		return m_Bounds;
	}

	static void Describe(Reflection::Builder<TextComponent>& c);

private:
	mutable BoundingBox m_Bounds;
	mutable std::string m_BoundsText;
	mutable uint32_t m_BoundsFontVersion = 0;
	mutable float m_BoundsMaxWidth = 0.0f;
	mutable bool m_BoundsCached = false;
};
