import Camera;

struct VertexInput
{
	[[vk::location(0)]] float3 worldPosition;
	[[vk::location(1)]] float3 localPosition;
	[[vk::location(2)]] float4 colour;
	[[vk::location(3)]] float thickness;
	[[vk::location(4)]] float fade;
	[[vk::location(5)]] int entityId;
};

struct VertexOutput
{
	float4 position : SV_Position;
	float3 localPosition;
	float4 colour;
	nointerpolation float thickness;
	nointerpolation float fade;
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
	output.position = mul(u_Camera.viewProjection, float4(input.worldPosition, 1.0));
	output.localPosition = input.localPosition;
	output.colour = input.colour;
	output.thickness = input.thickness;
	output.fade = input.fade;
	output.entityId = input.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fs_main(VertexOutput input)
{
	float dist = 1.0 - length(input.localPosition);
	float alpha = smoothstep(0.0, input.fade, dist);
	alpha *= smoothstep(input.thickness + input.fade, input.thickness, dist);
	if (alpha <= 0.0001)
		discard;

	FragmentOutput output;
	output.colour = float4(input.colour.rgb, input.colour.a * alpha);
	output.entityId = input.entityId;
	return output;
}
