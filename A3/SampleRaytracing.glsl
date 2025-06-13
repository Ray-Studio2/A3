#extension GL_GOOGLE_include_directive : require
#include "Sampler.glsl"

#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define MAX_DEPTH 3
#define SAMPLE_COUNT 20

vec3 toneMapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

struct RayPayload
{
    vec3 rayDirection;
    vec3 radiance;
    vec3 depthColor[MAX_DEPTH];
    uint depth;
};

layout( binding = 2 ) uniform CameraProperties
{
    vec3 cameraPos;
    float yFov_degree;
} g;

#if RAY_GENERATION_SHADER
//=========================
//   RAY GENERATION SHADER
//=========================
layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;
layout( binding = 1, rgba8 ) uniform image2D image;
layout(location = 0) rayPayloadEXT RayPayload gPayload;

vec3 gammaCorrect(vec3 color) {
    return pow(color, vec3(1.0 / 2.2));
}

void main()
{
    const vec3 cameraX = vec3( 1, 0, 0 );
    const vec3 cameraY = vec3( 0, -1, 0 );
    const vec3 cameraZ = vec3( 0, 0, -1 );
    const float aspect_y = tan( radians( g.yFov_degree ) * 0.5 );
    const float aspect_x = aspect_y * float( gl_LaunchSizeEXT.x ) / float( gl_LaunchSizeEXT.y );
    
    const vec2 screenCoord = vec2( gl_LaunchIDEXT.xy ) + vec2( 0.5 );
    const vec2 ndc = screenCoord / vec2( gl_LaunchSizeEXT.xy ) * 2.0 - 1.0;
    vec3 rayDir = ndc.x * aspect_x * cameraX + ndc.y * aspect_y * cameraY + cameraZ;
    
    gPayload.radiance = vec3( 0.0 );
    for(uint i = 0; i < MAX_DEPTH; ++i)
    {
       gPayload.depthColor[i] = vec3(0.0);
    }
    gPayload.depth = 0;
    
    gPayload.rayDirection = rayDir;
    traceRayEXT(
       topLevelAS,                         // topLevel
       gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
       0, 1, 0,                            // sbtRecordOffset, sbtRecordStride, missIndex
       g.cameraPos, 0.0, rayDir, 100.0,    // origin, tmin, direction, tmax
       0 );                                 // payload
    
    vec3 finalColor = gPayload.radiance;

   imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( finalColor, 0.0 ) );
}
#endif

#if CLOSEST_HIT_SHADER
//=========================
//   CLOSEST HIT SHADER
//=========================

struct VertexAttributes
{
   vec4 norm;
   vec4 uv;
};

struct ObjectDesc
{
   uint64_t vertexPositionDeviceAddress;
   uint64_t vertexAttributeDeviceAddress;
   uint64_t indexDeviceAddress;
};

layout(buffer_reference, scalar) buffer PositionBuffer { vec4 p[]; };
layout(buffer_reference, scalar) buffer AttributeBuffer { VertexAttributes a[]; };
layout(buffer_reference, scalar) buffer IndexBuffer { ivec3 i[]; }; // Triangle indices
layout( binding = 3, scalar) buffer ObjectDescBuffer
{
   ObjectDesc desc[];
} gObjectDescs;

layout( shaderRecordEXT ) buffer CustomData
{
   vec3 color;
};

layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;
layout(location = 0) rayPayloadInEXT RayPayload gPayload;
hitAttributeEXT vec2 attribs;

#define LIGHT_INSTANCE_INDEX 6

void main()
{
    ObjectDesc objDesc = gObjectDescs.desc[gl_InstanceCustomIndexEXT];

    IndexBuffer indexBuffer = IndexBuffer(objDesc.indexDeviceAddress);
    ivec3 index = indexBuffer.i[gl_PrimitiveID];

    PositionBuffer positionBuffer = PositionBuffer(objDesc.vertexPositionDeviceAddress);
    vec3 p0 = positionBuffer.p[index.x].xyz;
    vec3 p1 = positionBuffer.p[index.y].xyz;
    vec3 p2 = positionBuffer.p[index.z].xyz;

    float u = attribs.x;
    float v = attribs.y;
    float w = 1.0 - u - v;
    vec3 position = w * p0 + u * p1 + v * p2;

    AttributeBuffer attrBuf = AttributeBuffer(objDesc.vertexAttributeDeviceAddress);
    vec3 n0 = normalize(attrBuf.a[index.x].norm.xyz);
    vec3 n1 = normalize(attrBuf.a[index.y].norm.xyz);
    vec3 n2 = normalize(attrBuf.a[index.z].norm.xyz);
    vec3 normal = normalize(w * n0 + u * n1 + v * n2);

    vec3 worldPos = (gl_ObjectToWorldEXT * vec4(position, 1.0)).xyz;
    vec3 worldNormal = normalize(transpose(inverse(mat3(gl_ObjectToWorldEXT))) * normal);
    
    uint currentDepthIndex = gPayload.depth;
    gPayload.depthColor[currentDepthIndex] = color;

    // 광원에 맞은 경우
    if (gl_InstanceCustomIndexEXT == LIGHT_INSTANCE_INDEX) 
    {
        // 첫 ray가 광원에 맞은 경우
        if(currentDepthIndex == 0)
        {
            gPayload.radiance = vec3(1.0);
            return;
        }

        const float kDepthWeights[] = float[](
            1.00,
            0.60,
            0.30,
            0.10,
            0.05,
            0.04,
            0.02,
            0.01,
            0.005,
            0.002
         );
        // currentDepthIndex -= 1: 광원 mesh 색상은 일단 무시
        currentDepthIndex -= 1;

        float weightSum = 0.0;
        for (uint i = 0; i <= currentDepthIndex; ++i)
            weightSum += kDepthWeights[i];
            
        // 아직은 씬에 맞게 조정 필요.
        const float magicNumber = float(SAMPLE_COUNT) * 0.5;
        // 정규화하여 누적
        for (uint i = 0; i <= currentDepthIndex; ++i)
        {
            float normWeight = kDepthWeights[i] / weightSum;
            gPayload.radiance += gPayload.depthColor[i] * normWeight / magicNumber;
        }
        return;
    }
    
    // Tree Preorder 방식으로 순회 (부모 → 자식)
    if (gPayload.depth + 1 < MAX_DEPTH) 
    {
        uvec2 pixelCoord = gl_LaunchIDEXT.xy;
        uvec2 screenSize = gl_LaunchSizeEXT.xy;
        uint rngState = pixelCoord.y * screenSize.x + pixelCoord.x;
        RayPayload payloadBackup = gPayload;
        gPayload.depth += 1;
        for (int i = 0; i < SAMPLE_COUNT; ++i) 
        {
            vec2 seed = vec2(RandomValue2(rngState), RandomValue(rngState));
            vec3 dir = RandomCosineHemisphere(worldNormal, seed);

            // visit
            traceRayEXT(
                topLevelAS,
                gl_RayFlagsOpaqueEXT, 0xFF,
                0, 1, 0,
                worldPos, 0.001, dir, 100.0,
                0
            );
        }
        vec3 radianceBackup = gPayload.radiance;
        gPayload = payloadBackup;
        gPayload.radiance = radianceBackup;
    }
}

#endif

#if SHADOW_MISS_SHADER
//=========================
//   SHADOW MISS SHADER
//=========================
//layout( location = 1 ) rayPayloadInEXT uint missCount;

void main()
{

}
#endif


#if ENVIRONMENT_MISS_SHADER
//=========================
//   ENVIRONMENT MISS SHADER
//=========================
layout(location = 0) rayPayloadInEXT RayPayload gPayload;
layout(set = 0, binding = 4) uniform sampler2D environmentMap;

void main()
{
    if(gPayload.depth != 0)
    {
        return;
    }

    vec3 dir = normalize(gPayload.rayDirection); // ray direction 넘겨줘야 함
    vec2 uv = vec2(
        atan(dir.z, dir.x) / (2.0 * 3.1415926535) + 0.5,
        acos(clamp(dir.y, -1.0, 1.0)) / 3.1415926535
    );
    vec3 color = texture(environmentMap, uv).rgb;
    gPayload.radiance = toneMapACES(color);
}
#endif