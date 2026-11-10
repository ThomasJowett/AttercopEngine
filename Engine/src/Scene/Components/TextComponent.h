#pragma once

#include "cereal/cereal.hpp"
#include "Asset/Font.h"
#include "Scene/AssetManager.h"
#include "Scripting/Lua/LuaBindings.h"

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

	REFLECT_LUA_BEGIN(TextComponent)
		REFLECT_LUA_PROPERTY_CUSTOM("Text", "The string to draw", "string",
			([](Self& c) { return c.text; }),
			([](Self& c, const std::string& v) { c.text = v; }))
		REFLECT_LUA_PROPERTY_CUSTOM("MaxWidth", "Width at which lines wrap, in world units", "number",
			([](Self& c) { return c.maxWidth; }),
			([](Self& c, float v) { c.maxWidth = v; }))
		REFLECT_LUA_PROPERTY_CUSTOM("Colour", "Colour of the text", "Colour",
			([](Self& c) -> Colour& { return c.colour; }),
			([](Self& c, const Colour& v) { c.colour = v; }))
		REFLECT_LUA_PROPERTY_CUSTOM("Font", "The font the text is drawn with", "Font",
			([](Self& c) { return c.font; }),
			([](Self& c, const Ref<Font>& v) { c.font = v; }))
	REFLECT_LUA_END()

private:
	mutable BoundingBox m_Bounds;
	mutable std::string m_BoundsText;
	mutable uint32_t m_BoundsFontVersion = 0;
	mutable float m_BoundsMaxWidth = 0.0f;
	mutable bool m_BoundsCached = false;

	friend cereal::access;
	template<typename Archive>
	void save(Archive& archive) const
	{
		archive(text, maxWidth, colour);
		std::string relativePath;
		if(font != Font::GetDefaultFont())
			relativePath = font->GetFilepath().string();
		archive(relativePath);
	}

	template<typename Archive>
	void load(Archive& archive)
	{
		archive(text, maxWidth, colour);
		std::string relativePath;
		archive(relativePath);
		if (!relativePath.empty())
		{
			font = AssetManager::GetAsset<Font>(relativePath);
		}
		else
		{
			font = Font::GetDefaultFont();
		}
	}
};
