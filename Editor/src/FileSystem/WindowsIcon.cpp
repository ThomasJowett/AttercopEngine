#ifdef _WIN32
#include "WindowsIcon.h"

#include "Logging/Logger.h"

#include "stb_image.h"
#include "stb_image_write.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <vector>

namespace
{
#pragma pack(push, 2)
	struct GroupIconHeader
	{
		WORD reserved;
		WORD type;
		WORD count;
	};

	struct GroupIconEntry
	{
		BYTE width;
		BYTE height;
		BYTE colourCount;
		BYTE reserved;
		WORD planes;
		WORD bitCount;
		DWORD bytesInResource;
		WORD id;
	};
#pragma pack(pop)

	// Box filter, weighting colour by alpha so transparent edges don't darken
	std::vector<uint8_t> Resize(const uint8_t* pixels, int width, int height, int size)
	{
		std::vector<uint8_t> resized((size_t)size * size * 4);
		for (int y = 0; y < size; ++y)
		{
			int y0 = y * height / size;
			int y1 = std::max(y0 + 1, (y + 1) * height / size);
			for (int x = 0; x < size; ++x)
			{
				int x0 = x * width / size;
				int x1 = std::max(x0 + 1, (x + 1) * width / size);

				uint64_t sum[4] = {};
				for (int sourceY = y0; sourceY < y1; ++sourceY)
				{
					for (int sourceX = x0; sourceX < x1; ++sourceX)
					{
						const uint8_t* pixel = pixels + ((size_t)sourceY * width + sourceX) * 4;
						for (int channel = 0; channel < 3; ++channel)
							sum[channel] += (uint64_t)pixel[channel] * pixel[3];
						sum[3] += pixel[3];
					}
				}

				uint8_t* out = &resized[((size_t)y * size + x) * 4];
				for (int channel = 0; channel < 3; ++channel)
					out[channel] = sum[3] ? (uint8_t)(sum[channel] / sum[3]) : 0;
				out[3] = (uint8_t)(sum[3] / ((uint64_t)(x1 - x0) * (y1 - y0)));
			}
		}
		return resized;
	}

	void AppendToVector(void* context, void* data, int size)
	{
		std::vector<uint8_t>* out = static_cast<std::vector<uint8_t>*>(context);
		out->insert(out->end(), (uint8_t*)data, (uint8_t*)data + size);
	}
}

bool WindowsIcon::AddToExecutable(const std::filesystem::path& executablePath, const std::filesystem::path& pngPath)
{
	int width, height, channels;
	stbi_set_flip_vertically_on_load(0);
	stbi_uc* pixels = stbi_load(pngPath.string().c_str(), &width, &height, &channels, 4);
	if (!pixels)
	{
		ENGINE_ERROR("Could not load icon {0}: {1}", pngPath.string(), stbi_failure_reason());
		return false;
	}

	// PNG-compressed icon images, which Windows reads at any size
	const int sizes[] = { 16, 24, 32, 48, 64, 128, 256 };
	std::vector<std::vector<uint8_t>> images;
	for (int size : sizes)
	{
		std::vector<uint8_t> resized = Resize(pixels, width, height, size);
		std::vector<uint8_t>& png = images.emplace_back();
		stbi_write_png_to_func(AppendToVector, &png, size, size, 4, resized.data(), size * 4);
	}
	stbi_image_free(pixels);

	HANDLE update = BeginUpdateResourceW(executablePath.wstring().c_str(), FALSE);
	if (!update)
	{
		ENGINE_ERROR("Could not open {0} to add its icon, error {1}", executablePath.string(), GetLastError());
		return false;
	}

	const WORD language = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
	std::vector<uint8_t> group(sizeof(GroupIconHeader) + images.size() * sizeof(GroupIconEntry));
	GroupIconHeader* header = (GroupIconHeader*)group.data();
	header->reserved = 0;
	header->type = 1;
	header->count = (WORD)images.size();

	bool succeeded = true;
	for (size_t i = 0; i < images.size(); ++i)
	{
		WORD id = (WORD)(i + 1);
		GroupIconEntry* entry = (GroupIconEntry*)(group.data() + sizeof(GroupIconHeader)) + i;
		entry->width = (BYTE)(sizes[i] >= 256 ? 0 : sizes[i]);
		entry->height = entry->width;
		entry->colourCount = 0;
		entry->reserved = 0;
		entry->planes = 1;
		entry->bitCount = 32;
		entry->bytesInResource = (DWORD)images[i].size();
		entry->id = id;

		succeeded = succeeded && UpdateResourceW(update, RT_ICON, MAKEINTRESOURCEW(id), language, images[i].data(), (DWORD)images[i].size());
	}
	succeeded = succeeded && UpdateResourceW(update, RT_GROUP_ICON, MAKEINTRESOURCEW(1), language, group.data(), (DWORD)group.size());

	// Discards the changes if any failed
	if (!EndUpdateResourceW(update, !succeeded) || !succeeded)
	{
		ENGINE_ERROR("Could not add the icon to {0}, error {1}", executablePath.string(), GetLastError());
		return false;
	}
	return true;
}
#endif
