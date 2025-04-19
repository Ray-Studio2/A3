#extension GL_EXT_ray_tracing : enable

//////////////////////////////////////////////////////////////////////
//Noise code from https://www.shadertoy.com/view/Nsf3Ws
uint seed = 0u;
void hash(){
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
}
float random(){
    hash();
    return (2.0 * float(seed)/4294967295.0) -1.0;
}
/////////////////////////////////////////////////////////////////////

vec3 rvec3() {
    return vec3(random(), random(), random());
}

#if RAY_GENERATION_SHADER
//=========================
//	RAY GENERATION SHADER
//=========================
layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;
layout( binding = 1, rgba8 ) uniform image2D image;
layout( binding = 2 ) uniform CameraProperties
{
	vec3 cameraPos;
	float yFov_degree;
} g;

layout( location = 0 ) rayPayloadEXT vec3 hitValue;
layout( location = 1 ) rayPayloadEXT uint missCount;

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

	hitValue = vec3(0.0);

	traceRayEXT(
		topLevelAS,                         // topLevel
		gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
		0, 1, 0,                            // sbtRecordOffset, sbtRecordStride, missIndex
		g.cameraPos, 0.0, rayDir, 100.0,    // origin, tmin, direction, tmax
		0 );                                 // payload

	imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( hitValue, 0.0 ) );
}
#endif

#if ENVIRONMENT_MISS_SHADER
//=========================
//	ENVIRONMENT MISS SHADER
//=========================
// layout( location = 0 ) rayPayloadInEXT vec3 hitValue;
layout( location = 1 ) rayPayloadInEXT uint missCount;

void main()
{
    missCount = 1;
}
#endif

#if CLOSEST_HIT_SHADER
//=========================
//	CLOSEST HIT SHADER
//=========================
struct VertexAttributes
{
    vec4 pos;
	vec4 norm;
    vec4 color;
	vec4 uv;
};

layout( binding = 3 ) buffer VertexPosition
{
	vec4 vPosBuffer[];
};

layout( binding = 4 ) buffer VertexAttribute
{
	VertexAttributes vAttribBuffer[];
};

layout( binding = 5 ) buffer Indices
{
	uint idxBuffer[];
};

layout( shaderRecordEXT ) buffer CustomData
{
	vec3 color;
};

layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;

layout( location = 0 ) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadInEXT uint missCount;
hitAttributeEXT vec2 attribs;

#define NUM_AO_SAMPLES 1024
#define AO_RADIUS 100
#define EPSILON 0.001
#define AO_STRENGTH 0.5

vec3 randomHemisphereDirection(vec3 normal) {
    vec3 randomDir;
    while (true) {
        randomDir = rvec3();

        if (dot(randomDir, randomDir) <= 1.0 && dot(normal, randomDir) >= 0.0) {
            break;
        }
    }

    float norm = length(randomDir);
    randomDir.x /= norm;
    randomDir.y /= norm;
    randomDir.z /= norm;

    if (randomDir.z < 0.0) {
        randomDir.z = -randomDir.z;
    }

    return randomDir * sign(dot(normal, randomDir));
}

float RandomValue_HG(inout uint state) {
    state = (1664525u * state + 1013904223u);
    return float(state & 0x00FFFFFFu) / float(0x01000000u);
}
vec3 RandomDirection_HG(vec3 normal, inout uint state)
{
    vec3 dir;
    while (true) {
        dir = vec3(
            RandomValue_HG(state) * 2.0 - 1.0,
            RandomValue_HG(state) * 2.0 - 1.0,
            RandomValue_HG(state) * 2.0 - 1.0
        );
        if (dot(dir,dir) < 1.0 && dot(normal,dir) > 0.0)
            break;
    }
    float norm = sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    dir.x /= norm;
    dir.y /= norm;
    dir.z /= norm;
    if (dir.z < 0.0)
        dir.z *= -1.0;
    return dir;
}

vec3 RandomHemisphereDirection(vec3 normal, inout uint state) {
    vec3 dir = RandomDirection_HG(normal, state);
    return dir * sign(dot(normal, dir));
}

vec3 computeTriangleNormal(vec3 v0, vec3 v1, vec3 v2) {
    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    return normalize(cross(e1, e2));
}

void main()
{
    uint primitiveIndex = gl_PrimitiveID;
    uint i0 = idxBuffer[primitiveIndex * 3 + 0];
    uint i1 = idxBuffer[primitiveIndex * 3 + 1];
    uint i2 = idxBuffer[primitiveIndex * 3 + 2];
    
    vec3 v0 = vPosBuffer[i0].xyz;
    vec3 v1 = vPosBuffer[i1].xyz;
    vec3 v2 = vPosBuffer[i2].xyz;
    
    vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    
    vec3 n0 = vAttribBuffer[i0].norm.xyz;
    vec3 n1 = vAttribBuffer[i1].norm.xyz;
    vec3 n2 = vAttribBuffer[i2].norm.xyz;
    
    bool hasValidNormals = length(n0) > 0.001 && length(n1) > 0.001 && length(n2) > 0.001;
    
    vec3 normal;
    if (hasValidNormals) {
        normal = normalize(
            barycentrics.x * n0 +
            barycentrics.y * n1 +
            barycentrics.z * n2
        );
    } else {
        normal = computeTriangleNormal(v0, v1, v2);
    }
    
    // normal = normalize(normal * mat3(gl_ObjectToWorldEXT));
    
    vec3 hitPosition = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    
    // seed = uint(gl_LaunchIDEXT.x * 1973 + gl_LaunchIDEXT.y * 9277 + gl_PrimitiveID * 26699) ^ 12345;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // https://github.com/Ray-Studio2/build-up-phase/pull/54/commits/de4350f618a4264a3220ef6c6f0847da8d8103e6 //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

    vec3 localPos = (v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z);
    vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 localNorm = normalize(n0 * barycentrics.x + n1 * barycentrics.y + n2 * barycentrics.z);

    uvec2 pixelCoord = gl_LaunchIDEXT.xy;
    uvec2 screenSize = gl_LaunchSizeEXT.xy;
    uint rngState = pixelCoord.y * screenSize.x + pixelCoord.x;
    
    float occlusion = 0.0;
    
    for (int i = 0; i < NUM_AO_SAMPLES; i++) {
        vec3 sampleDir = randomHemisphereDirection(normal);
        // vec3 sampleDir = RandomHemisphereDirection(localNorm, rngState);
        vec3 origin = hitPosition + normal * EPSILON * 3.0;
        
        missCount = 0;
        
        traceRayEXT(
            topLevelAS,
            gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF,
            0, 1, 1,
            hitPosition, EPSILON, sampleDir, AO_RADIUS,
            1
        );
        
        // Accumulate occlusion
        occlusion += float(1 - missCount);
    }
    
    float ao = (occlusion / float(NUM_AO_SAMPLES)); // * AO_STRENGTH
    
    vec3 finalColor = color * ao;
    hitValue = finalColor;
}
#endif

#if SHADOW_MISS_SHADER
//=========================
//	SHADOW MISS SHADER
//=========================
layout( location = 1 ) rayPayloadInEXT uint missCount;

void main()
{
    missCount = 1;
}
#endif
