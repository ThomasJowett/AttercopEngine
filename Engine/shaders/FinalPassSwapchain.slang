#include "ShaderBindings.h"
import Fullscreen;

[[vk::binding(BINDING_INPUT_TEXTURE)]] Sampler2D u_SceneTexture;

// Undoes the WebGPU projection flip in Renderer::BeginScene
float4 SwapchainPosition(float4 position)
{
	__target_switch
	{
	case wgsl: return float4(position.x, -position.y, position.zw);
	default: return position;
	}
}

[shader("vertex")]
FullscreenOutput vs_main(FullscreenInput input)
{
	FullscreenOutput output = FullscreenVertex(input);
	output.position = SwapchainPosition(output.position);
	return output;
}

[shader("fragment")]
float4 fs_main(FullscreenOutput input) : SV_Target0
{
	return u_SceneTexture.Sample(input.texCoord);
}
