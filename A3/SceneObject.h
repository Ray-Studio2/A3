#pragma once

#include "Vector.h"
#include "Matrix.h"

namespace A3
{
class SceneObject
{
public:
	SceneObject()
		: position({0.0f, 0.0f, 0.0f}),
		  scale({1.0f, 1.0f, 1.0f}),
		  rotation(Mat3x3::identity),
		  localToWorld( Mat4x4::identity )
	{
		updateLocalToWorld();
	}

	virtual bool canRender() { return false; }

	const Mat4x4& getLocalToWorld() { return localToWorld; }
	const Vec3& getWorldPosition() { return Vec3( localToWorld.m03, localToWorld.m13, localToWorld.m23 ); }

	void setPosition( const Vec3& position )
	{
		this->position = position;
		updateLocalToWorld();
	}

	void setScale(const Vec3& scale)
	{
		this->scale = scale;
		updateLocalToWorld();
	}

	void setRotation(const Mat3x3& rotation)
	{
		this->rotation = rotation;
		updateLocalToWorld();
	}

	void setRotation(const Vec3& axis, float angleRad)
	{
		this->rotation = rotationFromAxisAngle(axis, angleRad);
		updateLocalToWorld();
	}

protected:
	void updateLocalToWorld()
	{
		// rotation x scale
		localToWorld.m00 = rotation.m00 * scale.x;
		localToWorld.m01 = rotation.m01 * scale.x;
		localToWorld.m02 = rotation.m02 * scale.x;
		localToWorld.m03 = position.x;

		localToWorld.m10 = rotation.m10 * scale.y;
		localToWorld.m11 = rotation.m11 * scale.y;
		localToWorld.m12 = rotation.m12 * scale.y;
		localToWorld.m13 = position.y;

		localToWorld.m20 = rotation.m20 * scale.z;
		localToWorld.m21 = rotation.m21 * scale.z;
		localToWorld.m22 = rotation.m22 * scale.z;
		localToWorld.m23 = position.z;

		localToWorld.m30 = 0.0f;
		localToWorld.m31 = 0.0f;
		localToWorld.m32 = 0.0f;
		localToWorld.m33 = 1.0f;
	}

	static Mat3x3 rotationFromAxisAngle(const Vec3& axis, float angleRad)
	{
		Vec3 n = normalize(axis);
		float c = std::cos(angleRad);
		float s = std::sin(angleRad);
		float t = 1.0f - c;

		float x = n.x, y = n.y, z = n.z;

		Mat3x3 r{};
		r.m00 = t * x * x + c;       r.m01 = t * x * y - s * z;   r.m02 = t * x * z + s * y;
		r.m10 = t * x * y + s * z;   r.m11 = t * y * y + c;       r.m12 = t * y * z - s * x;
		r.m20 = t * x * z - s * y;   r.m21 = t * y * z + s * x;   r.m22 = t * z * z + c;
		return r;
	}

	Vec3 position;
	Vec3 scale;
	Mat3x3 rotation;

	Mat4x4 localToWorld;
};
}