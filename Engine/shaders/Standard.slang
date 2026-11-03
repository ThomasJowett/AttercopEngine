#include "ShaderBindings.h"
import Camera;
import Model;

[[vk::binding(BINDING_ALBEDO)]] Sampler2D u_Albedo;

struct VertexOutput
{
	float4 position : SV_Position;
	float3 worldPosition;
	float2 texCoord;
	float3 normal;
};

struct FragmentOutput
{
	float4 colour : SV_Target0;
	int entityId : SV_Target1;
};

[shader("vertex")]
VertexOutput vs_main(MeshInput input)
{
	VertexOutput output;
	float4 world = mul(u_Model.modelMatrix, float4(input.position, 1.0));
	output.position = mul(u_Camera.viewProjection, world);
	output.worldPosition = world.xyz;
	output.texCoord = (input.texCoord + u_Model.textureOffset) * u_Model.tilingFactor;
	output.normal = normalize(mul(u_Model.modelMatrix, float4(input.normal, 0.0)).xyz);
	return output;
}

[shader("fragment")]
FragmentOutput fs_main(VertexOutput input)
{
	FragmentOutput output;
	output.colour = u_Albedo.Sample(input.texCoord) * u_Model.colour;
	if (output.colour.a <= 0.0001)
		discard;
	output.entityId = u_Model.entityId;
	return output;
}
