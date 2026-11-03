import Camera;
import TextureSlots;

struct VertexInput
{
	[[vk::location(0)]] float3 position;
	[[vk::location(1)]] float4 colour;
	[[vk::location(2)]] float2 texCoord;
	[[vk::location(3)]] float texIndex;
	[[vk::location(4)]] int entityId;
};

struct VertexOutput
{
	float4 position : SV_Position;
	float4 colour;
	float2 texCoord;
	nointerpolation float texIndex;
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
	output.texCoord = input.texCoord;
	output.texIndex = input.texIndex;
	output.entityId = input.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fs_main(VertexOutput input)
{
	FragmentOutput output;
	output.colour = SampleSlot(int(input.texIndex), input.texCoord) * input.colour;
	if (output.colour.a <= 0.0001)
		discard;
	output.entityId = input.entityId;
	return output;
}
