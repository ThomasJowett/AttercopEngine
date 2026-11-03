// One direction of a separable gaussian blur: customParam1 > 0.5 is horizontal, customParam2 is the strength
#include "ShaderBindings.h"
import Fullscreen;

[[vk::binding(BINDING_INPUT_TEXTURE)]] Sampler2D u_Colour;

static const int M = 16;
static const int N = 2 * M + 1;

// sigma = 10
static const float coeffs[N] = {
	0.012318109844189502,
	0.014381474814203989,
	0.016623532195728208,
	0.019024086115486723,
	0.02155484948872149,
	0.02417948052890078,
	0.02685404941667096,
	0.0295279624870386,
	0.03214534135442581,
	0.03464682117793548,
	0.0369716985390341,
	0.039060328279673276,
	0.040856643282313365,
	0.04231065439216247,
	0.043380781642569775,
	0.044035873841196206,
	0.04425662519949865,
	0.044035873841196206,
	0.043380781642569775,
	0.04231065439216247,
	0.040856643282313365,
	0.039060328279673276,
	0.0369716985390341,
	0.03464682117793548,
	0.03214534135442581,
	0.0295279624870386,
	0.02685404941667096,
	0.02417948052890078,
	0.02155484948872149,
	0.019024086115486723,
	0.016623532195728208,
	0.014381474814203989,
	0.012318109844189502
};

[shader("vertex")]
FullscreenOutput vs_main(FullscreenInput input)
{
	return FullscreenVertex(input);
}

[shader("fragment")]
float4 fs_main(FullscreenOutput input) : SV_Target0
{
	float2 step = u_PostProcess.customParam1 > 0.5
		? float2(1.0 / u_PostProcess.screenSize.x, 0.0)
		: float2(0.0, 1.0 / u_PostProcess.screenSize.y);
	step *= u_PostProcess.customParam2;

	float4 sum = float4(0.0);
	for (int i = 0; i < N; ++i)
		sum += coeffs[i] * u_Colour.Sample(input.texCoord + float(i - M) * step);
	return sum;
}
