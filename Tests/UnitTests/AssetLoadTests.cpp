#include "vendor/doctest.h"

#include "Asset/AudioClip.h"
#include "Asset/Material.h"
#include "Asset/PhysicsMaterial.h"
#include "Asset/Tileset.h"

#include <cstring>
#include <filesystem>
#include <vector>

namespace
{
	const std::vector<uint8_t> k_NotXml = { 'n', 'o', 't', ' ', '<', 'x', 'm', 'l' };

	// A mono 16-bit WAV with a few silent samples
	std::vector<uint8_t> CreateWav()
	{
		const uint32_t sampleRate = 8000;
		const uint16_t channels = 1;
		const uint16_t bitsPerSample = 16;
		const uint32_t dataSize = 16;
		const uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
		const uint16_t blockAlign = channels * bitsPerSample / 8;
		const uint32_t riffSize = 36 + dataSize;
		const uint32_t fmtSize = 16;
		const uint16_t pcm = 1;

		std::vector<uint8_t> wav;
		auto append = [&wav](const void* data, size_t size)
			{
				const uint8_t* bytes = static_cast<const uint8_t*>(data);
				wav.insert(wav.end(), bytes, bytes + size);
			};
		append("RIFF", 4); append(&riffSize, 4); append("WAVE", 4);
		append("fmt ", 4); append(&fmtSize, 4); append(&pcm, 2); append(&channels, 2);
		append(&sampleRate, 4); append(&byteRate, 4); append(&blockAlign, 2); append(&bitsPerSample, 2);
		append("data", 4); append(&dataSize, 4);
		wav.resize(wav.size() + dataSize, 0);
		return wav;
	}
}

TEST_CASE("An audio clip loaded from memory reports success")
{
	AudioClip clip;
	CHECK(clip.Load("Audio/Silence.wav", CreateWav()));
	CHECK(clip.GetSampleRate() == 8000);
}

TEST_CASE("Assets loaded from data that isn't valid XML report failure")
{
	Material material;
	CHECK_FALSE(material.Load("Materials/Broken.material", k_NotXml));

	PhysicsMaterial physicsMaterial;
	CHECK_FALSE(physicsMaterial.Load("Materials/Broken.physicsmaterial", k_NotXml));

	Tileset tileset;
	CHECK_FALSE(tileset.Load("Tilesets/Broken.tileset", k_NotXml));
}

TEST_CASE("An asset that failed to load becomes a placeholder that keeps its path")
{
	Material material;
	CHECK_FALSE(material.IsMissing());
	material.MakeMissing("Materials/Gone.material");
	CHECK(material.IsMissing());
	CHECK(material.GetFilepath() == std::filesystem::path("Materials/Gone.material").make_preferred());
	CHECK(material.GetTexture(0) == Texture2D::GetMissingTexture());

	Tileset tileset;
	tileset.MakeMissing("Tilesets/Gone.tileset");
	CHECK(tileset.IsMissing());
	CHECK(tileset.GetNumberOfTiles() == 1);
	CHECK(tileset.GetSubTexture()->GetTexture() == Texture2D::GetMissingTexture());
}
