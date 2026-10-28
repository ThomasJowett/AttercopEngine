#include "MeshSerializer.h"
#include "Scene/AssetManager.h"
#include "Utilities/SerializationUtils.h"
#include "TinyXml2/tinyxml2.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>

#define MESH_MAGIC 0x4D455348 // "MESH", the old binary-only format
#define MESH_BINARY_VERSION 1
#define MESH_VERSION 2

static const char* k_HeaderEnd = "</StaticMesh>";

static const char* LayoutName(MeshLayout layout)
{
	return layout == MeshLayout::SkinnedMesh ? "Skinned" : "Static";
}

static bool GetLayout(MeshLayout layoutType, BufferLayout& layout)
{
	switch (layoutType)
	{
	case MeshLayout::StaticMesh:
		layout = s_StaticMeshLayout;
		return true;
	case MeshLayout::SkinnedMesh:
		layout = s_SkinnedMeshLayout;
		return true;
	default:
		CLIENT_ERROR("Unsupported mesh layout type: {0}", static_cast<int>(layoutType));
		return false;
	}
}

static Ref<Material> LoadMaterial(const std::string& materialPath)
{
	if (materialPath.empty() || materialPath == "DefaultMaterial")
		return Material::GetDefaultMaterial();

	// A material that fails to load is the checker placeholder, which keeps its path
	Ref<Material> material = AssetManager::GetAsset<Material>(materialPath);
	return material ? material : Material::GetDefaultMaterial();
}

static void EncodeMatrix(tinyxml2::XMLElement* pElement, const Matrix4x4& matrix)
{
	std::ostringstream text;
	text.precision(9);
	for (int row = 0; row < 4; row++)
	{
		for (int column = 0; column < 4; column++)
			text << matrix(row, column) << (row == 3 && column == 3 ? "" : " ");
	}
	pElement->SetText(text.str().c_str());
}

static void DecodeMatrix(const tinyxml2::XMLElement* pElement, Matrix4x4& matrix)
{
	if (!pElement || !pElement->GetText())
		return;

	std::istringstream text(pElement->GetText());
	for (int row = 0; row < 4; row++)
	{
		for (int column = 0; column < 4; column++)
			text >> matrix.m[row][column];
	}
}

bool MeshSerializer::Serialize(const std::filesystem::path& filepath, const Ref<Mesh>& mesh, MeshLayout layoutType)
{
	const std::vector<float>& vertices = mesh->GetVertexList();
	const std::vector<uint32_t>& indices = mesh->GetIndexList();

	tinyxml2::XMLDocument doc;
	tinyxml2::XMLElement* pRoot = doc.NewElement("StaticMesh");
	doc.InsertFirstChild(pRoot);
	pRoot->SetAttribute("Version", MESH_VERSION);
	pRoot->SetAttribute("Layout", LayoutName(layoutType));
	pRoot->SetAttribute("VertexFloats", (uint64_t)vertices.size());
	pRoot->SetAttribute("Indices", (uint64_t)indices.size());

	for (const Ref<Material>& material : mesh->GetMaterials())
	{
		tinyxml2::XMLElement* pMaterial = pRoot->InsertNewChildElement("Material");
		if (material && material != Material::GetDefaultMaterial())
			SerializationUtils::Encode(pMaterial, material->GetFilepath());
	}

	for (const Submesh& submesh : mesh->GetSubmeshes())
	{
		tinyxml2::XMLElement* pSubmesh = pRoot->InsertNewChildElement("Submesh");
		pSubmesh->SetAttribute("FirstIndex", submesh.firstIndex);
		pSubmesh->SetAttribute("VertexOffset", submesh.vertexOffset);
		pSubmesh->SetAttribute("IndexCount", submesh.indexCount);
		pSubmesh->SetAttribute("VertexCount", submesh.vertexCount);
		pSubmesh->SetAttribute("Material", submesh.materialIndex);
		EncodeMatrix(pSubmesh->InsertNewChildElement("Transform"), submesh.transform);
		EncodeMatrix(pSubmesh->InsertNewChildElement("LocalTransform"), submesh.localTransform);
		SerializationUtils::Encode(pSubmesh->InsertNewChildElement("BoundsMin"), submesh.boundingBox.Min());
		SerializationUtils::Encode(pSubmesh->InsertNewChildElement("BoundsMax"), submesh.boundingBox.Max());
	}

	tinyxml2::XMLPrinter printer;
	doc.Print(&printer);

	std::vector<uint8_t> data(vertices.size() * sizeof(float) + indices.size() * sizeof(uint32_t));
	if (!vertices.empty())
		std::memcpy(data.data(), vertices.data(), vertices.size() * sizeof(float));
	if (!indices.empty())
		std::memcpy(data.data() + vertices.size() * sizeof(float), indices.data(), indices.size() * sizeof(uint32_t));

	return WriteWithHeader(filepath, printer.CStr(), data, 0);
}

bool MeshSerializer::SplitHeader(const std::vector<uint8_t>& data, std::string& header, size_t& binaryStart)
{
	if (data.empty() || data[0] != '<')
		return false;

	const char* begin = reinterpret_cast<const char*>(data.data());
	const char* end = begin + data.size();
	const char* found = std::search(begin, end, k_HeaderEnd, k_HeaderEnd + std::strlen(k_HeaderEnd));
	if (found == end)
		return false;

	binaryStart = (found - begin) + std::strlen(k_HeaderEnd);
	header.assign(begin, binaryStart);
	if (binaryStart < data.size() && data[binaryStart] == '\n')
		binaryStart++;
	return true;
}

bool MeshSerializer::WriteWithHeader(const std::filesystem::path& filepath, const std::string& header, const std::vector<uint8_t>& data, size_t binaryStart)
{
	std::ofstream out(filepath, std::ios::binary);
	if (!out.is_open())
	{
		CLIENT_ERROR("Could not open file {0} for writing", filepath.string());
		return false;
	}

	// One newline separates the header from the binary data
	std::string trimmedHeader = header;
	while (!trimmedHeader.empty() && std::isspace((unsigned char)trimmedHeader.back()))
		trimmedHeader.pop_back();

	out << trimmedHeader << '\n';
	if (binaryStart < data.size())
		out.write(reinterpret_cast<const char*>(data.data() + binaryStart), data.size() - binaryStart);
	return out.good();
}

static Ref<Mesh> DeserializeWithHeader(const std::filesystem::path& filepath, const std::vector<uint8_t>& data, const std::string& header, size_t binaryStart)
{
	tinyxml2::XMLDocument doc;
	if (doc.Parse(header.c_str(), header.size()) != tinyxml2::XML_SUCCESS)
	{
		CLIENT_ERROR("Could not read mesh header {0}. {1} on line {2}", filepath, doc.ErrorName(), doc.ErrorLineNum());
		return nullptr;
	}

	tinyxml2::XMLElement* pRoot = doc.FirstChildElement("StaticMesh");
	if (!pRoot)
	{
		CLIENT_ERROR("Mesh file has no StaticMesh element: {0}", filepath);
		return nullptr;
	}

	int version = 0;
	pRoot->QueryIntAttribute("Version", &version);
	if (version != MESH_VERSION)
	{
		CLIENT_ERROR("Unsupported mesh version {0}: {1}", version, filepath);
		return nullptr;
	}

	const char* layoutName = pRoot->Attribute("Layout");
	MeshLayout layoutType = layoutName && std::strcmp(layoutName, "Skinned") == 0 ? MeshLayout::SkinnedMesh : MeshLayout::StaticMesh;
	BufferLayout layout;
	if (!GetLayout(layoutType, layout))
		return nullptr;

	uint64_t vertexFloats = 0, indexCount = 0;
	pRoot->QueryUnsigned64Attribute("VertexFloats", &vertexFloats);
	pRoot->QueryUnsigned64Attribute("Indices", &indexCount);

	size_t binarySize = data.size() - binaryStart;
	if (binarySize != vertexFloats * sizeof(float) + indexCount * sizeof(uint32_t))
	{
		CLIENT_ERROR("Mesh data is {0} bytes but the header describes {1}: {2}", binarySize, vertexFloats * sizeof(float) + indexCount * sizeof(uint32_t), filepath);
		return nullptr;
	}

	std::vector<float> vertices(vertexFloats);
	std::vector<uint32_t> indices(indexCount);
	if (vertexFloats > 0)
		std::memcpy(vertices.data(), data.data() + binaryStart, vertexFloats * sizeof(float));
	if (indexCount > 0)
		std::memcpy(indices.data(), data.data() + binaryStart + vertexFloats * sizeof(float), indexCount * sizeof(uint32_t));

	std::vector<Ref<Material>> materials;
	for (tinyxml2::XMLElement* pMaterial = pRoot->FirstChildElement("Material"); pMaterial; pMaterial = pMaterial->NextSiblingElement("Material"))
	{
		const char* materialPath = pMaterial->Attribute("Filepath");
		materials.push_back(LoadMaterial(materialPath ? materialPath : ""));
	}

	std::vector<Submesh> submeshes;
	for (tinyxml2::XMLElement* pSubmesh = pRoot->FirstChildElement("Submesh"); pSubmesh; pSubmesh = pSubmesh->NextSiblingElement("Submesh"))
	{
		Submesh submesh{};
		pSubmesh->QueryUnsignedAttribute("FirstIndex", &submesh.firstIndex);
		pSubmesh->QueryUnsignedAttribute("VertexOffset", &submesh.vertexOffset);
		pSubmesh->QueryUnsignedAttribute("IndexCount", &submesh.indexCount);
		pSubmesh->QueryUnsignedAttribute("VertexCount", &submesh.vertexCount);
		pSubmesh->QueryUnsignedAttribute("Material", &submesh.materialIndex);
		DecodeMatrix(pSubmesh->FirstChildElement("Transform"), submesh.transform);
		DecodeMatrix(pSubmesh->FirstChildElement("LocalTransform"), submesh.localTransform);
		Vector3f min, max;
		SerializationUtils::Decode(pSubmesh->FirstChildElement("BoundsMin"), min);
		SerializationUtils::Decode(pSubmesh->FirstChildElement("BoundsMax"), max);
		submesh.boundingBox = BoundingBox(min, max);

		if (submesh.materialIndex >= materials.size())
		{
			CLIENT_ERROR("Submesh uses material {0} but the mesh only has {1}: {2}", submesh.materialIndex, materials.size(), filepath);
			submesh.materialIndex = 0;
			if (materials.empty())
				materials.push_back(Material::GetDefaultMaterial());
		}
		submeshes.push_back(submesh);
	}

	return CreateRef<Mesh>(vertices, indices, submeshes, materials, layout);
}

// The old binary-only format, still read so existing projects keep working
static Ref<Mesh> DeserializeBinary(std::istream& in)
{
	uint32_t magic = 0;
	in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	if (magic != MESH_MAGIC)
	{
		CLIENT_ERROR("Invalid mesh file format");
		return nullptr;
	}
	uint32_t version = 0;
	in.read(reinterpret_cast<char*>(&version), sizeof(version));
	if (version != MESH_BINARY_VERSION)
	{
		CLIENT_ERROR("Unsupported mesh version: {0}", version);
		return nullptr;
	}
	// Read vertex and index count
	uint32_t vertexCount = 0, indexCount = 0;
	in.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
	std::vector<float> vertices(vertexCount);
	in.read(reinterpret_cast<char*>(vertices.data()), vertexCount * sizeof(float));
	in.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
	std::vector<uint32_t> indices(indexCount);
	in.read(reinterpret_cast<char*>(indices.data()), indexCount * sizeof(uint32_t));
	// Read submeshes
	uint32_t submeshCount = 0;
	in.read(reinterpret_cast<char*>(&submeshCount), sizeof(submeshCount));
	std::vector<Submesh> submeshes(submeshCount);

	for (auto& sm : submeshes)
	{
		in.read(reinterpret_cast<char*>(&sm.firstIndex), sizeof(sm.firstIndex));
		in.read(reinterpret_cast<char*>(&sm.vertexOffset), sizeof(sm.vertexOffset));
		in.read(reinterpret_cast<char*>(&sm.indexCount), sizeof(sm.indexCount));
		in.read(reinterpret_cast<char*>(&sm.vertexCount), sizeof(sm.vertexCount));
		in.read(reinterpret_cast<char*>(&sm.materialIndex), sizeof(sm.materialIndex));
		in.read(reinterpret_cast<char*>(&sm.transform), sizeof(sm.transform));
		in.read(reinterpret_cast<char*>(&sm.localTransform), sizeof(sm.localTransform));
		Vector3f min, max;
		in.read(reinterpret_cast<char*>(&min), sizeof(Vector3f));
		in.read(reinterpret_cast<char*>(&max), sizeof(Vector3f));
		sm.boundingBox = BoundingBox(min, max);
	}

	// Read layout type
	MeshLayout layoutType;
	in.read(reinterpret_cast<char*>(&layoutType), sizeof(layoutType));

	BufferLayout layout;
	GetLayout(layoutType, layout);

	// Read materials
	std::vector<Ref<Material>> materials;
	uint32_t materialCount = 0;
	in.read(reinterpret_cast<char*>(&materialCount), sizeof(materialCount));
	materials.resize(materialCount);
	for (uint32_t i = 0; i < materialCount; ++i)
	{
		uint32_t nameLength = 0;
		in.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
		std::string materialName(nameLength, '\0');
		in.read(materialName.data(), nameLength);
		materials[i] = LoadMaterial(materialName);
	}

	return CreateRef<Mesh>(vertices, indices, submeshes, materials, layout);
}

Ref<Mesh> MeshSerializer::DeserializeFromMemory(const std::filesystem::path& filepath, const std::vector<uint8_t>& data)
{
	std::string header;
	size_t binaryStart = 0;
	if (SplitHeader(data, header, binaryStart))
		return DeserializeWithHeader(filepath, data, header, binaryStart);

	std::istringstream in(std::string(data.begin(), data.end()), std::ios::binary);
	return DeserializeBinary(in);
}

Ref<Mesh> MeshSerializer::Deserialize(const std::filesystem::path& filepath)
{
	std::ifstream in(filepath, std::ios::binary);
	if (!in)
	{
		CLIENT_ERROR("Could not open mesh file {0}", filepath.string());
		return nullptr;
	}

	std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return DeserializeFromMemory(filepath, data);
}
