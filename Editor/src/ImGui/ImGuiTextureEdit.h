#pragma once

#include "imgui/imgui.h"

#include "Asset/Texture.h"

namespace ImGui
{
	// filterMethod/wrapMethod belong to the owner, never the shared texture
	bool Texture2DEdit(const char* label, Ref<Texture2D>& texture, Texture::FilterMethod& filterMethod, Texture::WrapMethod& wrapMethod,
		const ImVec2& size = ImVec2(64.0f, 64.0f));
	// The texture alone, for owners that show their filter and wrap themselves; mixed with no texture shows a dash
	bool Texture2DEdit(const char* label, Ref<Texture2D>& texture, const ImVec2& size = ImVec2(64.0f, 64.0f), bool mixed = false);
}
