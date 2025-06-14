#extension GL_GOOGLE_include_directive : require
#include "Sampler.glsl"

#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define MAX_DEPTH 3
#define SAMPLES_PER_PIXEL 64

vec3 toneMapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 toneMapReinhard(vec3 color) {
    return color / (1.0 + color);
}

struct RayPayload
{
    vec3 rayDirection;
    vec3 radiance;
    vec3 throughput;
    uint depth;
    uint rngState;
};

layout( binding = 2 ) uniform CameraProperties
{
    vec3 cameraPos;
    float yFov_degree;
    uint frameCount;
} g;

// Improved random number generator
uint pcg_hash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random(inout uint rngState) {
    rngState = pcg_hash(rngState);
    return float(rngState) / float(0xffffffffu);
}

#if RAY_GENERATION_SHADER
//=========================
//   RAY GENERATION SHADER
//=========================
layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;
layout( binding = 1, rgba8 ) uniform image2D image;
layout(location = 0) rayPayloadEXT RayPayload gPayload;

void main()
{
    const vec3 cameraX = vec3( 1, 0, 0 );
    const vec3 cameraY = vec3( 0, -1, 0 );
    const vec3 cameraZ = vec3( 0, 0, -1 );
    const float aspect_y = tan( radians( g.yFov_degree ) * 0.5 );
    const float aspect_x = aspect_y * float( gl_LaunchSizeEXT.x ) / float( gl_LaunchSizeEXT.y );
    
    uint pixelIndex = gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;
    uint baseSeed = pcg_hash(pixelIndex + g.frameCount * 1664525u);
    
    vec3 totalRadiance = vec3(0.0);
    
    for (uint sampleIndex = 0; sampleIndex < SAMPLES_PER_PIXEL; ++sampleIndex)
    {
        uint rngState = pcg_hash(baseSeed + sampleIndex * 1013904223u);
        
        // Anti-aliasing jitter for each sample
        float r1 = random(rngState);
        float r2 = random(rngState);
        
        const vec2 screenCoord = vec2( gl_LaunchIDEXT.xy ) + vec2( r1, r2 );
        const vec2 ndc = screenCoord / vec2( gl_LaunchSizeEXT.xy ) * 2.0 - 1.0;
        vec3 rayDir = ndc.x * aspect_x * cameraX + ndc.y * aspect_y * cameraY + cameraZ;
        
        // Initialize payload for this sample
        gPayload.radiance = vec3( 0.0 );
        gPayload.throughput = vec3( 1.0 );
        gPayload.depth = 0;
        gPayload.rngState = rngState;
        gPayload.rayDirection = rayDir;
        
        // Trace ray for this sample
        traceRayEXT(
           topLevelAS,
           gl_RayFlagsOpaqueEXT, 0xff,
           0, 1, 0,
           g.cameraPos, 0.0, rayDir, 100.0,
           0 );
        
        // Accumulate radiance from this sample
        totalRadiance += gPayload.radiance;
    }
    
    // Average all samples
    vec3 finalColor = totalRadiance / float(SAMPLES_PER_PIXEL);
    
    // Tone mapping and gamma correction
    float exposure = 2.0;
    finalColor *= exposure;
    finalColor = toneMapACES(finalColor);
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( finalColor, 1.0 ) );
}
#endif

#if CLOSEST_HIT_SHADER
//=========================
//   CLOSEST HIT SHADER (BRUTE FORCE)
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
layout(buffer_reference, scalar) buffer IndexBuffer { ivec3 i[]; };
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
    
    // Light emission check
    if (gl_InstanceCustomIndexEXT == LIGHT_INSTANCE_INDEX) 
    {
        vec3 lightEmission = vec3(15.0);
        float distance = gl_HitTEXT;
        float attenuation = 1.0 / (1.0 + 0.05 * distance + 0.005 * distance * distance);
        
        gPayload.radiance = gPayload.throughput * lightEmission * attenuation;
        return;
    }
    
    if (gPayload.depth >= MAX_DEPTH) {
        return;
    }
    
    // Update throughput with surface color
    gPayload.throughput *= color;
    
    // Sample next direction
    float r1 = random(gPayload.rngState);
    float r2 = random(gPayload.rngState);
    vec2 seed = vec2(r1, r2);
    vec3 dir = RandomCosineHemisphere(worldNormal, seed);
    
    gPayload.depth += 1;
    traceRayEXT(
        topLevelAS,
        gl_RayFlagsOpaqueEXT, 0xFF,
        0, 1, 0,
        worldPos, 0.001, dir, 100.0,
        0
    );
}

#endif

#if SHADOW_MISS_SHADER
//=========================
//   SHADOW MISS SHADER
//=========================
void main()
{
    // Empty
}
#endif

#if ENVIRONMENT_MISS_SHADER
//=========================
//   ENVIRONMENT MISS SHADER (BRUTE FORCE)
//=========================
layout(location = 0) rayPayloadInEXT RayPayload gPayload;
layout(set = 0, binding = 4) uniform sampler2D environmentMap;

void main()
{
    vec3 dir = normalize(gPayload.rayDirection);
    vec2 uv = vec2(
        atan(dir.z, dir.x) / (2.0 * 3.1415926535) + 0.5,
        acos(clamp(dir.y, -1.0, 1.0)) / 3.1415926535
    );
    vec3 color = texture(environmentMap, uv).rgb;
    
    // Environment contribution scaled by depth
    float envScale = (gPayload.depth == 0) ? 1.0 : 0.3;
    gPayload.radiance += gPayload.throughput * color * envScale;
}
#endif