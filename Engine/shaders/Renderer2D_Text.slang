// Multi-channel signed distance field text
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

float Median(float r, float g, float b)
{
	return max(min(r, g), min(max(r, g), b));
}

// How many screen pixels the atlas's 2 pixel distance range covers
float ScreenPxRange(int slot, float2 texCoord)
{
	float2 unitRange = float2(2.0) / SlotSize(slot);
	float2 screenTexSize = float2(1.0) / fwidth(texCoord);
	return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

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
	int slot = int(input.texIndex);
	float3 msd = SampleSlot(slot, input.texCoord).rgb;
	float screenPxDistance = ScreenPxRange(slot, input.texCoord) * (Median(msd.r, msd.g, msd.b) - 0.5);
	float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

	output.colour = float4(input.colour.rgb, input.colour.a * opacity);
	if (output.colour.a <= 0.0001)
		discard;
	output.entityId = input.entityId;
	return output;
}
