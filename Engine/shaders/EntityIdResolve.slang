// Resolves a multisampled entity id attachment on WebGPU
#include "ShaderBindings.h"
import Fullscreen;

[[vk::binding(BINDING_INPUT_TEXTURE)]] Texture2DMS<int> u_EntityIdMultisampled;

[shader("vertex")]
float4 vs_main(FullscreenInput input) : SV_Position
{
	return float4(input.position, 1.0);
}

// Averaging entity ids is meaningless, so this picks sample 0
[shader("fragment")]
int fs_main(float4 fragCoord : SV_Position) : SV_Target0
{
	return u_EntityIdMultisampled.Load(int2(fragCoord.xy), 0);
}
