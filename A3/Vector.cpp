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

Vec3 normalize(const Vec3& v)
{
	float lenSq = lengthSquared(v);
	if (lenSq == 0.0f)
		return Vec3{ 0.0f, 0.0f, 0.0f };

	float lenInv = 1.0f / std::sqrt(lenSq);
	return Vec3{ v.x * lenInv, v.y * lenInv, v.z * lenInv };
}

}