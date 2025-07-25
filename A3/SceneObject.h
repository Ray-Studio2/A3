#pragma once

#include "Vector.h"
#include "Matrix.h"
#include <string>

namespace A3
{
class SceneObject
{
public:
	SceneObject()
		: position(0.0f),
		  scale(1.0f),
		  rotation(Mat3x3::identity),
		  localToWorld( Mat4x4::identity ),
		  baseColor(0.0f),
		  emittance(0.0f),
		  name("temp")
	{
		updateLocalToWorld();
	}

	virtual bool canRender() { return false; }

	const Mat4x4& getLocalToWorld() { return localToWorld; }
	const Vec3& getLocalPosition() { return position; }
	const Vec3& getWorldPosition() { return Vec3( localToWorld.m03, localToWorld.m13, localToWorld.m23 ); }
	const Vec3& getLocalScale() { return scale; }
	const Vec3& getWorldScale() {
		return Vec3(
			length(Vec3(localToWorld.m00, localToWorld.m01, localToWorld.m02)),
			length(Vec3(localToWorld.m10, localToWorld.m11, localToWorld.m12)),
			length(Vec3(localToWorld.m20, localToWorld.m21, localToWorld.m22))
		);
	}
	const Vec3& getBaseColor() { return baseColor; }
	const float getMetallic() const { return metallic; }
	const float getRoughness() const { return roughness; }
	const float getEmittance() const { return emittance; }
	std::string_view getName() const { return name; }
	std::string getMaterialName() const { return materialName; }	///////////////// Juhwan

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

	void setRotation(const Vec3& angleRad)
	{
		float cx = std::cos(angleRad.x); float sx = std::sin(angleRad.x); // pitch
		float cy = std::cos(angleRad.y); float sy = std::sin(angleRad.y); // yaw
		float cz = std::cos(angleRad.z); float sz = std::sin(angleRad.z); // roll

		Mat3x3 rotX{
			1,  0,  0,
			0, cx, -sx,
			0, sx,  cx
		};

		Mat3x3 rotY{
			 cy, 0, sy,
			  0, 1, 0,
			-sy, 0, cy
		};

		Mat3x3 rotZ{
			cz, -sz, 0,
			sz,  cz, 0,
			 0,   0, 1
		};

		Mat3x3 rot = rotZ * rotY * rotX;
		rotation = rot;
		updateLocalToWorld();
	}

	void setBaseColor(const Vec3& baseColor) { this->baseColor = baseColor; }
	void setMetallic(const float& metallic) { this->metallic = metallic; }
	void setRoughness(const float& roughness) { this->roughness = roughness; }
	void setEmittance(float emittance) { this->emittance = emittance; }
	void setName(std::string_view name) { this->name = name; }
	void setMaterialName(std::string materialName) { this->materialName = materialName; }	///////////////// Juhwan

	bool isLight()
	{
		if (emittance != Vec3(0.0))
			return true;
		return false;
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

	Vec3 position;
	Vec3 scale;
	Mat3x3 rotation;

	Mat4x4 localToWorld;

	Vec3 baseColor;
	float metallic;
	float roughness;
	// for light objects
	float emittance;

	std::string name;
	std::string materialName;	///////////////// Juhwan
};
}