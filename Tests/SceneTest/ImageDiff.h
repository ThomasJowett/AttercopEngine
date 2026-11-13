#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Tightly packed RGBA8 pixels, top row first
struct Image
{
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint8_t> rgba;

	bool Load(const std::filesystem::path& path);
	bool Save(const std::filesystem::path& path) const;
};

// How far an image may be from its golden before the comparison fails
struct ImageDiffLimits
{
	// A pixel differs if any channel is further away than this
	int pixelThreshold = 0;
	// Fails if more than this fraction of the pixels differ
	double maxDifferentFraction = 0.0;
	// Fails if any channel of any pixel is further away than this
	int maxChannelDifference = 0;
};

struct ImageDiffResult
{
	bool sizeMatches = false;
	uint64_t differentPixels = 0;
	double differentFraction = 0.0;
	int maxChannelDifference = 0;
	bool passed = false;

	std::string ToString() const;
};

ImageDiffResult CompareImages(const Image& actual, const Image& expected, const ImageDiffLimits& limits);

// Differing pixels in red over a dimmed grey copy of the expected image
Image MakeDiffImage(const Image& actual, const Image& expected, int pixelThreshold);
