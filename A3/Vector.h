#pragma once
#include <cmath>

namespace A3
{

static bool floatEqual(float a, float b, float eps = 1e-6f) {
	return std::fabs(a - b) < eps;
}

struct Vec2
{
	union
	{
		struct
		{
			float x;
			float y;
		};
		struct
		{
			float u;
			float v;
		};
	};

	Vec2()
		: Vec2(0.0f, 0.0f)
	{}

	Vec2( float num )
		: Vec2(num, num)
	{}

	Vec2( float inX, float inY )
		: x( inX ), y( inY )
	{}

	bool operator==(const Vec2& other) const {
		return floatEqual(x, other.x) && floatEqual(y, other.y);
	}
};

struct Vec3 : public Vec2
{
	union
	{
		float z;
		float b;
	};

	Vec3()
		: Vec2{}, z{0.0f}
	{}

	Vec3( float num )
		: Vec2{num}, z{ num }
	{}

	Vec3( float inX, float inY, float inZ )
		: Vec2( inX, inY ), z( inZ )
	{}

	bool operator==(const Vec3& other) const {
		return Vec2::operator==(other) && floatEqual(z, other.z);
	}
};

struct Vec4 : public Vec3
{
	union
	{
		float w;
		float a;
	};

	Vec4()
		: Vec3{}, w{ 0.0f }
	{}

	Vec4(float num)
		: Vec3{ num }, w{ num }
	{}

	Vec4( float inX, float inY, float inZ, float inW )
		: Vec3( inX, inY, inZ ), w( inW )
	{}
};

struct IVec2
{
	int x;
	int y;
};

struct IVec3
{
	int x;
	int y;
	int z;
};

struct IVec4
{
	int x;
	int y;
	int z;
	int w;
};

inline Vec3 operator-(const Vec3& lhs, const Vec3& rhs)
{
	return {
		lhs.x - rhs.x,
		lhs.y - rhs.y,
		lhs.z - rhs.z,
	};
}
inline Vec3& operator-=(Vec3& lhs, const Vec3& rhs)
{
	lhs = lhs - rhs;
	return lhs;
}
inline Vec3 operator+(const Vec3& lhs, const Vec3& rhs)
{
	return {
		lhs.x + rhs.x,
		lhs.y + rhs.y,
		lhs.z + rhs.z,
	};
}
inline Vec3& operator+=(Vec3& lhs, const Vec3& rhs)
{
	lhs = lhs + rhs;
	return lhs;
}
inline Vec3 operator*(const Vec3& lhs, float rhs)
{
	return {
		lhs.x * rhs,
		lhs.y * rhs,
		lhs.z * rhs,
	};
}

float lengthSquared(const Vec3& v);
float length(const Vec3& v);
Vec3 normalize(const Vec3& v);
Vec3 cross(const Vec3& lhs, const Vec3& rhs);
}
