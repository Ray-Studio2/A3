#pragma once

#include "Vector.h"
#include "Matrix.h"

namespace A3
{
class SceneObject
{
public:
	virtual ~SceneObject() = default;

	SceneObject()
		: localToWorld( Mat3x4::identity )
	{}

	virtual bool canRender() { return false; }

	const Mat3x4& getLocalToWorld() { return localToWorld; }
	const Vec3& getPosition() { return Vec3( localToWorld.m03, localToWorld.m13, localToWorld.m23 ); }
	void setPosition( const Vec3& position )
	{
		localToWorld.m03 = position.x;
		localToWorld.m13 = position.y;
		localToWorld.m23 = position.z;
	}
	void setScale(const Vec3& scale)
	{
		localToWorld.m00 *= scale.x;
		localToWorld.m01 *= scale.x;
		localToWorld.m02 *= scale.x;

		localToWorld.m10 *= scale.y;
		localToWorld.m11 *= scale.y;
		localToWorld.m12 *= scale.y;

		localToWorld.m20 *= scale.z;
		localToWorld.m21 *= scale.z;
		localToWorld.m22 *= scale.z;
	}

protected:
	Mat3x4 localToWorld;
};
}