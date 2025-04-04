#pragma once

namespace A3
{
struct Mat3x3
{
	float m00, m01, m02;
	float m10, m11, m12;
	float m20, m21, m22;
};

struct Mat4x4
{
	float m00, m01, m02, m03;
	float m10, m11, m12, m13;
	float m20, m21, m22, m23;
	float m30, m31, m32, m33;
};

struct Mat3x4
{
	union
	{
		struct
		{
			float m00, m01, m02, m03;
			float m10, m11, m12, m13;
			float m20, m21, m22, m23;
		};
		float m[ 3 ][ 4 ];
	};

	static Mat3x4 identity;
};
}