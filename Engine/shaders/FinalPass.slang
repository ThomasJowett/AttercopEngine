// Copies the scene and its entity ids into the target the scene is shown in
#include "ShaderBindings.h"
import Fullscreen;

// Read with Load: OpenGL needs a combined sampler, WebGPU must not have one
#if defined(TARGET_GLSL)
[[vk::binding(BINDING_ENTITY_ID)]] Sampler2D<int> u_EntityId;
#else
[[vk::binding(BINDING_ENTITY_ID)]] Texture2D<int> u_EntityId;
#endif

[[vk::binding(BINDING_INPUT_TEXTURE)]] Sampler2D u_SceneTexture;

struct FragmentOutput
{
	float4 colour : SV_Target0;
	int entityId : SV_Target1;
};

[shader("vertex")]
FullscreenOutput vs_main(FullscreenInput input)
{
	return FullscreenVertex(input);
}

[shader("fragment")]
FragmentOutput fs_main(FullscreenOutput input)
{
	uint width, height;
	u_EntityId.GetDimensions(width, height);

	FragmentOutput output;
	output.colour = u_SceneTexture.Sample(input.texCoord);
	output.entityId = u_EntityId.Load(int3(int2(input.texCoord * float2(width, height)), 0));
	return output;
}
