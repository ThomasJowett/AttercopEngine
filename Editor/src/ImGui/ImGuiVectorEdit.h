#pragma once

class Vector2f;
class Vector3f;

namespace ImGui
{
	bool Vector(const char* label, Vector2f& vector, float resetValue = 0.0f);
	bool Vector(const char* label, Vector2f& vector, const Vector2f& resetValue);
	bool Vector(const char* label, Vector3f& vector, float resetValue = 0.0f);
	bool Vector(const char* label, Vector3f& vector, const Vector3f& resetValue);

	// Parts flagged in mixed show "--" until edited; changedPart is set to the part edited
	bool Vector(const char* label, Vector2f& vector, const Vector2f& resetValue, const bool mixed[2], int& changedPart);
	bool Vector(const char* label, Vector3f& vector, const Vector3f& resetValue, const bool mixed[3], int& changedPart);
}
