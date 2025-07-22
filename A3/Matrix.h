#pragma once

namespace A3
{
struct Mat3x3
{
	float m00, m01, m02;
	float m10, m11, m12;
	float m20, m21, m22;

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
	};

	float* operator[](const int row)
	{
		return _m[row];
	}

	static Mat4x4 identity;
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
}
