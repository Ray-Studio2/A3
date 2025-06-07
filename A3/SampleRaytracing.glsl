#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

float rand(vec2 co) {
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

vec3 randomHemisphereNormal(vec3 normal, vec2 seed) {
    float theta = acos(rand(seed)); // [0, ��/2]
    float phi = rand(seed.yx + vec2(0.123, 0.456)) * 6.2831853;

    vec3 dir = vec3(
        sin(theta) * cos(phi),
        sin(theta) * sin(phi),
        cos(theta)
    );

    // local space �� world space
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
    // Cosine-weighted hemisphere sampling
    float r1 = RandomValue(state);
    float r2 = RandomValue(state);
    
    float theta = acos(sqrt(1.0 - r1)); // cosine-weighted
    float phi = 2.0 * 3.14159265 * r2;
    
    // Generate direction in local space
    vec3 localDir = vec3(
        sin(theta) * cos(phi),
        sin(theta) * sin(phi),
        cos(theta)
    );
    
    // Transform to world space aligned with normal
    vec3 tangent = abs(normal.x) > 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    tangent = normalize(cross(tangent, normal));
    vec3 bitangent = cross(normal, tangent);
    
    return tangent * localDir.x + bitangent * localDir.y + normal * localDir.z;
}

struct RayPayload 
{
	vec3 color;
	int depth;
};

#define MAX_DEPTH 1

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

layout( location = 0 ) rayPayloadEXT RayPayload payload;

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

	payload = RayPayload(vec3(0.0), 0);

	traceRayEXT(
		topLevelAS,                         // topLevel
		gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
		0, 1, 0,                            // sbtRecordOffset, sbtRecordStride, missIndex
		g.cameraPos, 0.0, rayDir, 100.0,    // origin, tmin, direction, tmax
		0 );                                 // payload

	imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( payload.color, 0.0 ) );
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
} objectDescs;

layout( shaderRecordEXT ) buffer CustomData
{
	vec3 color;
};

layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;

layout( location = 0 ) rayPayloadInEXT RayPayload payload;
layout( location = 1 ) rayPayloadEXT uint missCount;
hitAttributeEXT vec2 attribs;

#define AO_SAMPLES 128

void main() {
	ObjectDesc objDesc = objectDescs.desc[gl_InstanceCustomIndexEXT];

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

	mat4x3 objToWorld = gl_ObjectToWorldEXT;
	vec3 worldNormal = normalize(objToWorld * vec4(localNormal, 0.0));
	vec3 worldPos = objToWorld * vec4(position, 1.0);
	
	uvec2 pixelCoord = gl_LaunchIDEXT.xy;
	uvec2 screenSize = gl_LaunchSizeEXT.xy;
	uint rngState = pixelCoord.y * screenSize.x + pixelCoord.x;
	// Add more entropy to reduce pixel correlation
	rngState = rngState * 1664525u + 1013904223u;
	rngState ^= rngState >> 16;
	rngState *= 0x85ebca6bu;
	rngState ^= rngState >> 13;


	float ao = 0;

	for (int i = 0; i < AO_SAMPLES; ++i) {
		vec3 aoDir = RandomHemisphereDirection(worldNormal, rngState);
			
		missCount = 0;

		traceRayEXT(
			topLevelAS,
			gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xff,
			0, 1, 1,
			worldPos + worldNormal * 0.01, 0.001, aoDir, 0.7, // origin, tmin, direction, tmax
			1 );                                // payload location (use 1 for missCount)
			
		ao += float(missCount);
	}

	ao /= float(AO_SAMPLES);

	payload.color = color * ao;
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


#if ENVIRONMENT_MISS_SHADER
//=========================
//	ENVIRONMENT MISS SHADER
//=========================
layout( location = 0 ) rayPayloadInEXT RayPayload payload;

void main()
{
	payload.color = vec3( 0.0, 0.0, 0.2 );  // Dark blue background
}
#endif