#pragma once

#include "SceneObject.h"

namespace A3
{

inline Vec3 operator*(const Mat3x3& m, const Vec3& v)
{
	return Vec3{
		m.m00 * v.x + m.m01 * v.y + m.m02 * v.z,
		m.m10 * v.x + m.m11 * v.y + m.m12 * v.z,
		m.m20 * v.x + m.m21 * v.y + m.m22 * v.z
	};
}

class CameraObject : public SceneObject
{
public:
	void setFov( const float fov ) { this->fov = fov; }
	float getFov() const { return fov; }

	void setExposure(const float exposure) { this->exposure = exposure; }
	float getExposure() const { return exposure; }

	void setFront(const Vec3& front) { this->front = front; }
	Vec3 getFront() { return front; }

	void move(int key) {
		static const float camSpeed = 0.05f;
		static const Vec3 upVector(0.0f, 1.0f, 0.0f);

		if (key == GLFW_KEY_W) position += (front * camSpeed);
		if (key == GLFW_KEY_A) position -= (normalize(cross(front, upVector)) * camSpeed);
		if (key == GLFW_KEY_S) position -= (front * camSpeed);
		if (key == GLFW_KEY_D) position += (normalize(cross(front, upVector)) * camSpeed);

		updateLocalToWorld();
	}
	void rotateView(float dx, float dy) {
		// radian of 89.0 degree
		static const float PITCH_LIMIT = 1.553343f;

		// initial pitch and yaw based on the first camera front direction
		static float pitch = asinf(front.y);
		static float yaw   = atan2f(front.z, front.x);

		pitch += dy;
    	yaw += dx;

		if (pitch > PITCH_LIMIT)  pitch = PITCH_LIMIT;
    	if (pitch < -PITCH_LIMIT) pitch = -PITCH_LIMIT;

		front = normalize(Vec3(
			cosf(yaw) * cosf(pitch),
			sinf(pitch),
			sinf(yaw) * cosf(pitch))
		);
	}

	void initializeFrontFromLocalToWorld()
	{
		// 기본 front 방향 (-Z)
		const Vec3 defaultFront(0.0f, 0.0f, -1.0f);

		// 회전 부분만 추출 (upper-left 3x3)
		Mat3x3 rot{
			localToWorld.m00, localToWorld.m01, localToWorld.m02,
			localToWorld.m10, localToWorld.m11, localToWorld.m12,
			localToWorld.m20, localToWorld.m21, localToWorld.m22
		};

		front = normalize(rot * defaultFront);
	}

private:
	float fov = 60.f;
	float exposure = 0.0f;

	Vec3 front = { 0.0f, 0.0f, 0.0f };
};
}