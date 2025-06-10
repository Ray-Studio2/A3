#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct Ray
{
    vec3 radiance;
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

layout( location = 0 ) rayPayloadEXT Ray ray;

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

	ray.radiance = vec3(0.0);
	ray.depth = 0;

	traceRayEXT(
		topLevelAS,                         // topLevel
		gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
		0, 1, 0,                            // sbtRecordOffset, sbtRecordStride, missIndex
		g.cameraPos, 0.0, rayDir, 100.0,    // origin, tmin, direction, tmax
		0 );                                 // payload

	vec3 finalColor = ray.radiance;

	imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( finalColor, 1.0 ) );
}
#endif

#if CLOSEST_HIT_SHADER
//=========================
//	CLOSEST HIT SHADER
//=========================

#define LIGHT_INSTANCE_INDEX 6

struct VertexAttributes
{
	vec4 norm;
	vec4 uv;
};

struct ObjectDesc {
	uint64_t vertexPositionDeviceAddress;
	uint64_t vertexAttributeDeviceAddress;
	uint64_t indexDeviceAddress;
};

layout(buffer_reference, scalar) buffer PositionBuffer { vec4 p[]; };
layout(buffer_reference, scalar) buffer AttributeBuffer { VertexAttributes a[]; };
layout(buffer_reference, scalar) buffer IndexBuffer { uint i[]; };

layout(binding=3, scalar) buffer ObjectDescBuffer {
	ObjectDesc desc[];
} objectDescs;

layout( binding=4 ) uniform SampleQuality {
	uint maxDepth;
	uint numSamples;
} sq;

layout( shaderRecordEXT ) buffer CustomData
{
	vec3 color;
};

layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;

layout( location = 0 ) rayPayloadInEXT Ray ray;
hitAttributeEXT vec2 attribs;

float RandomValue(inout uint state) {
    state *= (state + 195439) * (state + 124395) * (state + 845921);
    return state / 4294967295.0; // 2^31 - 1 (uint 최댓값으로 나눔) -> 0~1 사이의 실수
}

float RandomValueNormalDistribution(inout uint state) {
    float theta = 2 * 3.1415926 * RandomValue(state);
    float rho = sqrt(-2.0 * log(RandomValue(state)));
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

uint wang_hash(uint seed)
{
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

uint generateSeed(uvec2 pixel, uint sampleIndex, uint depth, uint axis)
{
    uint base = pixel.x * 1973 + pixel.y * 9277 + sampleIndex * 26699 + depth * 59359 + axis * 19937;
    return wang_hash(base);
}

float random(uvec2 pixel, uint sampleIndex, uint depth, uint axis)
{
    return float(generateSeed(pixel, sampleIndex, depth, axis)) / 4294967296.0;
}
// Uniform sampling on unit sphere
vec3 uniformSampleSphere(uvec2 pixel, uint sampleIndex, uint depth)
{
    float z = random(pixel, sampleIndex, depth, 0) * 2.0 - 1.0;
    float phi = random(pixel, sampleIndex, depth, 1) * 2.0 * 3.14159265;
    float r = sqrt(1.0 - z * z);
    return normalize(vec3(r * cos(phi), r * sin(phi), z));
}

// Cosine weighted sampling on hemisphere
vec3 cosineSampleHemisphere(uvec2 pixel, uint sampleIndex, uint depth, vec3 normal)
{
    float r1 = random(pixel, sampleIndex, depth, 0);
    float r2 = random(pixel, sampleIndex, depth, 1);
    
    float phi = 2.0 * 3.14159265 * r1;
    float cosTheta = sqrt(r2);
    float sinTheta = sqrt(1.0 - r2);
    
    vec3 direction = vec3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    );
    
    vec3 bitangent = normalize(cross(abs(normal.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0), normal));
    vec3 tangent = cross(normal, bitangent);
    
    return normalize(tangent * direction.x + bitangent * direction.y + normal * direction.z);
}

void main()
{
    ObjectDesc objDesc = objectDescs.desc[gl_InstanceCustomIndexEXT];

	IndexBuffer indexBuffer = IndexBuffer(objDesc.indexDeviceAddress);
	uvec3 index;
	uint base = gl_PrimitiveID * 3u;
	index.x = indexBuffer.i[base + 0];
	index.y = indexBuffer.i[base + 1];
	index.z = indexBuffer.i[base + 2];

	PositionBuffer positionBuffer = PositionBuffer(objDesc.vertexPositionDeviceAddress);
	vec4 p0 = positionBuffer.p[index.x];
    vec4 p1 = positionBuffer.p[index.y];
    vec4 p2 = positionBuffer.p[index.z];
	
	float u = attribs.x;
	float v = attribs.y;
	float w = 1.0 - u - v;
	
	vec3 position = (w * p0 + u * p1 + v * p2).xyz;

	AttributeBuffer attributeBuffer = AttributeBuffer(objDesc.vertexAttributeDeviceAddress);
	VertexAttributes a0 = attributeBuffer.a[index.x];
	VertexAttributes a1 = attributeBuffer.a[index.y];
	VertexAttributes a2 = attributeBuffer.a[index.z];
	vec3 n0 = normalize(a0.norm.xyz);
	vec3 n1 = normalize(a1.norm.xyz);
 	vec3 n2 = normalize(a2.norm.xyz);
	vec3 localNormal = normalize(w * n0 + u * n1 + v * n2);

	mat4x3 objToWorld = gl_ObjectToWorldEXT;
	vec3 worldNormal = normalize(objToWorld * vec4(localNormal, 0.0));
	vec3 worldPos = objToWorld * vec4(position, 1.0);

    uvec2 pixelCoord = gl_LaunchIDEXT.xy;
    uvec2 screenSize = gl_LaunchSizeEXT.xy;

	vec3 lightColor = vec3(0.0);
	vec3 temp = vec3(0.0);

	uint tempDepth = ray.depth;

	if (LIGHT_INSTANCE_INDEX == gl_InstanceCustomIndexEXT) 
	{
		lightColor = vec3(5.0);
	}

	uint numSampleByDepth = (ray.depth == 0 ? sq.numSamples : sq.numSamples > 4 ? 4 : sq.numSamples);
	if (ray.depth + 1 < sq.maxDepth) {
		ray.depth += 1;
		for (uint i=0; i < numSampleByDepth; ++i) {
			uint rngState = (pixelCoord.y * screenSize.x + pixelCoord.x) * (ray.depth + 1) * (i + 1);
			vec3 rayDir = RandomHemisphereDirection(worldNormal, rngState);
            //vec3 rayDir = uniformSampleSphere(gl_LaunchIDEXT.xy, i, ray.depth);

			traceRayEXT(
				topLevelAS,                         // topLevel
				gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
				0, 1, 1,                            // sbtRecordOffset, sbtRecordStride, missIndex
				worldPos, 0.0001, rayDir, 100.0,  	// origin, tmin, direction, tmax
				0);                                 // payload 

			temp += max(dot(worldNormal, rayDir), 0.0) * ray.radiance;
		}
		temp *= 1.0 / float(numSampleByDepth);
		ray.depth = tempDepth;
	}
	ray.radiance = lightColor +  2.0 * color * temp; // FIX: doesn't show anything; very bright with no division
}
#endif

#if SHADOW_MISS_SHADER
//=========================
//	SHADOW MISS SHADER
//=========================

layout( location = 0 ) rayPayloadInEXT Ray ray;

void main()
{
	ray.radiance = vec3(0.0);
}
#endif


#if ENVIRONMENT_MISS_SHADER
//=========================
//	ENVIRONMENT MISS SHADER
//=========================

layout( location = 0 ) rayPayloadInEXT Ray ray;

void main()
{
	ray.radiance = vec3(0.0);
}
#endif