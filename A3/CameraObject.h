#pragma once

#include "SceneObject.h"

namespace A3
{
class CameraObject : public SceneObject
{
public:
	void setFov( const float fov ) { this->fov = fov; }
	float getFov() const { return fov; }

private:
	float fov = 60.f;
};
}