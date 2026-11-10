#include "Font.h"

#include "Renderer/UI/MSDFData.h"
#include "Logging/Instrumentor.h"
#include "Scene/AssetManager.h"

Font::Font()
{
}

Font::Font(const std::filesystem::path& filepath)
{
	Load(filepath);
}

Font::Font(const std::filesystem::path& filepath, const std::vector<uint8_t>& data)
{
	Load(filepath, data);
}

Font::~Font()
{
	delete m_MSDFData;
}

Ref<Font> Font::s_DefaultFont;
uint32_t Font::s_NextVersion = 0;

void Font::Init()
{
	PROFILE_FUNCTION();
	std::filesystem::path fontPath = "data/Fonts/Manrope-Medium.ttf";
	if (AssetManager::HasBundle())
	{
		std::vector<uint8_t> data;
		AssetManager::GetFileData(fontPath, data);
		s_DefaultFont = CreateRef<Font>(fontPath, data);
	}
	else {
		std::filesystem::path absolutePath = Application::GetWorkingDirectory() / fontPath;
		s_DefaultFont = CreateRef<Font>(absolutePath);
	}
}

void Font::Shutdown()
{
	s_DefaultFont.reset();
}

bool Font::Load(const std::filesystem::path& filepath)
{
	PROFILE_FUNCTION();
	std::filesystem::path absolutePath = std::filesystem::absolute(Application::GetOpenDocumentDirectory() / filepath);
	if (!std::filesystem::exists(absolutePath))
	{
		ENGINE_ERROR("Font does not exist: {0}", absolutePath);
		return false;
	}

	m_TextureAtlas.reset();

	// Try Loading the atlas cache
	std::filesystem::path cachePath = filepath;
	cachePath.replace_extension(".png");
	std::filesystem::path absoluteCachePath = std::filesystem::absolute(Application::GetOpenDocumentDirectory() / cachePath);
	if (std::filesystem::exists(absoluteCachePath))
	{
		m_TextureAtlas = AssetManager::GetTexture(cachePath);
		m_TextureAtlas->SetFilterMethod(Texture::FilterMethod::Linear);
	}

	msdfgen::FreetypeHandle* ftHandle = msdfgen::initializeFreetype();
	msdfgen::FontHandle* fontHandle = msdfgen::loadFont(ftHandle, absolutePath.string().c_str());

	if (!ftHandle || !fontHandle)
	{
		ENGINE_ERROR("Could not load font: {0}", filepath);
		msdfgen::destroyFont(fontHandle);
		msdfgen::deinitializeFreetype(ftHandle);
		return false;
	}

	m_Filepath = filepath;
	m_Filepath.make_preferred();

	if (m_MSDFData)
		delete m_MSDFData;
	m_MSDFData = new MSDFData();
	m_MSDFData->fontGeometry = msdf_atlas::FontGeometry(&m_MSDFData->glyphs);
	m_Version = ++s_NextVersion;

	int glyphsLoaded = -1;

	glyphsLoaded = m_MSDFData->fontGeometry.loadCharset(fontHandle, 1.0f, msdf_atlas::Charset::ASCII);

	ASSERT(glyphsLoaded >= 0, "Could not load any glyphs from the font");

	m_MSDFData->fontGeometry.setName(m_Filepath.string().c_str());

	for (msdf_atlas::GlyphGeometry& glyph : m_MSDFData->glyphs)
	{
		glyph.edgeColoring(msdfgen::edgeColoringInkTrap, 3.0, 0);
	}

	msdf_atlas::TightAtlasPacker atlasPacker;

	atlasPacker.setDimensionsConstraint(msdf_atlas::TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE);
	atlasPacker.setPadding(0);
	atlasPacker.setScale(40);
	atlasPacker.setPixelRange(2.0);
	atlasPacker.setMiterLimit(1.0);
	if (int remaining = atlasPacker.pack(m_MSDFData->glyphs.data(), (int)m_MSDFData->glyphs.size()))
	{
		ASSERT(remaining >= 0, "Remaining cannot be negative");
		ENGINE_ERROR("Could not fit {0} out of {1} glyphs into the atlas.", remaining, (int)m_MSDFData->glyphs.size());
	}

	if (m_TextureAtlas)
		return true;

	int width = 0, height = 0;
	atlasPacker.getDimensions(width, height);
	ASSERT(width > 0 && height > 0, "Area of font atlas cannot be zero");
	ENGINE_TRACE("Generated font atlas with dimensions: {0} x {1}", width, height);

	const int bytes = 4;

	msdf_atlas::ImmediateAtlasGenerator<float, bytes, msdf_atlas::mtsdfGenerator, msdf_atlas::BitmapAtlasStorage<float, bytes>> generator(width, height);

	msdf_atlas::GeneratorAttributes generatorAttributes;
	generatorAttributes.config.overlapSupport = true;
	generatorAttributes.scanlinePass = true;
	generator.setAttributes(generatorAttributes);
#ifdef __EMSCRIPTEN__
	generator.setThreadCount(1);
#else
	generator.setThreadCount(8);
#endif
	generator.generate(m_MSDFData->glyphs.data(), (int)m_MSDFData->glyphs.size());

	msdfgen::BitmapConstRef<float, bytes> bitmap = (msdfgen::BitmapConstRef<float, bytes>)generator.atlasStorage();

	msdfgen::destroyFont(fontHandle);
	msdfgen::deinitializeFreetype(ftHandle);

	// Cache the texture to png file
	if (!msdfgen::savePng(bitmap, absoluteCachePath.string().c_str()))
		ENGINE_ERROR("Could not save the font atlas: {0}", absoluteCachePath.string());

	m_TextureAtlas = AssetManager::GetTexture(cachePath);

	m_TextureAtlas->SetFilterMethod(Texture::FilterMethod::Linear);
	return true;
}

bool Font::Load(const std::filesystem::path& filepath, const std::vector<uint8_t>& data)
{
	PROFILE_FUNCTION();

	msdfgen::FreetypeHandle* ftHandle = msdfgen::initializeFreetype();
	msdfgen::FontHandle* fontHandle = msdfgen::loadFontData(ftHandle, data.data(), (int)data.size());

	if (!ftHandle || !fontHandle)
	{
		ENGINE_ERROR("Could not load font: {0}", filepath);
		msdfgen::destroyFont(fontHandle);
		msdfgen::deinitializeFreetype(ftHandle);
		return false;
	}

	m_Filepath = filepath;
	m_Filepath.make_preferred();
	m_TextureAtlas.reset();

	// Load PNG atlas from memory
	std::filesystem::path cachePath = filepath;
	cachePath.replace_extension(".png");

	std::vector<uint8_t> pngData;
	AssetManager::GetFileData(cachePath, pngData);
	if (pngData.empty())
	{
		ENGINE_ERROR("Could not load font atlas from memory: {0}", cachePath.string());
		msdfgen::destroyFont(fontHandle);
		msdfgen::deinitializeFreetype(ftHandle);
		return false;
	}

	m_TextureAtlas = AssetManager::GetTexture(cachePath);
	m_TextureAtlas->SetFilterMethod(Texture::FilterMethod::Linear);

	if (m_MSDFData)
		delete m_MSDFData;
	m_MSDFData = new MSDFData();
	m_MSDFData->fontGeometry = msdf_atlas::FontGeometry(&m_MSDFData->glyphs);
	m_Version = ++s_NextVersion;

	int glyphsLoaded = m_MSDFData->fontGeometry.loadCharset(fontHandle, 1.0f, msdf_atlas::Charset::ASCII);
	ASSERT(glyphsLoaded >= 0, "Could not load any glyphs from the font");

	m_MSDFData->fontGeometry.setName(m_Filepath.string().c_str());

	for (msdf_atlas::GlyphGeometry& glyph : m_MSDFData->glyphs)
	{
		glyph.edgeColoring(msdfgen::edgeColoringInkTrap, 3.0, 0);
	}

	msdf_atlas::TightAtlasPacker atlasPacker;

	atlasPacker.setDimensionsConstraint(msdf_atlas::TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE);
	atlasPacker.setPadding(0);
	atlasPacker.setScale(40);
	atlasPacker.setPixelRange(2.0);
	atlasPacker.setMiterLimit(1.0);
	if (int remaining = atlasPacker.pack(m_MSDFData->glyphs.data(), (int)m_MSDFData->glyphs.size()))
	{
		ASSERT(remaining >= 0, "Remaining cannot be negative");
		ENGINE_ERROR("Could not fit {0} out of {1} glyphs into the atlas.", remaining, (int)m_MSDFData->glyphs.size());
	}

	msdfgen::destroyFont(fontHandle);
	msdfgen::deinitializeFreetype(ftHandle);

	return true;
}

// Invalid or truncated sequences decode to U+FFFD
static void DecodeUtf8(const std::string& text, std::u32string& out)
{
	out.clear();
	out.reserve(text.size());
	size_t i = 0;
	while (i < text.size())
	{
		unsigned char lead = (unsigned char)text[i];
		int length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
		if (length == 0 || i + length > text.size())
		{
			out.push_back(0xFFFD);
			i++;
			continue;
		}

		char32_t codepoint = length == 1 ? lead : lead & (0x7F >> length);
		bool valid = true;
		for (int j = 1; j < length; j++)
		{
			unsigned char next = (unsigned char)text[i + j];
			if ((next & 0xC0) != 0x80)
			{
				valid = false;
				break;
			}
			codepoint = (codepoint << 6) | (next & 0x3F);
		}

		out.push_back(valid ? codepoint : 0xFFFD);
		i += valid ? length : 1;
	}
}

BoundingBox Font::LayoutString(const std::string& text, float maxWidth, std::vector<GlyphQuad>& quads) const
{
	PROFILE_FUNCTION();
	quads.clear();
	BoundingBox bounds;
	if (!m_MSDFData || text.empty())
		return bounds;

	static thread_local std::u32string s_Codepoints;
	DecodeUtf8(text, s_Codepoints);
	const std::u32string& codepoints = s_Codepoints;

	const msdf_atlas::FontGeometry& fontGeometry = m_MSDFData->fontGeometry;
	const msdfgen::FontMetrics& metrics = fontGeometry.getMetrics();
	const double scale = 1.0 / (metrics.ascenderY - metrics.descenderY);
	const double lineAdvance = scale * metrics.lineHeight;
	const msdf_atlas::GlyphGeometry* space = fontGeometry.getGlyph(' ');
	const double tabAdvance = space ? scale * space->getAdvance() * 4.0 : 0.0;

	double x = 0.0;
	double y = 0.0;
	// The last space or tab on this line, where it wraps; glyphs placed after it are dropped when it does
	size_t lastSpace = std::u32string::npos;
	size_t quadsAtLastSpace = 0;

	for (size_t i = 0; i < codepoints.size(); i++)
	{
		char32_t character = codepoints[i];
		if (character == '\n')
		{
			x = 0.0;
			y -= lineAdvance;
			lastSpace = std::u32string::npos;
			continue;
		}

		if (character == ' ' || character == '\t')
		{
			lastSpace = i;
			quadsAtLastSpace = quads.size();
		}

		if (character == '\t')
		{
			x += tabAdvance;
			continue;
		}

		const msdf_atlas::GlyphGeometry* glyph = fontGeometry.getGlyph(character);
		if (!glyph)
			glyph = fontGeometry.getGlyph('?');
		if (!glyph)
			continue;

		if (!glyph->isWhitespace())
		{
			double pl, pb, pr, pt;
			glyph->getQuadPlaneBounds(pl, pb, pr, pt);
			pl = pl * scale + x, pb = pb * scale + y, pr = pr * scale + x, pt = pt * scale + y;

			if (pr > maxWidth && lastSpace != std::u32string::npos)
			{
				// Start a new line after the space, the loop picks up from the character following it
				quads.resize(quadsAtLastSpace);
				i = lastSpace;
				lastSpace = std::u32string::npos;
				x = 0.0;
				y -= lineAdvance;
				continue;
			}

			double l, b, r, t;
			glyph->getQuadAtlasBounds(l, b, r, t);
			quads.push_back({ Vector2f((float)pl, (float)pb), Vector2f((float)pr, (float)pt), Vector2f((float)l, (float)b), Vector2f((float)r, (float)t) });
		}

		double advance = glyph->getAdvance();
		char32_t nextCharacter = i + 1 < codepoints.size() ? codepoints[i + 1] : 0;
		fontGeometry.getAdvance(advance, character, nextCharacter);
		x += scale * advance;
	}

	for (const GlyphQuad& quad : quads)
	{
		bounds.Merge(Vector3f(quad.planeMin.x, quad.planeMin.y, 0.0f));
		bounds.Merge(Vector3f(quad.planeMax.x, quad.planeMax.y, 0.0f));
	}
	return bounds;
}

BoundingBox Font::MeasureString(const std::string& text, float maxWidth) const
{
	std::vector<GlyphQuad> quads;
	return LayoutString(text, maxWidth, quads);
}
