#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace atcp
{
class SimpleMeshParser
{
public:
    static bool LoadGeometry(const std::filesystem::path& path, std::vector<float>& vertexData, std::vector<uint16_t>& indexData);
};
} // namespace atcp
