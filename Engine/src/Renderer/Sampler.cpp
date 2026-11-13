#include "Sampler.h"
#include "Renderer.h"

#include "Platform/OpenGL/OpenGLSampler.h"
#include "Platform/WebGPU/WebGPUSampler.h"

#include <array>

static std::array<Ref<Sampler>, 12> s_Samplers;

Ref<Sampler> Sampler::Get(Texture::FilterMethod filterMethod, Texture::WrapMethod wrapMethod, bool anisotropic)
{
	Ref<Sampler>& sampler = s_Samplers[(anisotropic ? 6 : 0) + (size_t)filterMethod * 3 + (size_t)wrapMethod];
	if (sampler)
		return sampler;

	switch (Renderer::GetAPI())
	{
	case RendererAPI::API::OpenGL:
		sampler = CreateRef<OpenGLSampler>(filterMethod, wrapMethod, anisotropic);
		break;
	case RendererAPI::API::WebGPU:
		sampler = CreateRef<WebGPUSampler>(filterMethod, wrapMethod, anisotropic);
		break;
	default:
		CORE_ASSERT(false, "Could not create Sampler: Invalid Renderer API")
		break;
	}
	return sampler;
}

void Sampler::ClearCache()
{
	s_Samplers.fill(nullptr);
}
