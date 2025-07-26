#pragma once

#include "SceneObject.h"

namespace A3
{
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
		static const float camSpeed = 1.0f;
		static const Vec3 upVector(0.0f, 1.0f, 0.0f);

		if (key == GLFW_KEY_W) position += (front * camSpeed);
		if (key == GLFW_KEY_A) position -= (normalize(cross(front, upVector)) * camSpeed);
		if (key == GLFW_KEY_S) position -= (front * camSpeed);
		if (key == GLFW_KEY_D) position += (normalize(cross(front, upVector)) * camSpeed);
		if (key == GLFW_KEY_Q) position -= (upVector * camSpeed);
		if (key == GLFW_KEY_E) position += (upVector * camSpeed);

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

private:
	float fov = 60.f;
	float exposure = 0.0f;

	Vec3 front = { 0.0f, 0.0f, -1.0f };
};
}