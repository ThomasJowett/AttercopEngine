#include "ImageDiff.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

bool Image::Load(const std::filesystem::path& path)
{
	int w = 0, h = 0, channels = 0;
	// Texture loading can leave stb's global flip on
	stbi_set_flip_vertically_on_load(0);
	stbi_uc* data = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
	if (!data)
		return false;
	width = (uint32_t)w;
	height = (uint32_t)h;
	rgba.assign(data, data + (size_t)w * h * 4);
	stbi_image_free(data);
	return true;
}

bool Image::Save(const std::filesystem::path& path) const
{
	std::error_code error;
	if (path.has_parent_path())
		std::filesystem::create_directories(path.parent_path(), error);
	stbi_flip_vertically_on_write(0);
	return stbi_write_png(path.string().c_str(), (int)width, (int)height, 4, rgba.data(), (int)width * 4) != 0;
}

static int ChannelDifference(const uint8_t* a, const uint8_t* b)
{
	int difference = 0;
	for (int c = 0; c < 4; ++c)
		difference = std::max(difference, std::abs((int)a[c] - (int)b[c]));
	return difference;
}

std::string ImageDiffResult::ToString() const
{
	if (!sizeMatches)
		return "the sizes differ";
	std::ostringstream text;
	text << differentPixels << " pixels differ (" << differentFraction * 100.0 << "%), largest channel difference "
		<< maxChannelDifference;
	return text.str();
}

ImageDiffResult CompareImages(const Image& actual, const Image& expected, const ImageDiffLimits& limits)
{
	ImageDiffResult result;
	result.sizeMatches = actual.width == expected.width && actual.height == expected.height
		&& actual.rgba.size() == expected.rgba.size() && !actual.rgba.empty();
	if (!result.sizeMatches)
		return result;

	const size_t pixelCount = (size_t)actual.width * actual.height;
	for (size_t i = 0; i < pixelCount; ++i)
	{
		int difference = ChannelDifference(&actual.rgba[i * 4], &expected.rgba[i * 4]);
		result.maxChannelDifference = std::max(result.maxChannelDifference, difference);
		if (difference > limits.pixelThreshold)
			++result.differentPixels;
	}
	result.differentFraction = (double)result.differentPixels / (double)pixelCount;
	result.passed = result.differentFraction <= limits.maxDifferentFraction
		&& result.maxChannelDifference <= std::max(limits.maxChannelDifference, limits.pixelThreshold);
	return result;
}

Image MakeDiffImage(const Image& actual, const Image& expected, int pixelThreshold)
{
	Image diff;
	diff.width = expected.width;
	diff.height = expected.height;
	diff.rgba.resize(expected.rgba.size());
	bool sameSize = actual.width == expected.width && actual.height == expected.height;

	for (size_t i = 0; i < expected.rgba.size(); i += 4)
	{
		const uint8_t* e = &expected.rgba[i];
		uint8_t* d = &diff.rgba[i];
		if (sameSize && ChannelDifference(&actual.rgba[i], e) > pixelThreshold)
		{
			d[0] = 255; d[1] = 0; d[2] = 0;
		}
		else
		{
			uint8_t grey = (uint8_t)((e[0] * 77 + e[1] * 150 + e[2] * 29) >> 10);
			d[0] = d[1] = d[2] = grey;
		}
		d[3] = 255;
	}
	return diff;
}
