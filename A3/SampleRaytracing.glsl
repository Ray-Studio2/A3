#extension GL_GOOGLE_include_directive : require
#include "Sampler.glsl"

#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define MAX_DEPTH 3

#define PI 3.1415926535897932384626433832795

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

struct ShadowPayload
{
    bool inShadow;
};

struct LightData
{
    vec3 position;
    float radius;
    vec3 emission;
    float pad0;  // Alignment for std430
};

layout(binding = 4, std430) readonly buffer LightBuffer
{
    uint lightCount;
    uint pad1;
    uint pad2;
    uint pad3;
    LightData lights[];
} lightBuffer;

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

// Sample a random light from the scene
LightData sampleRandomLight(inout uint rngState, out uint lightIndex) {
    if (lightBuffer.lightCount == 0) {
        lightIndex = 0;
        LightData emptyLight;
        emptyLight.position = vec3(0.0);
        emptyLight.radius = 0.0;
        emptyLight.emission = vec3(0.0);
        emptyLight.pad0 = 0.0;
        return emptyLight;
    }
    
    lightIndex = uint(random(rngState) * float(lightBuffer.lightCount));
    lightIndex = min(lightIndex, lightBuffer.lightCount - 1);
    return lightBuffer.lights[lightIndex];
}

// Sample a point on a spherical light
vec3 sampleSphereLight(LightData light, vec3 hitPoint, vec2 u, out float pdf) {
    vec3 toLight = light.position - hitPoint;
    float distToLight = length(toLight);
    
    if (distToLight < light.radius) {
        pdf = 0.0;
        return vec3(0.0);
    }
    
    vec3 lightDir = normalize(toLight);
    
    // Sample sphere uniformly
    float cosTheta = 1.0 - 2.0 * u.x;
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    float phi = 2.0 * PI * u.y;
    
    vec3 localDir = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    
    // Transform to world space
    vec3 up = abs(lightDir.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 tangent = normalize(cross(up, lightDir));
    vec3 bitangent = cross(lightDir, tangent);
    
    vec3 sampleDir = tangent * localDir.x + bitangent * localDir.y + lightDir * localDir.z;
    vec3 lightSample = light.position + light.radius * sampleDir;
    
    // PDF for sampling the light uniformly
    pdf = 1.0 / (4.0 * PI * light.radius * light.radius);
    
    // Account for multiple lights
    pdf /= float(lightBuffer.lightCount);
    
    return lightSample;
}

#if RAY_GENERATION_SHADER
//=========================
//   RAY GENERATION SHADER
//=========================
layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;
layout( binding = 1, rgba8 ) uniform image2D image;
layout( binding = 5, rgba32f ) uniform image2D accumulationImage;
layout(location = 0) rayPayloadEXT RayPayload gPayload;
layout(location = 1) rayPayloadEXT ShadowPayload gShadowPayload;
void main()
{
    const vec3 cameraX = vec3( 1, 0, 0 );
    const vec3 cameraY = vec3( 0, -1, 0 );
    const vec3 cameraZ = vec3( 0, 0, -1 );
    const float aspect_y = tan( radians( g.yFov_degree ) * 0.5 );
    const float aspect_x = aspect_y * float( gl_LaunchSizeEXT.x ) / float( gl_LaunchSizeEXT.y );
    
    // Better random seed generation
    uint pixelIndex = gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;
    uint rngState = pcg_hash(pixelIndex + g.frameCount * 1664525u);
    
    // Anti-aliasing jitter
    float r1 = random(rngState);
    float r2 = random(rngState);
    
    const vec2 screenCoord = vec2( gl_LaunchIDEXT.xy ) + vec2( r1, r2 );
    const vec2 ndc = screenCoord / vec2( gl_LaunchSizeEXT.xy ) * 2.0 - 1.0;
    vec3 rayDir = ndc.x * aspect_x * cameraX + ndc.y * aspect_y * cameraY + cameraZ;
    
    // Initialize payload for path tracing
    gPayload.radiance = vec3( 0.0 );
    gPayload.throughput = vec3( 1.0 );
    gPayload.depth = 0;
    gPayload.rngState = rngState;
    gPayload.rayDirection = rayDir;
    
    // Single ray per pixel per frame
    traceRayEXT(
       topLevelAS,
       gl_RayFlagsOpaqueEXT, 0xff,
       0, 1, 0,
       g.cameraPos, 0.0, rayDir, 100.0,
       0 );
    
    vec3 currentSample = gPayload.radiance;
    
    // Progressive accumulation
    vec3 previousAccumulation = vec3(0.0);
    if (g.frameCount > 1) {
        previousAccumulation = imageLoad(accumulationImage, ivec2(gl_LaunchIDEXT.xy)).rgb;
    }
    
    // Proper incremental average
    vec3 accumulated = (previousAccumulation * float(g.frameCount - 1) + currentSample) / float(g.frameCount);
    
    // Store in accumulation buffer
    imageStore(accumulationImage, ivec2(gl_LaunchIDEXT.xy), vec4(accumulated, 1.0));
    
    // Tone mapping and gamma correction
    vec3 finalColor = accumulated;
    float exposure = 1.0;
    finalColor *= exposure;
    finalColor = toneMapACES(finalColor);
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

   imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( finalColor, 1.0 ) );
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
layout(location = 1) rayPayloadEXT ShadowPayload gShadowPayload;
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
        vec3 lightEmission = vec3(5.0);
        float distance = gl_HitTEXT;
        float attenuation = 1.0 / (1.0 + 0.1 * distance + 0.01 * distance * distance);
        
        gPayload.radiance = gPayload.throughput * lightEmission * attenuation;
        return;
    }
    
    // Russian roulette for path termination
    if (gPayload.depth >= MAX_DEPTH) {
        return;
    }
    
    vec3 directLighting = vec3(0.0);
    
    // Only do direct lighting if we have lights
    if (lightBuffer.lightCount > 0) {
        // Sample a random light for NEE
        uint lightIndex;
        LightData light = sampleRandomLight(gPayload.rngState, lightIndex);
        
        float r1 = random(gPayload.rngState);
        float r2 = random(gPayload.rngState);
        float lightPdf;
        vec3 lightSample = sampleSphereLight(light, worldPos, vec2(r1, r2), lightPdf);
    
    if (lightPdf > 0.0) {
        vec3 toLight = lightSample - worldPos;
        float lightDist = length(toLight);
        vec3 lightDir = toLight / lightDist;
        
        float cosTheta = max(0.0, dot(worldNormal, lightDir));
        
        if (cosTheta > 0.0) {
            // Cast shadow ray
            gShadowPayload.inShadow = false;
            traceRayEXT(
                topLevelAS,
                gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT,
                0xFF,
                0, 1, 1,  // Use miss shader index 1 for shadows
                worldPos, 0.001, lightDir, lightDist - 0.001,
                1  // Use shadow payload
            );
            
            if (!gShadowPayload.inShadow) {
                // Calculate direct lighting contribution
                vec3 brdf = color / PI;  // Lambertian BRDF
                float geometryTerm = cosTheta / (lightDist * lightDist);
                directLighting = gPayload.throughput * brdf * light.emission * geometryTerm / lightPdf;
            }
        }
    }
    } // End of light count check
    
    gPayload.throughput *= color;
    
    // Russian roulette
    if (gPayload.depth > 0) {
        float maxComponent = max(gPayload.throughput.r, max(gPayload.throughput.g, gPayload.throughput.b));
        float rr_prob = min(maxComponent, 0.95);
        if (random(gPayload.rngState) > rr_prob) {
            gPayload.radiance += directLighting;
            return;
        }
        gPayload.throughput /= rr_prob;
    }
    
    // Sample indirect direction
    float r3 = random(gPayload.rngState);
    float r4 = random(gPayload.rngState);
    vec2 seed = vec2(r3, r4);
    vec3 indirectDir = RandomCosineHemisphere(worldNormal, seed);
    
    // Continue path for indirect lighting
    gPayload.depth += 1;
    traceRayEXT(
        topLevelAS,
        gl_RayFlagsOpaqueEXT, 0xFF,
        0, 1, 0,
        worldPos, 0.001, indirectDir, 100.0,
        0
    );
    
    // Add direct lighting to final result
    gPayload.radiance += directLighting;
}

#endif

#if SHADOW_MISS_SHADER
//=========================
//   SHADOW MISS SHADER
//=========================

layout(location = 1) rayPayloadInEXT ShadowPayload gShadowPayload;
void main()
{
    gShadowPayload.inShadow = false;
}

#endif

#if ENVIRONMENT_MISS_SHADER
//=========================
//   ENVIRONMENT MISS SHADER
//=========================
layout(location = 0) rayPayloadInEXT RayPayload gPayload;
layout(set = 0, binding = 6) uniform sampler2D environmentMap;

void main()
{
    // Only contribute environment lighting on primary rays
    if(gPayload.depth == 0)
    {
        vec3 dir = normalize(gPayload.rayDirection);
        vec2 uv = vec2(
            atan(dir.z, dir.x) / (2.0 * PI) + 0.5,
            acos(clamp(dir.y, -1.0, 1.0)) / PI
        );
        vec3 color = texture(environmentMap, uv).rgb;
        gPayload.radiance = gPayload.throughput * color * 0.5;
    }
}
#endif