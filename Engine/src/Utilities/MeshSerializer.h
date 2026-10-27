#pragma once

#include "Asset/StaticMesh.h"

#include <string>
#include <vector>

// A .staticmesh file is a readable XML header (version, layout, materials, submeshes)
// followed by the raw vertex and index data
class MeshSerializer
{
public:
	static bool Serialize(const std::filesystem::path& filepath, const Ref<Mesh>& mesh, MeshLayout layoutType);
	static Ref<Mesh> DeserializeFromMemory(const std::filesystem::path& filepath, const std::vector<uint8_t>& data);
	static Ref<Mesh> Deserialize(const std::filesystem::path& filepath);

	// Splits a file into its XML header and the offset of the binary data; false for the old binary-only format
	static bool SplitHeader(const std::vector<uint8_t>& data, std::string& header, size_t& binaryStart);

	// Writes the header followed by the binary data starting at binaryStart in data
	static bool WriteWithHeader(const std::filesystem::path& filepath, const std::string& header, const std::vector<uint8_t>& data, size_t binaryStart);
};
