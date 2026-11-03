// One pixel wide lines, drawn as a line list
import Camera;

struct VertexInput
{
	[[vk::location(0)]] float3 position;
	[[vk::location(1)]] float4 colour;
	[[vk::location(2)]] int entityId;
};

struct VertexOutput
{
	float4 position : SV_Position;
	float4 colour;
	nointerpolation int entityId;
};

struct FragmentOutput
{
	float4 colour : SV_Target0;
	int entityId : SV_Target1;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input)
{
	VertexOutput output;
	output.position = mul(u_Camera.viewProjection, float4(input.position, 1.0));
	output.colour = input.colour;
	output.entityId = input.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fs_main(VertexOutput input)
{
	FragmentOutput output;
	output.colour = input.colour;
	output.entityId = input.entityId;
	return output;
}
