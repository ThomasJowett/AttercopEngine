#include "SimpleMeshParser.hpp"
namespace atcp
{
bool SimpleMeshParser::LoadGeometry(const std::filesystem::path& path, std::vector<float>& vertexData, std::vector<uint16_t>& indexData, int dimensions)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		return false;
	}

	vertexData.clear();
	indexData.clear();

	enum class Section {
		None,
		Vertices,
		Indices
	};
	Section currentSection = Section::None;

	float value;
	uint16_t index;
	std::string line;
	while (!file.eof())
	{
		getline(file, line);
		if (!line.empty() && line.back() == 'r') {
			line.pop_back();
		}

		if (line == "[vertices]") {
			currentSection = Section::Vertices;
		}
		else if (line == "[indices]") {
			currentSection = Section::Indices;
		}
		else if (line.empty() || line[0] == '#')
		{
			continue;
		}
		else if (currentSection == Section::Vertices) {
			std::istringstream iss(line);
			for (int i = 0; i < dimensions + 3; ++i) {
				iss >> value;
				vertexData.push_back(value);
			}
		}
		else if (currentSection == Section::Indices) {
			std::istringstream iss(line);
			for (int i = 0; i < 3; ++i) {
				iss >> index;
				indexData.push_back(index);
			}
		}
	}

	return true;
}
} // namespace atcp
