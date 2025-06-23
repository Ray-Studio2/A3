layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;
layout( binding = 1, rgba8 ) uniform image2D image;
layout( binding = 2 ) uniform CameraProperties
{
    vec3 cameraPos;
    float yFov_degree;
    uint frameCount;
} g;

layout( binding = 3, scalar) buffer ObjectDescBuffer
{
   ObjectDesc desc[];
} gObjectDescs;

layout(binding = 4, std430) readonly buffer LightBuffer
{
    uint lightCount;
    uint pad1;
    uint pad2;
    uint pad3;
    LightData lights[];
} lightBuffer;

layout( binding = 5, rgba32f ) uniform image2D accumulationImage;
layout( binding = 6 ) uniform sampler2D environmentMap;
layout( binding = 7 ) uniform imguiParam {
	uint maxDepth;
	uint numSamples;
    uint padding0;
    uint isProgressive;
} ip;