#include <webgpu/webgpu.hpp>

#include <string>

namespace atcp
{
static std::string WGPUFeatureNamesToStr(wgpu::FeatureName feature)
{
	switch (feature)
	{
	case wgpu::FeatureName::Undefined:
		return "Undefined";
	case wgpu::FeatureName::DepthClipControl:
		return "DepthClipControl";
	case wgpu::FeatureName::Depth32FloatStencil8:
		return "Depth32FloatStencil8";
	case wgpu::FeatureName::TimestampQuery:
		return "TimestampQuery";
	case wgpu::FeatureName::TextureCompressionBC:
		return "TextureCompressionBC";
	case wgpu::FeatureName::TextureCompressionETC2:
		return "TextureCompressionETC2";
	case wgpu::FeatureName::TextureCompressionASTC:
		return "TextureCompressionASTC";
	case wgpu::FeatureName::IndirectFirstInstance:
		return "IndirectFirstInstance";
	case wgpu::FeatureName::ShaderF16:
		return "ShaderF16";
	case wgpu::FeatureName::RG11B10UfloatRenderable:
		return "RG11B10UfloatRenderable";
	case wgpu::FeatureName::BGRA8UnormStorage:
		return "BGRA8UnormStorage";
	case wgpu::FeatureName::Float32Filterable:
		return "Float32Filterable";
	default:
		return "Unknown WGPUFeatureName value: " + std::to_string(feature);
	}
}
}