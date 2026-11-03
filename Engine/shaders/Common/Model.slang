#include "ShaderBindings.h"

// Per draw values for a mesh (SceneData::ModelBuffer in Renderer.cpp)
struct Model
{
	float4x4 modelMatrix;
	float4 colour;
	float2 textureOffset;
	float tilingFactor;
	int entityId;
};

[[vk::binding(BINDING_MODEL)]] ConstantBuffer<Model> u_Model;

struct MeshInput
{
	[[vk::location(0)]] float3 position;
	[[vk::location(1)]] float3 normal;
	[[vk::location(2)]] float3 tangent;
	[[vk::location(3)]] float2 texCoord;
};
