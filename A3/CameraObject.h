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

private:
	float fov = 60.f;
	float exposure = 0.0f;
};
}