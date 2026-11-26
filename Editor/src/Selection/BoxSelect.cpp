#include "BoxSelect.h"

#include "math/Vector4f.h"

#include <algorithm>
#include <vector>

namespace BoxSelect
{
	namespace
	{
		void Corners(const BoundingBox& box, const Matrix4x4& transform, Vector3f corners[8])
		{
			const Vector3f min = box.Min();
			const Vector3f max = box.Max();
			// Bit 0, 1 and 2 of a corner's index pick max over min for x, y and z
			for (int i = 0; i < 8; ++i)
				corners[i] = transform * Vector3f((i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 4) ? max.z : min.z);
		}

		float Cross(const Vector2f& o, const Vector2f& a, const Vector2f& b)
		{
			return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
		}

		// Counter-clockwise, without repeated or collinear points
		std::vector<Vector2f> ConvexHull(std::vector<Vector2f> points)
		{
			std::sort(points.begin(), points.end(), [](const Vector2f& a, const Vector2f& b)
				{
					return a.x < b.x || (a.x == b.x && a.y < b.y);
				});
			if (points.size() < 3)
				return points;

			std::vector<Vector2f> hull(points.size() * 2);
			size_t count = 0;
			for (const Vector2f& point : points)
			{
				while (count >= 2 && Cross(hull[count - 2], hull[count - 1], point) <= 0.0f)
					--count;
				hull[count++] = point;
			}
			const size_t lowerCount = count + 1;
			for (auto point = points.rbegin() + 1; point != points.rend(); ++point)
			{
				while (count >= lowerCount && Cross(hull[count - 2], hull[count - 1], *point) <= 0.0f)
					--count;
				hull[count++] = *point;
			}
			hull.resize(count - 1);
			return hull;
		}

		void Project(const Vector2f* points, size_t count, const Vector2f& axis, float& min, float& max)
		{
			min = max = points[0].x * axis.x + points[0].y * axis.y;
			for (size_t i = 1; i < count; ++i)
			{
				const float value = points[i].x * axis.x + points[i].y * axis.y;
				min = std::min(min, value);
				max = std::max(max, value);
			}
		}

		// Separating axes: the rectangle's two and each hull edge's normal
		bool HullTouchesRect(const std::vector<Vector2f>& hull, const Rect& rect)
		{
			const Vector2f rectCorners[4] = { rect.min, Vector2f(rect.max.x, rect.min.y), rect.max, Vector2f(rect.min.x, rect.max.y) };

			std::vector<Vector2f> axes = { Vector2f(1.0f, 0.0f), Vector2f(0.0f, 1.0f) };
			for (size_t i = 0; hull.size() > 1 && i < hull.size(); ++i)
			{
				const Vector2f edge = hull[(i + 1) % hull.size()] - hull[i];
				axes.push_back(Vector2f(-edge.y, edge.x));
			}

			for (const Vector2f& axis : axes)
			{
				float hullMin, hullMax, rectMin, rectMax;
				Project(hull.data(), hull.size(), axis, hullMin, hullMax);
				Project(rectCorners, 4, axis, rectMin, rectMax);
				if (hullMax < rectMin || rectMax < hullMin)
					return false;
			}
			return true;
		}
	}

	Rect FromPixels(const Vector2f& corner, const Vector2f& otherCorner, const Vector2f& viewportSize)
	{
		auto toNdc = [&viewportSize](const Vector2f& pixel)
		{
			return Vector2f(pixel.x / viewportSize.x * 2.0f - 1.0f, 1.0f - pixel.y / viewportSize.y * 2.0f);
		};
		const Vector2f a = toNdc(corner);
		const Vector2f b = toNdc(otherCorner);
		return { Vector2f(std::min(a.x, b.x), std::min(a.y, b.y)), Vector2f(std::max(a.x, b.x), std::max(a.y, b.y)) };
	}

	Matrix4x4 PickProjection(const Matrix4x4& projection, const Rect& rect)
	{
		// Clip x' = (x - centre * w) / halfSize, the same for y
		const float halfX = std::max((rect.max.x - rect.min.x) * 0.5f, 1e-6f);
		const float halfY = std::max((rect.max.y - rect.min.y) * 0.5f, 1e-6f);
		const float centreX = (rect.max.x + rect.min.x) * 0.5f;
		const float centreY = (rect.max.y + rect.min.y) * 0.5f;

		Matrix4x4 fit;
		fit.m[0][0] = 1.0f / halfX;
		fit.m[0][3] = -centreX / halfX;
		fit.m[1][1] = 1.0f / halfY;
		fit.m[1][3] = -centreY / halfY;
		return fit * projection;
	}

	Tester::Tester(const Matrix4x4& projection, const Matrix4x4& view, const Rect& rect)
		: m_ViewProjection(projection * view), m_PickFrustum(PickProjection(projection, rect) * view), m_Rect(rect)
	{
	}

	bool Tester::TouchesFrustum(const BoundingBox& localBox, const Matrix4x4& transform) const
	{
		if (!localBox.IsValid())
			return false;
		Vector3f corners[8];
		Corners(localBox, transform, corners);
		return m_PickFrustum.Intersects(corners, 8);
	}

	bool Tester::Touches(const BoundingBox& localBox, const Matrix4x4& transform) const
	{
		if (!localBox.IsValid())
			return false;
		Vector3f corners[8];
		Corners(localBox, transform, corners);
		if (!m_PickFrustum.Intersects(corners, 8))
			return false;

		// With a corner behind the near plane its projection is meaningless, so the plane test stands
		std::vector<Vector2f> projected;
		projected.reserve(8);
		for (const Vector3f& corner : corners)
		{
			const Vector4f clip = m_ViewProjection * Vector4f(corner, 1.0f);
			if (clip.w <= 1e-6f || clip.z < 0.0f)
				return true;
			projected.push_back(Vector2f(clip.x / clip.w, clip.y / clip.w));
		}
		return HullTouchesRect(ConvexHull(projected), m_Rect);
	}
}
