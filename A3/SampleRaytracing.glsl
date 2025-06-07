#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define MAX_DEPTH 10
#define SAMPLE_COUNT 2000

struct RayPayload
{
	vec3 radiance;
	vec3 HitLightColor;
	uint depth;
};

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

//layout( location = 0 ) rayPayloadEXT vec3 hitValue;
layout(location = 0) rayPayloadEXT RayPayload payload;

vec3 toneMapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

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

	payload.radiance = vec3( 0.0 );
	payload.HitLightColor = vec3(0.0);
	payload.depth = 0;

	traceRayEXT(
		topLevelAS,                         // topLevel
		gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
		0, 1, 0,                            // sbtRecordOffset, sbtRecordStride, missIndex
		g.cameraPos, 0.0, rayDir, 100.0,    // origin, tmin, direction, tmax
		0 );                                 // payload

	vec3 finalColor = payload.radiance / float(MAX_DEPTH);
	finalColor = toneMapACES(finalColor);
	//finalColor = gammaCorrect(finalColor);

	imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( finalColor, 0.0 ) );
}
#endif

#if CLOSEST_HIT_SHADER
//=========================
//	CLOSEST HIT SHADER
//=========================

// 난수 함수 (예시: 해시 기반)
float rand(vec2 co) {
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

// 반구 샘플링
vec3 randomHemisphereNormal(vec3 normal, vec2 seed) {
    float theta = acos(rand(seed)); // [0, π/2]
    float phi = rand(seed.yx + vec2(0.123, 0.456)) * 6.2831853;

    vec3 dir = vec3(
        sin(theta) * cos(phi),
        sin(theta) * sin(phi),
        cos(theta)
    );

    // local space → world space
    if (dot(dir, normal) < 0.0) dir = -dir;
    return normalize(dir);
}

float RandomValue(inout uint state) {
    state *= (state + 195439) * (state + 124395) * (state + 845921);
    return state / 4294967295.0;
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
	vec4 p0 = positionBuffer.p[index.x];
    vec4 p1 = positionBuffer.p[index.y];
    vec4 p2 = positionBuffer.p[index.z];
	
	float u = attribs.x;
	float v = attribs.y;
	float w = 1.0 - u - v;
	
	vec3 position = (w * p0 + u * p1 + v * p2).xyz;

	AttributeBuffer attributeBuffer = AttributeBuffer(objDesc.vertexAttributeDeviceAddress);
	VertexAttributes vertexAttributes0 = attributeBuffer.a[index.x];
	VertexAttributes vertexAttributes1 = attributeBuffer.a[index.y];
	VertexAttributes vertexAttributes2 = attributeBuffer.a[index.z];
	vec3 n0 = normalize(vertexAttributes0.norm.xyz);
	vec3 n1 = normalize(vertexAttributes1.norm.xyz);
 	vec3 n2 = normalize(vertexAttributes2.norm.xyz);
	vec3 localNormal = normalize(w * n0 + u * n1 + v * n2);

	vec3 worldPos = (gl_ObjectToWorldEXT * vec4(position, 1.0)).xyz;
    vec3 worldNormal = normalize(gl_ObjectToWorldEXT * vec4(localNormal, 0.0));
	
	if (LIGHT_INSTANCE_INDEX == gl_InstanceCustomIndexEXT) 
	{
		gPayload.HitLightColor = vec3(255.0, 255.0, 255.0);
	}

	if(gPayload.depth + 1 < MAX_DEPTH)
	{
		uvec2 pixelCoord = gl_LaunchIDEXT.xy;
		uvec2 screenSize = gl_LaunchSizeEXT.xy;
		uint rngState = pixelCoord.y * screenSize.x + pixelCoord.x;
		gPayload.depth += 1;
		for (int i = 0; i < SAMPLE_COUNT; ++i)
		{
			gPayload.HitLightColor = vec3(0.0);
			//vec2 seed = vec2(i, gl_LaunchIDEXT.x ^ gl_LaunchIDEXT.y);
			//vec2 seed = vec2(gl_LaunchSizeEXT.x + i, gl_LaunchSizeEXT.y + i);
			//vec2 seed = vec2(float(gl_LaunchIDEXT.x + i * 17), float(gl_LaunchIDEXT.y + i * 21));
			//vec3 rayDir = randomHemisphereNormal(worldNormal, seed);
			vec3 rayDir = RandomHemisphereDirection(worldNormal, rngState);
			traceRayEXT(
				topLevelAS,
				gl_RayFlagsOpaqueEXT,
				0xFF,
				0, 1, 0,
				worldPos,
				0.0001,
				rayDir,
				100.0,
				0
			);

			vec3 finalColor = color * gPayload.HitLightColor / float(SAMPLE_COUNT);
			gPayload.radiance += finalColor;
		}
	}
}

#endif

#if SHADOW_MISS_SHADER
//=========================
//	SHADOW MISS SHADER
//=========================
//layout( location = 1 ) rayPayloadInEXT uint missCount;

void main()
{

}
#endif


#if ENVIRONMENT_MISS_SHADER
//=========================
//	ENVIRONMENT MISS SHADER
//=========================
//layout( location = 0 ) rayPayloadInEXT vec3 hitValue;

void main()
{
	//hitValue = vec3( 0.0, 0.0, 0.2 );
}
#endif