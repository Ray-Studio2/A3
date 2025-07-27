#pragma once

#include "Vector.h"

namespace A3
{
struct Mat3x3
{
	union
	{
		struct
		{
			float m00, m01, m02;
			float m10, m11, m12;
			float m20, m21, m22;
		};

		float _m[3][3];
	};

	float* operator[](const int row)
	{
		return _m[row];
	}

	const float* operator[](const int row) const
	{
		return _m[row];
	}

	static Mat3x3 identity;
};

struct Mat4x4
{
	union
	{
		struct
		{
			float m00, m01, m02, m03;
			float m10, m11, m12, m13;
			float m20, m21, m22, m23;
			float m30, m31, m32, m33;
		};

		float _m[4][4];
		float _arr[16];
	};

	float* operator[](const int row)
	{
		return _m[row];
	}

	static Mat4x4 identity;

	Mat4x4& transpose()
	{
		std::swap(m10, m01);
		std::swap(m20, m02);
		std::swap(m21, m12);
		std::swap(m30, m03);
		std::swap(m31, m13);
		std::swap(m32, m23);

		return *this;
	}
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

inline Mat3x4 toMat3x4(const Mat4x4& m)
{
	Mat3x4 t{};

	t.m00 = m.m00;  t.m01 = m.m01;  t.m02 = m.m02;  t.m03 = m.m03;
	t.m10 = m.m10;  t.m11 = m.m11;  t.m12 = m.m12;  t.m13 = m.m13;
	t.m20 = m.m20;  t.m21 = m.m21;  t.m22 = m.m22;  t.m23 = m.m23;

	return t;
}

inline Mat3x3 mul(const Mat3x3& A, const Mat3x3& B)
{
	Mat3x3 R;

	R.m00 = A.m00 * B.m00 + A.m01 * B.m10 + A.m02 * B.m20;
	R.m01 = A.m00 * B.m01 + A.m01 * B.m11 + A.m02 * B.m21;
	R.m02 = A.m00 * B.m02 + A.m01 * B.m12 + A.m02 * B.m22;

	R.m10 = A.m10 * B.m00 + A.m11 * B.m10 + A.m12 * B.m20;
	R.m11 = A.m10 * B.m01 + A.m11 * B.m11 + A.m12 * B.m21;
	R.m12 = A.m10 * B.m02 + A.m11 * B.m12 + A.m12 * B.m22;

	R.m20 = A.m20 * B.m00 + A.m21 * B.m10 + A.m22 * B.m20;
	R.m21 = A.m20 * B.m01 + A.m21 * B.m11 + A.m22 * B.m21;
	R.m22 = A.m20 * B.m02 + A.m21 * B.m12 + A.m22 * B.m22;

	return R;
}

inline Mat3x3 operator*(const Mat3x3& lhs, const Mat3x3& rhs)
{
	return mul(lhs, rhs);
}

inline Mat3x3& operator*=(Mat3x3& lhs, const Mat3x3& rhs)
{
	lhs = lhs * rhs;
	return lhs;
}
inline Vec3 operator*(const Mat3x3& rhs, const Vec3& vec)
{
	Vec3 t;
	t.x = rhs.m00 * vec.x + rhs.m01 * vec.y + rhs.m02 * vec.z;
	t.y = rhs.m10 * vec.x + rhs.m11 * vec.y + rhs.m12 * vec.z;
	t.z = rhs.m20 * vec.x + rhs.m21 * vec.y + rhs.m22 * vec.z;

	return t;
}

inline Mat4x4 mul(const Mat4x4& A, const Mat4x4& B)
{
	Mat4x4 R;

	R.m00 = A.m00 * B.m00 + A.m01 * B.m10 + A.m02 * B.m20 + A.m03 * B.m30;
	R.m01 = A.m00 * B.m01 + A.m01 * B.m11 + A.m02 * B.m21 + A.m03 * B.m31;
	R.m02 = A.m00 * B.m02 + A.m01 * B.m12 + A.m02 * B.m22 + A.m03 * B.m32;
	R.m03 = A.m00 * B.m03 + A.m01 * B.m13 + A.m02 * B.m23 + A.m03 * B.m33;

	R.m10 = A.m10 * B.m00 + A.m11 * B.m10 + A.m12 * B.m20 + A.m13 * B.m30;
	R.m11 = A.m10 * B.m01 + A.m11 * B.m11 + A.m12 * B.m21 + A.m13 * B.m31;
	R.m12 = A.m10 * B.m02 + A.m11 * B.m12 + A.m12 * B.m22 + A.m13 * B.m32;
	R.m13 = A.m10 * B.m03 + A.m11 * B.m13 + A.m12 * B.m23 + A.m13 * B.m33;

	R.m20 = A.m20 * B.m00 + A.m21 * B.m10 + A.m22 * B.m20 + A.m23 * B.m30;
	R.m21 = A.m20 * B.m01 + A.m21 * B.m11 + A.m22 * B.m21 + A.m23 * B.m31;
	R.m22 = A.m20 * B.m02 + A.m21 * B.m12 + A.m22 * B.m22 + A.m23 * B.m32;
	R.m23 = A.m20 * B.m03 + A.m21 * B.m13 + A.m22 * B.m23 + A.m23 * B.m33;

	R.m30 = A.m30 * B.m00 + A.m31 * B.m10 + A.m32 * B.m20 + A.m33 * B.m30;
	R.m31 = A.m30 * B.m01 + A.m31 * B.m11 + A.m32 * B.m21 + A.m33 * B.m31;
	R.m32 = A.m30 * B.m02 + A.m31 * B.m12 + A.m32 * B.m22 + A.m33 * B.m32;
	R.m33 = A.m30 * B.m03 + A.m31 * B.m13 + A.m32 * B.m23 + A.m33 * B.m33;

	return R;
}

inline Mat4x4 operator+(const Mat4x4& lhs, const Mat4x4& rhs)
{
	Mat4x4 temp;
	temp._m[0][0] = lhs._m[0][0] + rhs._m[0][0];
	temp._m[0][1] = lhs._m[0][1] + rhs._m[0][1];
	temp._m[0][2] = lhs._m[0][2] + rhs._m[0][2];
	temp._m[0][3] = lhs._m[0][3] + rhs._m[0][3];

	temp._m[1][0] = lhs._m[1][0] + rhs._m[1][0];
	temp._m[1][1] = lhs._m[1][1] + rhs._m[1][1];
	temp._m[1][2] = lhs._m[1][2] + rhs._m[1][2];
	temp._m[1][3] = lhs._m[1][3] + rhs._m[1][3];

	temp._m[2][0] = lhs._m[2][0] + rhs._m[2][0];
	temp._m[2][1] = lhs._m[2][1] + rhs._m[2][1];
	temp._m[2][2] = lhs._m[2][2] + rhs._m[2][2];
	temp._m[2][3] = lhs._m[2][3] + rhs._m[2][3];

	temp._m[3][0] = lhs._m[3][0] + rhs._m[3][0];
	temp._m[3][1] = lhs._m[3][1] + rhs._m[3][1];
	temp._m[3][2] = lhs._m[3][2] + rhs._m[3][2];
	temp._m[3][3] = lhs._m[3][3] + rhs._m[3][3];

	return temp;
}

inline Mat4x4& operator+=(Mat4x4& lhs, const Mat4x4& rhs)
{
	lhs = lhs + rhs;
	return lhs;
}

inline Mat4x4 operator*(const Mat4x4& lhs, const float rhs)
{
	Mat4x4 temp;
	temp._m[0][0] = lhs._m[0][0] * rhs;
	temp._m[0][1] = lhs._m[0][1] * rhs;
	temp._m[0][2] = lhs._m[0][2] * rhs;
	temp._m[0][3] = lhs._m[0][3] * rhs;

	temp._m[1][0] = lhs._m[1][0] * rhs;
	temp._m[1][1] = lhs._m[1][1] * rhs;
	temp._m[1][2] = lhs._m[1][2] * rhs;
	temp._m[1][3] = lhs._m[1][3] * rhs;

	temp._m[2][0] = lhs._m[2][0] * rhs;
	temp._m[2][1] = lhs._m[2][1] * rhs;
	temp._m[2][2] = lhs._m[2][2] * rhs;
	temp._m[2][3] = lhs._m[2][3] * rhs;

	temp._m[3][0] = lhs._m[3][0] * rhs;
	temp._m[3][1] = lhs._m[3][1] * rhs;
	temp._m[3][2] = lhs._m[3][2] * rhs;
	temp._m[3][3] = lhs._m[3][3] * rhs;

	return temp;
}

inline Vec4 operator*(const Mat4x4& m, const Vec4& v)
{
	return {
		m.m00 * v.x + m.m01 * v.y + m.m02 * v.z + m.m03 * v.w,
		m.m10 * v.x + m.m11 * v.y + m.m12 * v.z + m.m13 * v.w,
		m.m20 * v.x + m.m21 * v.y + m.m22 * v.z + m.m23 * v.w,
		m.m30 * v.x + m.m31 * v.y + m.m32 * v.z + m.m33 * v.w
	};
}
}
