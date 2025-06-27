#pragma once
#include "EngineTypes.h"
#include <string>

namespace A3
{
struct RenderSettings
{
	static constexpr uint32 screenWidth = 1200;
	static constexpr uint32 screenHeight = 800;

	static constexpr uint32 shaderGroupHandleSize = 32;

	static constexpr uint32 maxLightCounts = 16;

	static constexpr const char* sceneFile = "../Assets/bruteforce-local.json";
	//static constexpr const char* sceneFile = "../Assets/nee-local.json";
	//static constexpr const char* sceneFile = "../Assets/bruteforce-env.json";
	//static constexpr const char* sceneFile = "../Assets/nee-env.json";

	// static constexpr const char* envMapDefault = "../Assets/reichstag_1_4k.hdr";
	static constexpr const char* envMapDefault = "../Assets/rogland_sunset_4k.hdr";
	static inline std::string envMapPath = "";
};
}