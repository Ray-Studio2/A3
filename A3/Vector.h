#pragma once
#include <cmath>

namespace A3
{
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
};

struct Vec4 : public Vec3
{
	union
	{
		float w;
		float a;
	};

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

inline float lengthSquared(const Vec3& v)
{
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

inline float length(const Vec3& v)
{
	return std::sqrt(lengthSquared(v));
}

inline Vec3 normalize(const Vec3& v) 
{
	float lenSq = lengthSquared(v);
	if (lenSq == 0.0f)
		return Vec3{ 0.0f, 0.0f, 0.0f };

	float lenInv = 1.0f / std::sqrt(lenSq);
	return Vec3{ v.x * lenInv, v.y * lenInv, v.z * lenInv };
}
}
