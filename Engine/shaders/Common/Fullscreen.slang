// Full-screen quad vertex stage shared by the post-process and final pass shaders
#include "ShaderBindings.h"

// PostProcessData in PostProcess.h
struct PostProcess
{
	float2 screenSize;
	float time;
	float deltaTime;
	float customParam1;
	float customParam2;
	float customParam3;
	float customParam4;
};

[[vk::binding(BINDING_POST_PROCESS)]] ConstantBuffer<PostProcess> u_PostProcess;

struct FullscreenInput
{
	[[vk::location(0)]] float3 position;
	[[vk::location(1)]] float2 texCoord;
};

struct FullscreenOutput
{
	float4 position : SV_Position;
	float2 texCoord;
};

// OpenGL textures start at the bottom row, WebGPU textures at the top
float2 FramebufferUV(float2 uv)
{
	__target_switch
	{
	case glsl: return float2(uv.x, 1.0 - uv.y);
	default: return uv;
	}
}

FullscreenOutput FullscreenVertex(FullscreenInput input)
{
	FullscreenOutput output;
	output.position = float4(input.position, 1.0);
	output.texCoord = FramebufferUV(input.texCoord);
	return output;
}
