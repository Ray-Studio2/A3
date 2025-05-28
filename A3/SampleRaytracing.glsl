#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define MAX_DEPTH 5
#define SAMPLE_COUNT 1000

struct RayPayload
{
	vec3 radiance;
	vec3 HitLightColor;
	uint depth;
};

layout( binding = 2 ) uniform CameraProperties
{
	vec3 cameraPos;
	float yFov_degree;
} g;

#if RAY_GENERATION_SHADER
//=========================
//	RAY GENERATION SHADER
//=========================
layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;
layout( binding = 1, rgba8 ) uniform image2D image;

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

// 0.0 ~ 1.0 사이의 float 반환
float RandomValue(inout uint state) {
    state *= (state + 195439) * (state + 124395) * (state + 845921);
    return state / 4294967295.0;
}
// 0.0 ~ 1.0 사이의 float 반환
float RandomValue2(inout uint state) {
    state = uint(state ^ 61u) ^ (state >> 16);
    state *= 9u;
    state = state ^ (state >> 4);
    state *= 0x27d4eb2du;
    state = state ^ (state >> 15);
    return float(state) / 4294967295.0;
}

// SampleUniformHemisphere()는 +Z 방향(normal)을 기준으로 반구에서 균일하게 방향을 샘플링함
// 이 로컬 기준 샘플을 월드 기준 normal 방향에 정렬시키기 위해 회전 행렬(TBN)을 생성함
// TBN은 UE의 TRotationMatrix::MakeFromZ와 동일한 원리
mat3 CreateTangentSpace(vec3 normal) {
    vec3 up = abs(normal.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    return mat3(tangent, bitangent, normal);
}
vec3 SampleUniformHemisphere(vec2 xi) {
    float z = xi.x;
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = 2.0 * 3.1415926 * xi.y;

    return vec3(r * cos(phi), r * sin(phi), z);
}
// Uniform fast
vec3 RandomHemisphereNormal(vec3 normal, vec2 xi) {
    vec3 local = SampleUniformHemisphere(xi);
	// 기준축이 Z였던 것을 기준축을 normal로 바꾸겠다~ 는 회전 행렬
	// 예를 들어 Z축 기준으로 오른쪽을 향하는 vector였으면 이 회전 행렬을 곱하면 normal 기준으로 오른쪽을 향하는 vector가 된다~
    mat3 tbn = CreateTangentSpace(normal);
    return normalize(tbn * local);
}

// Uniform slow
//vec3 RandomHemisphereNormal(vec3 Normal, vec2 Seed)
//{
//    vec3 Result;
//    const int MaxTries = 10; // 무한 루프 방지용
//
//    // Unit sphere 안에서 균일한 샘플을 찾음
//    for (int i = 0; i < MaxTries; ++i) {
//        // [-1, 1] 범위 난수 생성
//        vec3 Candidate = vec3(
//            rand(Seed + float(i) + vec2(1.0, 0.0)) * 2.0 - 1.0,
//            rand(Seed + float(i) + vec2(0.0, 1.0)) * 2.0 - 1.0,
//            rand(Seed + float(i) + vec2(1.0, 1.0)) * 2.0 - 1.0
//        );
//
//        if (dot(Candidate, Candidate) < 1.0) {
//            Result = normalize(Candidate);
//            break;
//        }
//    }
//
//    // 반구 방향으로 반사 (z > 0과 같은 효과)
//    if (dot(Result, Normal) < 0.0)
//        Result = -Result;
//
//    return Result;
//}

// Non Uniform?
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
	mat3 normalMatrix = transpose(inverse(mat3(gl_ObjectToWorldEXT)));
	vec3 worldNormal = normalize(normalMatrix * localNormal);
	
	if (LIGHT_INSTANCE_INDEX == gl_InstanceCustomIndexEXT) 
	{
		gPayload.HitLightColor = vec3(500.0, 500.0, 500.0);
	}

	if(gPayload.depth + 1 < MAX_DEPTH)
	{
		gPayload.depth += 1;
		uvec2 pixelCoord = gl_LaunchIDEXT.xy;
		uvec2 screenSize = gl_LaunchSizeEXT.xy;
		uint randomSeedBase = pixelCoord.y * screenSize.x + pixelCoord.x;
		for (int i = 0; i < SAMPLE_COUNT; ++i)
		{
			gPayload.HitLightColor = vec3(0.0);
			vec2 RandomSeed = vec2(RandomValue2(randomSeedBase), RandomValue(randomSeedBase));
			vec3 rayDir = RandomHemisphereNormal(worldNormal, RandomSeed);
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

			vec3 finalColor = color * gPayload.HitLightColor / float(SAMPLE_COUNT);// * MAX_DEPTH / gPayload.depth;
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