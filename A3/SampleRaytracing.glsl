#extension GL_EXT_ray_tracing : enable

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

	hitValue = vec3( 0.0 );

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
layout( location = 0 ) rayPayloadInEXT vec3 hitValue;

void main()
{
	hitValue = vec3( 0.0, 0.0, 0.2 );
}
#endif

#if CLOSEST_HIT_SHADER
//=========================
//	CLOSEST HIT SHADER
//=========================
struct VertexAttributes
{
	vec4 norm;
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
layout( location = 1 ) rayPayloadEXT uint missCount;
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

    vec4 v0 = vPosBuffer[idxBuffer[gl_PrimitiveID * 3 + 0]]; // TODO: if indices data are modified, need to pass the index data also, repeated vertex rn
    vec4 v1 = vPosBuffer[idxBuffer[gl_PrimitiveID * 3 + 1]];
    vec4 v2 = vPosBuffer[idxBuffer[gl_PrimitiveID * 3 + 2]];

	VertexAttributes a0 = vAttribBuffer[idxBuffer[gl_PrimitiveID * 3 + 0]]; // TODO: if indices data are modified, need to pass the index data also, repeated vertex rn
    VertexAttributes a1 = vAttribBuffer[idxBuffer[gl_PrimitiveID * 3 + 1]];
    VertexAttributes a2 = vAttribBuffer[idxBuffer[gl_PrimitiveID * 3 + 2]];
    
    const vec3 barycentrics = {1.0f - attribs.x - attribs.y, attribs.x, attribs.y};

    vec3 localPos = (v0.xyz * barycentrics.x +
                     v1.xyz * barycentrics.y +
                     v2.xyz * barycentrics.z);
    vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    vec3 localNorm = normalize(a0.norm.xyz * barycentrics.x +
                               a1.norm.xyz * barycentrics.y +
                               a2.norm.xyz * barycentrics.z);
    vec3 worldNorm = localNorm; // TODO: need a normalMatrix for rotation and scale

    uvec2 pixelCoord = gl_LaunchIDEXT.xy;
    uvec2 screenSize = gl_LaunchSizeEXT.xy;
    uint rngState = pixelCoord.y * screenSize.x + pixelCoord.x; // 1-D array에서의 pixel 위치

    const uint numShadowRays = 3000;
    for (uint i=0; i < numShadowRays; ++i) {
        vec3 rayDir = RandomHemisphereDirection(worldNorm, rngState);
        traceRayEXT(
            topLevelAS,                         // topLevel
            gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xff,         // rayFlags, cullMask
            0, 1, 1,                            // sbtRecordOffset, sbtRecordStride, missIndex
            worldPos, 0.001, rayDir, 100.0,  // origin, tmin, direction, tmax
            1);                                 // payload
    }
    
    hitValue = color * (float(missCount) / numShadowRays);
}
#endif

#if SHADOW_MISS_SHADER
//=========================
//	SHADOW MISS SHADER
//=========================

void main()
{

}
#endif
