#pragma once

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
		: Vec2( 0, 0 ), z( 0 ) 
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
}