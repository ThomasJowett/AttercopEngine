#include "vendor/doctest.h"

#include "Platform/Null/NullTexture.h"

#include <filesystem>
#include <vector>

namespace
{
	// A 1x1 RGBA PNG
	const std::vector<uint8_t> k_OnePixelPng = {
		0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
		0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4,
		0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00,
		0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE,
		0x42, 0x60, 0x82
	};

	const std::vector<uint8_t> k_NotAnImage = { 'n', 'o', 't', ' ', 'a', 'n', ' ', 'i', 'm', 'a', 'g', 'e' };
}

TEST_CASE("A texture created from image data in memory takes the image's size")
{
	NullTexture2D texture("Textures/Pixel.png", k_OnePixelPng);
	CHECK(texture.GetWidth() == 1);
	CHECK(texture.GetHeight() == 1);
	CHECK(texture.GetFilepath() == std::filesystem::path("Textures/Pixel.png").make_preferred());
}

TEST_CASE("A texture created from data that isn't an image is a placeholder that keeps its path")
{
	NullTexture2D texture("Textures/Broken.png", k_NotAnImage);
	CHECK(texture.GetWidth() == 4);
	CHECK(texture.GetHeight() == 4);
	// Keeping the path means saving a scene doesn't lose the reference
	CHECK(texture.GetFilepath() == std::filesystem::path("Textures/Broken.png").make_preferred());
}

TEST_CASE("Texture2D::Load from memory isn't supported")
{
	NullTexture2D texture(16, 16, Texture::Format::RGBA);
	CHECK_FALSE(texture.Load("Textures/Pixel.png", k_OnePixelPng));
}
