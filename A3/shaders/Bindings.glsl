layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;
layout( binding = 1, rgba8 ) uniform image2D image;
layout( binding = 2 ) uniform CameraProperties
{
    vec3 cameraPos;
    vec3 cameraFront;

    float yFov_degree;
    float exposure;
    uint currentFrame;
} g;

layout( binding = 3, scalar) buffer ObjectDescBuffer
{
   ObjectDesc desc[];
} gObjectDescs;

layout(buffer_reference, scalar) buffer PositionBuffer { vec4 p[]; };
layout(buffer_reference, scalar) buffer AttributeBuffer { VertexAttributes a[]; };
layout(buffer_reference, scalar) buffer IndexBuffer { uint i[]; }; 
layout(buffer_reference, scalar) buffer cumulativeTriangleAreaBuffer { float t[]; };
layout(buffer_reference, scalar) buffer MaterialBuffer { MaterialParameter mat; };

layout(binding = 4, std430) readonly buffer LightBuffer
{
    uint lightIndex[16];
    uint lightCount;
    uint pad1;
    uint pad2;
    uint pad3;
    LightData lights[];
} gLightBuffer;

layout( binding = 5, rgba32f ) uniform image2D accumulationImage;
layout( binding = 6 ) uniform sampler2D environmentMap;
layout( binding = 7 ) uniform imguiParam {
	uint maxDepth;
	uint numSamples;
    uint isProgressive;
    float envmapRotDeg;
    uint brdfMode; // 0: basic metallic-roughness, 1: gltf, 2: disney, 3: unreal
} gImguiParam;

layout(binding = 8) uniform sampler2D envImportanceData;
layout( binding = 9 ) uniform sampler2D envHitPdf;

layout(binding = 10) uniform sampler linearSampler;

#define TEXTUREBINDLESS_BINDING_LOCATION 11
layout(binding = TEXTUREBINDLESS_BINDING_LOCATION) uniform texture2D textures[];