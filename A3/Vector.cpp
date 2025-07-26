#include "Vector.h"

namespace A3 {

float lengthSquared(const Vec3& v)
{
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

float length(const Vec3& v)
{
	return std::sqrt(lengthSquared(v));
}

float distanceSquared(const Vec3& v0, const Vec3& v1)
{
	return lengthSquared(v1 - v0);
}

float distance(const Vec3& v0, const Vec3& v1)
{
	return std::sqrt(distanceSquared(v1, v0));
}

Vec3 normalize(const Vec3& v)
{
	float lenSq = lengthSquared(v);
	if (lenSq == 0.0f)
		return Vec3{ 0.0f, 0.0f, 0.0f };

	float lenInv = 1.0f / std::sqrt(lenSq);
	return Vec3{ v.x * lenInv, v.y * lenInv, v.z * lenInv };
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs)
{
	return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}
}