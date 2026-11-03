// Copies a depth texture into the depth attachment, for WebGPU framebuffer copies
#include "ShaderBindings.h"
import Fullscreen;

[[vk::binding(BINDING_INPUT_TEXTURE)]] DepthTexture2D u_Depth;

[shader("vertex")]
float4 vs_main(FullscreenInput input) : SV_Position
{
	return float4(input.position, 1.0);
}

struct FragmentOutput
{
	float depth : SV_Depth;
};

[shader("fragment")]
FragmentOutput fs_main(float4 fragCoord : SV_Position)
{
	FragmentOutput output;
	output.depth = u_Depth.Load(int3(int2(fragCoord.xy), 0));
	return output;
}
