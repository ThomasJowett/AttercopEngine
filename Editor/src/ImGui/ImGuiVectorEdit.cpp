#include "ImGuiVectorEdit.h"

#include "math/Vector2f.h"
#include "math/Vector3f.h"

#include "imgui/imgui_internal.h"

#include <string>

namespace
{
	// A drag per part, each reset by a right-click; mixed parts show "--" until they're active
	bool VectorParts(const char* label, float* parts, const float* resets, int count, const bool* mixed, int& changedPart)
	{
		static const char* names[] = { "X", "Y", "Z" };
		static const ImVec4 colours[] = { { 245, 0, 0, 255 }, { 0, 245, 0, 255 }, { 0, 0, 245, 255 } };

		bool edited = false;
		const float width = ImGui::CalcItemWidth();

		ImGui::BeginGroup();
		for (int i = 0; i < count; ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			ImGui::TextColored(colours[i], "%s", names[i]);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(width / (float)count - 20);
			const std::string id = "##" + std::string(label) + names[i];
			const bool showMixed = mixed && mixed[i] && ImGui::GetActiveID() != ImGui::GetID(id.c_str());
			if (ImGui::DragFloat(id.c_str(), &parts[i], 0.1f, 0.0f, 0.0f, showMixed ? "--" : "%.3f"))
			{
				edited = true;
				changedPart = i;
			}
			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				parts[i] = resets[i];
				edited = true;
				changedPart = i;
			}
		}

		ImGui::SameLine();
		ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));
		ImGui::EndGroup();
		return edited;
	}
}

bool ImGui::Vector(const char* label, Vector2f& vector, float resetValue)
{
	return Vector(label, vector, Vector2f(resetValue, resetValue));
}

bool ImGui::Vector(const char* label, Vector2f& vector, const Vector2f& resetValue)
{
	int changedPart = -1;
	return Vector(label, vector, resetValue, nullptr, changedPart);
}

bool ImGui::Vector(const char* label, Vector3f& vector, float resetValue)
{
	return Vector(label, vector, Vector3f(resetValue, resetValue, resetValue));
}

bool ImGui::Vector(const char* label, Vector3f& vector, const Vector3f& resetValue)
{
	int changedPart = -1;
	return Vector(label, vector, resetValue, nullptr, changedPart);
}

bool ImGui::Vector(const char* label, Vector2f& vector, const Vector2f& resetValue, const bool mixed[2], int& changedPart)
{
	float parts[2] = { vector.x, vector.y };
	const float resets[2] = { resetValue.x, resetValue.y };
	if (!VectorParts(label, parts, resets, 2, mixed, changedPart))
		return false;
	vector = Vector2f(parts[0], parts[1]);
	return true;
}

bool ImGui::Vector(const char* label, Vector3f& vector, const Vector3f& resetValue, const bool mixed[3], int& changedPart)
{
	float parts[3] = { vector.x, vector.y, vector.z };
	const float resets[3] = { resetValue.x, resetValue.y, resetValue.z };
	if (!VectorParts(label, parts, resets, 3, mixed, changedPart))
		return false;
	vector = Vector3f(parts[0], parts[1], parts[2]);
	return true;
}
