// The TEXTURE_SLOT_COUNT textures a sprite or text batch draws from, picked per vertex by slot index

#include "ShaderBindings.h"

[[vk::binding(BINDING_TEXTURE_SLOTS + 0)]] Sampler2D u_TextureSlot0;
[[vk::binding(BINDING_TEXTURE_SLOTS + 2)]] Sampler2D u_TextureSlot1;
[[vk::binding(BINDING_TEXTURE_SLOTS + 4)]] Sampler2D u_TextureSlot2;
[[vk::binding(BINDING_TEXTURE_SLOTS + 6)]] Sampler2D u_TextureSlot3;
[[vk::binding(BINDING_TEXTURE_SLOTS + 8)]] Sampler2D u_TextureSlot4;
[[vk::binding(BINDING_TEXTURE_SLOTS + 10)]] Sampler2D u_TextureSlot5;
[[vk::binding(BINDING_TEXTURE_SLOTS + 12)]] Sampler2D u_TextureSlot6;
[[vk::binding(BINDING_TEXTURE_SLOTS + 14)]] Sampler2D u_TextureSlot7;

// WGSL can't index an array of textures, so the slot is picked with a switch.
// Derivatives are taken before the switch, as WGSL needs uniform control flow for them.
float4 SampleSlot(int slot, float2 uv)
{
	float2 dx = ddx(uv);
	float2 dy = ddy(uv);
	switch (slot)
	{
	case 0: return u_TextureSlot0.SampleGrad(uv, dx, dy);
	case 1: return u_TextureSlot1.SampleGrad(uv, dx, dy);
	case 2: return u_TextureSlot2.SampleGrad(uv, dx, dy);
	case 3: return u_TextureSlot3.SampleGrad(uv, dx, dy);
	case 4: return u_TextureSlot4.SampleGrad(uv, dx, dy);
	case 5: return u_TextureSlot5.SampleGrad(uv, dx, dy);
	case 6: return u_TextureSlot6.SampleGrad(uv, dx, dy);
	default: return u_TextureSlot7.SampleGrad(uv, dx, dy);
	}
}

float2 SlotSize(int slot)
{
	uint width, height;
	switch (slot)
	{
	case 0: u_TextureSlot0.GetDimensions(width, height); break;
	case 1: u_TextureSlot1.GetDimensions(width, height); break;
	case 2: u_TextureSlot2.GetDimensions(width, height); break;
	case 3: u_TextureSlot3.GetDimensions(width, height); break;
	case 4: u_TextureSlot4.GetDimensions(width, height); break;
	case 5: u_TextureSlot5.GetDimensions(width, height); break;
	case 6: u_TextureSlot6.GetDimensions(width, height); break;
	default: u_TextureSlot7.GetDimensions(width, height); break;
	}
	return float2(width, height);
}
