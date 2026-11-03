#include "ShaderBindings.h"

struct Camera
{
	float4x4 viewProjection;
	float3 eyePosition;
	float2 viewportSize; // pixels
};

[[vk::binding(BINDING_CAMERA)]] ConstantBuffer<Camera> u_Camera;
