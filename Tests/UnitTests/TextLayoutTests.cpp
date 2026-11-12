#include "vendor/doctest.h"
#include "TestHelpers.h"

#include "Asset/Font.h"
#include "Scene/Components/TextComponent.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

namespace
{
	Ref<Font> LoadTestFont()
	{
		static Ref<Font> font = CreateRef<Font>(RepositoryPath("Editor/data/Fonts/Manrope-Medium.ttf"));
		return font;
	}

	// Glyphs on one line sit within half a line of each other
	int LineCount(const std::vector<GlyphQuad>& quads)
	{
		std::vector<float> lines;
		for (const GlyphQuad& quad : quads)
		{
			if (std::none_of(lines.begin(), lines.end(), [&quad](float y) { return std::abs(y - quad.planeMin.y) < 0.5f; }))
				lines.push_back(quad.planeMin.y);
		}
		return (int)lines.size();
	}
}

TEST_CASE("Empty text has no bounds and no glyphs")
{
	Ref<Font> font = LoadTestFont();
	REQUIRE(font->GetMSDFData());
	std::vector<GlyphQuad> quads;
	CHECK_FALSE(font->LayoutString("", 10.0f, quads).IsValid());
	CHECK(quads.empty());
	CHECK_FALSE(font->LayoutString("   ", 10.0f, quads).IsValid());
	CHECK(quads.empty());
}

TEST_CASE("A font that never loaded measures nothing")
{
	Font font;
	CHECK_FALSE(font.MeasureString("Hello", 10.0f).IsValid());
}

TEST_CASE("The bounds enclose every glyph quad")
{
	Ref<Font> font = LoadTestFont();
	std::vector<GlyphQuad> quads;
	BoundingBox bounds = font->LayoutString("Hello world\nSecond line", 100.0f, quads);
	REQUIRE(bounds.IsValid());
	CHECK(quads.size() == 20);
	for (const GlyphQuad& quad : quads)
	{
		CHECK(quad.planeMin.x >= bounds.Min().x);
		CHECK(quad.planeMin.y >= bounds.Min().y);
		CHECK(quad.planeMax.x <= bounds.Max().x);
		CHECK(quad.planeMax.y <= bounds.Max().y);
	}
	CHECK(bounds.Min().z == 0.0f);
	CHECK(bounds.Max().z == 0.0f);
	CHECK(font->MeasureString("Hello world\nSecond line", 100.0f).Max() == bounds.Max());
}

TEST_CASE("Text wraps at the last space before maxWidth")
{
	Ref<Font> font = LoadTestFont();
	std::vector<GlyphQuad> quads;
	BoundingBox oneLine = font->LayoutString("aaaa bbbb cccc", 100.0f, quads);
	CHECK(LineCount(quads) == 1);

	float wordWidth = font->MeasureString("aaaa bbbb", 100.0f).Max().x;
	BoundingBox wrapped = font->LayoutString("aaaa bbbb cccc", wordWidth + 0.01f, quads);
	CHECK(LineCount(quads) == 2);
	CHECK(quads.size() == 12);
	CHECK(wrapped.Max().x <= wordWidth + 0.01f);
	CHECK(wrapped.Min().y < oneLine.Min().y);
}

TEST_CASE("A wrapped line starts at the left edge")
{
	Ref<Font> font = LoadTestFont();
	std::vector<GlyphQuad> quads;
	float wordWidth = font->MeasureString("aaaa", 100.0f).Max().x;
	font->LayoutString("aaaa aaaa", wordWidth + 0.01f, quads);
	REQUIRE(quads.size() == 8);
	CHECK(quads[4].planeMin.x == doctest::Approx(quads[0].planeMin.x));
}

TEST_CASE("A long word after a newline doesn't wrap the line before it")
{
	Ref<Font> font = LoadTestFont();
	std::vector<GlyphQuad> quads;
	float width = font->MeasureString("aa bb", 100.0f).Max().x + 0.01f;
	font->LayoutString("aa bb\ncccccccccccccccccccc", width, quads);
	CHECK(LineCount(quads) == 2);
}

TEST_CASE("Wrapping measures a tab as wide as it's drawn")
{
	Ref<Font> font = LoadTestFont();
	std::vector<GlyphQuad> quads;
	font->LayoutString("a\tb", 100.0f, quads);
	REQUIRE(quads.size() == 2);
	float tabEnd = quads[1].planeMax.x;
	font->LayoutString("a\tb c", tabEnd + 0.01f, quads);
	CHECK(LineCount(quads) == 2);
}

TEST_CASE("A multi-byte UTF-8 character is one glyph")
{
	Ref<Font> font = LoadTestFont();
	std::vector<GlyphQuad> quads;
	font->LayoutString("caf\xC3\xA9", 100.0f, quads);
	CHECK(quads.size() == 4);
}

TEST_CASE("The text component's bounds follow its text, font and max width")
{
	Ref<Font> font = LoadTestFont();
	TextComponent text;
	text.font = font;
	text.text = "aaaa bbbb";
	text.maxWidth = 100.0f;
	BoundingBox oneLine = text.GetBounds();
	REQUIRE(oneLine.IsValid());

	text.maxWidth = font->MeasureString("aaaa", 100.0f).Max().x + 0.01f;
	CHECK(text.GetBounds().Min().y < oneLine.Min().y);

	text.text = "aaaa";
	CHECK(text.GetBounds().Max().x < oneLine.Max().x);

	text.text.clear();
	CHECK_FALSE(text.GetBounds().IsValid());

	text.text = "aaaa bbbb";
	text.font = nullptr;
	CHECK_FALSE(text.GetBounds().IsValid());
}
