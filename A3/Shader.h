#pragma once

const char* raygen_src = R"(
#version 460
#extension GL_EXT_ray_tracing : enable

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, rgba8) uniform image2D image;
layout(binding = 2) uniform CameraProperties 
{
    vec3 cameraPos;
    float yFov_degree;
} g;

layout(location = 0) rayPayloadEXT vec3 hitValue;

void main() 
{
    const vec3 cameraX = vec3(1, 0, 0);
    const vec3 cameraY = vec3(0, -1, 0);
    const vec3 cameraZ = vec3(0, 0, -1);
    const float aspect_y = tan(radians(g.yFov_degree) * 0.5);
    const float aspect_x = aspect_y * float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);

    const vec2 screenCoord = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 ndc = screenCoord/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;
    vec3 rayDir = ndc.x*aspect_x*cameraX + ndc.y*aspect_y*cameraY + cameraZ;

    hitValue = vec3(0.0);

    traceRayEXT(
        topLevelAS,                         // topLevel
        gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
        0, 1, 0,                            // sbtRecordOffset, sbtRecordStride, missIndex
        g.cameraPos, 0.0, rayDir, 100.0,    // origin, tmin, direction, tmax
        0);                                 // payload

    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(hitValue, 0.0));
})";

const char* miss_src = R"(
#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    hitValue = vec3(0.0, 0.0, 0.2);
})";

const char* chit_src = R"(
#version 460
#extension GL_EXT_ray_tracing : enable

struct VertexAttributes {
    vec4 norm;
    vec4 uv;
};

layout(std430, binding = 3) buffer VertexPosition {
    vec4 vPosBuffer[];
};

layout(std430, binding = 4) buffer VertexAttribute {
    VertexAttributes vAttribBuffer[];
};

layout(std430, binding = 5) buffer Indices {
    uint idxBuffer[];
};

layout(shaderRecordEXT) buffer CustomData
{
    vec3 color;
};

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT uint missCount;
hitAttributeEXT vec2 attribs;

float RandomValue(inout uint state) {
    state *= (state + 195439) * (state + 124395) * (state + 845921);
    return state / 4294967295.0; // 2^31 - 1 (uint 최댓값으로 나눔) -> 0~1 사이의 실수
}

float RandomValueNormalDistribution(inout uint state) {
    float theta = 2 * 3.1415926 * RandomValue(state);
    float rho = sqrt(-2 * log(RandomValue(state)));
    return rho * cos(theta);
}

vec3 RandomDirection(inout uint state) {
    float x = RandomValueNormalDistribution(state);
    float y = RandomValueNormalDistribution(state);
    float z = RandomValueNormalDistribution(state);
    return normalize(vec3(x, y, z));
}

vec3 RandomHemisphereDirection(vec3 normal, inout uint state) {
    vec3 dir = RandomDirection(state);
    return dir * sign(dot(normal, dir));
}

void main()
{
    missCount = 0;

    uvec3 indices = uvec3(idxBuffer[gl_PrimitiveID * 3 + 0], idxBuffer[gl_PrimitiveID * 3 + 1], idxBuffer[gl_PrimitiveID * 3 + 2]);

    vec3 v0 = vPosBuffer[indices.x].xyz;
    vec3 v1 = vPosBuffer[indices.y].xyz;
    vec3 v2 = vPosBuffer[indices.z].xyz;

    VertexAttributes a0 = vAttribBuffer[indices.x];
    VertexAttributes a1 = vAttribBuffer[indices.y];
    VertexAttributes a2 = vAttribBuffer[indices.z];
    
    const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

    vec3 localPos = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    vec3 worldPos = (gl_ObjectToWorldEXT * vec4(localPos, 1.0)).xyz;

    vec3 localNorm = normalize(a0.norm.xyz * barycentrics.x + a1.norm.xyz * barycentrics.y + a2.norm.xyz * barycentrics.z);
    mat3 normalMatrix = transpose(inverse(mat3(gl_ObjectToWorldEXT)));
    vec3 worldNorm = normalize(normalMatrix * localNorm);

    uvec2 pixelCoord = gl_LaunchIDEXT.xy;
    uvec2 screenSize = gl_LaunchSizeEXT.xy;
    uint rngState = pixelCoord.y * screenSize.x + pixelCoord.x; // 1-D array에서의 pixel 위치

    const uint numShadowRays = 100;
    for (uint i=0; i < numShadowRays; ++i) {
        vec3 rayDir = RandomHemisphereDirection(worldNorm, rngState);
        vec3 newPos = worldPos + 0.01 * rayDir;

        traceRayEXT(
            topLevelAS,                         // topLevel
            gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xff,         // rayFlags, cullMask
            0, 1, 1,                            // sbtRecordOffset, sbtRecordStride, missIndex
            newPos, 0.01, rayDir, 100.0,  // origin, tmin, direction, tmax
            1);                                 // payload
    }

    hitValue = color * (float(missCount) / numShadowRays);
})";

const char* shadow_miss_src = R"(
#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 1) rayPayloadInEXT uint missCount;

void main()
{
    missCount++;
}
)";
