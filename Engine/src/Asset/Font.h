#pragma once

#include "Asset/Asset.h"
#include "cereal/access.hpp"
#include "Texture.h"
#include "Math/BoundingBox.h"
#include "math/Vector2f.h"

#include <vector>

struct MSDFData;

// A glyph placed by Font::LayoutString, in the string's local space
struct GlyphQuad
{
	Vector2f planeMin;
	Vector2f planeMax;
	// In atlas pixels
	Vector2f atlasMin;
	Vector2f atlasMax;
};

class Font : public Asset
{
public:
	Font();
	Font(const std::filesystem::path& filepath);
	Font(const std::filesystem::path& filepath, const std::vector<uint8_t>& data);
	~Font();

	// Inherited via Asset
	virtual bool Load(const std::filesystem::path& filepath) override;
	virtual bool Load(const std::filesystem::path& filepath, const std::vector<uint8_t>& data) override;

	Ref<Texture2D> GetFontAtlas() const { return m_TextureAtlas; }
	const MSDFData* GetMSDFData() const { return m_MSDFData; }

	// Changes every time the font is loaded, so cached layouts can tell when they're stale
	uint32_t GetVersion() const { return m_Version; }

	// Places each glyph as Renderer2D::DrawString draws it, wrapping at maxWidth, and returns their bounds
	BoundingBox LayoutString(const std::string& text, float maxWidth, std::vector<GlyphQuad>& quads) const;
	BoundingBox MeasureString(const std::string& text, float maxWidth) const;

	// The font text is drawn with: the default font in place of one that failed to load
	static const Ref<Font>& OrDefault(const Ref<Font>& font)
	{
		return font && font->IsMissing() && s_DefaultFont ? s_DefaultFont : font;
	}

	static void Init();
	static void Shutdown();

	static Ref<Font> GetDefaultFont() { return s_DefaultFont; }
private:
	Ref<Texture2D> m_TextureAtlas;
	MSDFData* m_MSDFData = nullptr;
	uint32_t m_Version = 0;

	static uint32_t s_NextVersion;

	static Ref<Font> s_DefaultFont;
};