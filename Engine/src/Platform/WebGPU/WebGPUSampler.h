#pragma once

#include "Renderer/Sampler.h"

#include <webgpu/webgpu.hpp>

class WebGPUSampler : public Sampler
{
public:
	WebGPUSampler(Texture::FilterMethod filterMethod, Texture::WrapMethod wrapMethod, bool anisotropic);
	virtual ~WebGPUSampler();

	const wgpu::Sampler& GetSampler() const { return m_Sampler; }

	static wgpu::SamplerDescriptor CreateDescriptor(Texture::FilterMethod filterMethod, Texture::WrapMethod wrapMethod, bool anisotropic = true);

private:
	wgpu::Sampler m_Sampler;
};
