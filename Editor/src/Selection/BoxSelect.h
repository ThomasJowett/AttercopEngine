#pragma once

#include "Math/BoundingBox.h"
#include "Math/Frustum.h"
#include "math/Matrix.h"
#include "math/Vector2f.h"

// Box select by bounds: whether an entity's bounds touch a rectangle on screen
namespace BoxSelect
{
	// In normalised device coordinates: -1 to 1, y up
	struct Rect
	{
		Vector2f min;
		Vector2f max;
	};

	// The rectangle between two corners given in viewport pixels, y down
	Rect FromPixels(const Vector2f& corner, const Vector2f& otherCorner, const Vector2f& viewportSize);

	// The projection scaled and offset so the rectangle fills clip space
	Matrix4x4 PickProjection(const Matrix4x4& projection, const Rect& rect);

	// Tests boxes against one rectangle seen by one camera, orthographic or perspective
	class Tester
	{
	public:
		// view is the inverse of the camera's transform
		Tester(const Matrix4x4& projection, const Matrix4x4& view, const Rect& rect);

		// The box in local space after the transform; a box of no size is a point
		bool Touches(const BoundingBox& localBox, const Matrix4x4& transform) const;
		// Only the pick frustum's planes, which also accept boxes near the rectangle's corners
		bool TouchesFrustum(const BoundingBox& localBox, const Matrix4x4& transform) const;

	private:
		Matrix4x4 m_ViewProjection;
		Frustum m_PickFrustum;
		Rect m_Rect;
	};
}
