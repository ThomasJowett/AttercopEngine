// Editor grid drawn on a plane, with lines that fade out towards the horizon
#include "ShaderBindings.h"
import Camera;
import Model;

// Unused, but Renderer.cpp always binds an albedo texture, and WebGPU's auto layout only keeps
// bindings the shader reads - fs_main reads one texel at weight 0.
[[vk::binding(BINDING_ALBEDO)]] Sampler2D u_Albedo;

struct VertexOutput
{
	float4 position : SV_Position;
	float3 worldPosition;
	float2 texCoord;
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
	output.texCoord = input.texCoord;
	return output;
}

[shader("fragment")]
FragmentOutput fs_main(VertexOutput input)
{
	float3 coord = input.worldPosition;
	float dist = length(coord - u_Camera.eyePosition);

	float3 deriv = fwidth(coord);
	bool3 isFlatAxis = deriv < float3(0.00001);
	float3 derivEpsilon = float3(0.0001);

	// The tiling factor doubles as the fade-to-horizon radius, so it can be set per material
	float fadeRadius = u_Model.tilingFactor;

	// Minor grid lines
	float3 grid = select(isFlatAxis, float3(1000.0), abs(frac(coord - 0.5) - 0.5) / max(deriv, derivEpsilon));
	float size = min(min(grid.x, grid.y), grid.z);
	float val = 1.0 - min(size, 1.0);
	val *= clamp(1.0 - ((dist - fadeRadius) / fadeRadius), 0.0, 1.0);

	// Major grid lines (axes)
	const float extraThickness = 0.5;
	float3 axes = select(isFlatAxis, float3(1000.0), abs(coord) / max(deriv, derivEpsilon) - extraThickness);
	size = min(min(axes.x, axes.y), axes.z);
	float axisVal = 1.0 - min(size, 1.0);
	axisVal *= clamp(1.0 - ((dist - fadeRadius * 1.5) / (fadeRadius * 1.5)), 0.0, 1.0);

	float unusedAlbedo = u_Albedo.Sample(input.texCoord).r * 0.0;
	float finalAlpha = max(val * 0.6, axisVal) + unusedAlbedo;
	if (finalAlpha <= 0.0)
		discard;

	float3 colour = u_Model.colour.rgb;
	if (axisVal > val * 0.6)
		colour *= 0.3; // Darken major grid lines

	FragmentOutput output;
	output.colour = float4(colour, finalAlpha);
	output.entityId = u_Model.entityId;
	return output;
}
