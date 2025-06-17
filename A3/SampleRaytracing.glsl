#extension GL_EXT_ray_query : require
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct Payload
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

layout( location = 0 ) rayPayloadEXT Payload payload;

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

	payload.radiance = vec3(0.0);
	payload.depth = 0;

	traceRayEXT(
		topLevelAS,                         // topLevel
		gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
		0, 1, 0,                            // sbtRecordOffset, sbtRecordStride, missIndex
		g.cameraPos, 0.0, rayDir, 100.0,    // origin, tmin, direction, tmax
		0 );                                 // payload

	vec3 finalColor = payload.radiance;

	imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( finalColor, 1.0 ) );
}
#endif

#if CLOSEST_HIT_SHADER
//=========================
//	CLOSEST HIT SHADER
//=========================

#define LIGHT_INSTANCE_INDEX 6
#define PI 3.1415926535

struct TriangleVertices {
	vec4 p0;
	vec4 p1;
	vec4 p2;

	vec4 n0;
	vec4 n1;
	vec4 n2;
};

struct VertexAttributes
{
	vec4 norm;
	vec4 uv;
};

struct ObjectDesc {
    uint64_t vertexPositionDeviceAddress;
    uint64_t vertexAttributeDeviceAddress;
    uint64_t indexDeviceAddress;
    uint64_t cumulativeTriangleAreaAddress;
    vec4     objToWorld[3];
    uint     triangleCount;
	uint     padding0; // 4 bytes
    uint     padding1; // 4 bytes
};

// 점(p.x, p.y, p.z, 1)을 변환
vec3 transformPoint(vec4 row[3], vec3 p)
{
    vec4 v = vec4(p, 1.0);
    return vec3(
        dot(row[0], v),
        dot(row[1], v),
        dot(row[2], v)
    );
}

// 방향벡터(p.x, p.y, p.z, 0)을 변환
vec3 transformVector(vec4 row[3], vec3 v)
{
    vec4 w = vec4(v, 0.0);
    return vec3(
        dot(row[0], w),
        dot(row[1], w),
        dot(row[2], w)
    );
}

layout(buffer_reference, scalar) buffer PositionBuffer { vec4 p[]; };
layout(buffer_reference, scalar) buffer AttributeBuffer { VertexAttributes a[]; };
layout(buffer_reference, scalar) buffer IndexBuffer { uint i[]; };
layout(buffer_reference, scalar) buffer cumulativeTriangleAreaBuffer { float t[]; };

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

layout( location = 0 ) rayPayloadInEXT Payload payload;
hitAttributeEXT vec2 attribs;

/////////////////////////////////////////////////////////////////////////////////////////////
float RandomValue(inout uint state) {
    state *= (state + 195439) * (state + 124395) * (state + 845921);
    return float(state) * (1.0 / 4294967295.0);
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

/////////////////////////////////////////////////////////////////////////////////////////////
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

/////////////////////////////////////////////////////////////////////////////////////////////
float getLightArea() 
{
	ObjectDesc lightObjDesc = objectDescs.desc[LIGHT_INSTANCE_INDEX];
	cumulativeTriangleAreaBuffer sum = cumulativeTriangleAreaBuffer(lightObjDesc.cumulativeTriangleAreaAddress);

	return sum.t[lightObjDesc.triangleCount];
}

uint binarySearchTriangleIdx(inout uint state, float lightArea) 
{
	ObjectDesc lightObjDesc = objectDescs.desc[LIGHT_INSTANCE_INDEX];
	cumulativeTriangleAreaBuffer sum = cumulativeTriangleAreaBuffer(lightObjDesc.cumulativeTriangleAreaAddress);
	float target = RandomValue(state) * lightArea;

	uint l = 1;
	uint r = lightObjDesc.triangleCount;
	while (l <= r) {
		uint mid = l + ((r - l) / 2);
		if (sum.t[mid - 1] < target && target <= sum.t[mid])
			return mid;
		else if (target > sum.t[mid]) l = mid + 1;
		else r = mid - 1;
	}
}

void uniformSamplePointOnTriangle(inout uint state, uint triangleIdx, 
								  out vec3 pointOnTriangle, 
								  out vec3 normalOnTriangle, 
								  out vec3 pointOnTriangleWorld, 
								  out vec3 normalOnTriangleWorld) 
{
	ObjectDesc lightObjDesc = objectDescs.desc[LIGHT_INSTANCE_INDEX];

	IndexBuffer lightIndexBuffer = IndexBuffer(lightObjDesc.indexDeviceAddress);
	uvec3 index;
	uint base = triangleIdx * 3u;
	index.x = lightIndexBuffer.i[base + 0];
	index.y = lightIndexBuffer.i[base + 1];
	index.z = lightIndexBuffer.i[base + 2];

	PositionBuffer lightPositionBuffer = PositionBuffer(lightObjDesc.vertexPositionDeviceAddress);
	vec4 p0 = lightPositionBuffer.p[index.x];
    vec4 p1 = lightPositionBuffer.p[index.y];
    vec4 p2 = lightPositionBuffer.p[index.z];

	AttributeBuffer attributeBuffer = AttributeBuffer(lightObjDesc.vertexAttributeDeviceAddress);
	VertexAttributes a0 = attributeBuffer.a[index.x];
	VertexAttributes a1 = attributeBuffer.a[index.y];
	VertexAttributes a2 = attributeBuffer.a[index.z];
	vec3 n0 = normalize(a0.norm.xyz);
	vec3 n1 = normalize(a1.norm.xyz);
 	vec3 n2 = normalize(a2.norm.xyz);

	vec2 xi   = vec2(RandomValue(state), RandomValue(state));
	float su0 = sqrt(xi.x);
	float u  = 1.0 - su0;
	float v  = xi.y * su0;
	float w  = 1.0 - u - v;

	pointOnTriangle = (w * p0 + u * p1 + v * p2).xyz;
	normalOnTriangle = normalize(w * n0 + u * n1 + v * n2).xyz;
	
	vec4[3] lightTransform = lightObjDesc.objToWorld;

	pointOnTriangleWorld = transformPoint(lightTransform, pointOnTriangle);
	normalOnTriangleWorld = normalize(transformVector(lightTransform, normalOnTriangle));
}
/////////////////////////////////////////////////////////////////////////////////////////////

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

	const float eps = 1e-4;
	vec3 lightEmittance = vec3(5.0);

	if (payload.depth >= sq.maxDepth) 
		return;

	if (payload.depth == 0 && gl_InstanceCustomIndexEXT == LIGHT_INSTANCE_INDEX) // depth = 0
	{
		payload.radiance = lightEmittance;
		return;
	}

	uint rngState = (pixelCoord.y * screenSize.x + pixelCoord.x) * (payload.depth + 1);
	//////////////////////////////////////////////////////////////// Direct Light
	vec3 tempRadianceD = vec3(0.0);
	float lightArea = getLightArea();
	const uint directLightSampleCount = (payload.depth == 0 ? sq.numSamples : 1);
	for (int i=0; i < directLightSampleCount; ++i) {
		uint rngState = (pixelCoord.y * screenSize.x + pixelCoord.x) * (payload.depth + 1) * (i + 1);
		uint triangleIdx = binarySearchTriangleIdx(rngState, lightArea);

		vec3 pointOnTriangle, normalOnTriangle, pointOnTriangleWorld, normalOnTriangleWorld;
		uniformSamplePointOnTriangle(rngState, triangleIdx, 
									 pointOnTriangle, 
									 normalOnTriangle, 
									 pointOnTriangleWorld, 
									 normalOnTriangleWorld);

		float distToLight = length(pointOnTriangleWorld - worldPos);
		vec3 shadowRayDir = normalize(pointOnTriangleWorld - worldPos);

		float tMax = max(distToLight - eps, 0.0); // 목표 직전까지

		rayQueryEXT rq;
		rayQueryInitializeEXT(
			rq, topLevelAS,
			gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
			0xFF,
			worldPos,
			eps,
			shadowRayDir,
			tMax
		);
		
		while (rayQueryProceedEXT(rq)) {}

		bool visible = rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT;

		float visibility = visible ? 1.0 : 0.0;
		vec3 r = pointOnTriangleWorld - worldPos;
		float cosL = max(dot(normalOnTriangleWorld, -shadowRayDir), 0.0);
		float P = cosL / dot(r, r);
		tempRadianceD += (color / PI) * max(dot(worldNormal, shadowRayDir), 0.0) * lightEmittance * visibility * P * lightArea;
	}
	tempRadianceD *= (1 / float(directLightSampleCount)); // average
	payload.radiance = tempRadianceD;

	//////////////////////////////////////////////////////////////// Indirect Light
	uint tempDepth = payload.depth;
	vec3 tempRadianceI = vec3(0.0);
	uint numSampleByDepth = (payload.depth == 0 ? sq.numSamples : 1);

	payload.depth += 1;
	for (uint i=0; i < numSampleByDepth; ++i)
	{
		uint rngState = (pixelCoord.y * screenSize.x + pixelCoord.x) * (payload.depth + 1) * (i + 1);
		vec3 rayDir = RandomHemisphereDirection(worldNormal, rngState);

		traceRayEXT(
			topLevelAS,                         // topLevel
			gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
			0, 1, 1,                            // sbtRecordOffset, sbtRecordStride, missIndex
			worldPos, eps, rayDir, 100.0,  	// origin, tmin, direction, tmax
			0);                                 // payload 

		tempRadianceI += 2.0 * color * max(dot(worldNormal, rayDir), 0.0) * payload.radiance;
	}
	tempRadianceI *= 1.0 / float(numSampleByDepth);
	payload.radiance = tempRadianceD + tempRadianceI;

	payload.depth = tempDepth;
}
#endif

#if SHADOW_MISS_SHADER
//=========================
//	SHADOW MISS SHADER
//=========================

layout( location = 0 ) rayPayloadInEXT Payload payload;

void main()
{
	payload.radiance = vec3(0.0);
}
#endif


#if ENVIRONMENT_MISS_SHADER
//=========================
//	ENVIRONMENT MISS SHADER
//=========================

layout( location = 0 ) rayPayloadInEXT Payload payload;

void main()
{
	payload.radiance = vec3(0.0);
}
#endif