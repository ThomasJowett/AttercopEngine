#include <webgpu/webgpu.h>

#include <string>

namespace atcp
{
	static std::string WGPUFeatureNamesToStr(WGPUFeatureName feature)
	{
		switch (feature)
		{
		case WGPUFeatureName_Undefined:
			return "Undefined";
		case WGPUFeatureName_DepthClipControl:
			return "DepthClipControl";
		case WGPUFeatureName_Depth32FloatStencil8:
			return "Depth32FloatStencil8";
		case WGPUFeatureName_TimestampQuery:
			return "TimestampQuery";
		case WGPUFeatureName_PipelineStatisticsQuery:
			return "PipelineStatisticsQuery";
		case WGPUFeatureName_TextureCompressionBC:
			return "TextureCompressionBC";
		case WGPUFeatureName_TextureCompressionETC2:
			return "TextureCompressionETC2";
		case WGPUFeatureName_TextureCompressionASTC:
			return "TextureCompressionASTC";
		case WGPUFeatureName_IndirectFirstInstance:
			return "IndirectFirstInstance";
		case WGPUFeatureName_ShaderF16:
			return "ShaderF16";
		case WGPUFeatureName_RG11B10UfloatRenderable:
			return "RG11B10UfloatRenderable";
		case WGPUFeatureName_BGRA8UnormStorage:
			return "BGRA8UnormStorage";
		case WGPUFeatureName_Force32:
			return "Force32";
		default:
			return "Unknown WGPUFeatureName value: " + std::to_string(feature);
		}
	}
}