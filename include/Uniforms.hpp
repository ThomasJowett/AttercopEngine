#include <array>

namespace atcp
{
struct MyUniform {
    std::array<float, 4> colour;
    float time;
    float _pad[3];

};
static_assert(sizeof(MyUniform) % 16 == 0, "Struct must be 16 byte aligned");
} // namespace atcp
