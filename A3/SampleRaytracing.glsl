#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define SAMPLE_COUNT 100
#define MAX_DEPTH 2
#define PI 3.1415926535

struct EnvImportanceSampleData {
    float conditional_cdf;   // .r
    float conditional_pdf;   // .g
    float marginal_pdf;      // .b
    float marginal_cdf;      // .a
};

vec3 toneMapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 toneMapReinhard(vec3 x) {
    return x / (1.0 + x);
}

vec3 applyGamma(vec3 color) {
    return pow(color, vec3(1.0 / 2.2));
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
    return vec3(r * cos(phi), r * sin(phi), z);
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

struct Payload {
    vec3 hitValue;
    uint depth;
    vec3 rayDirection;
    bool bEnvMap;
};

#define LIGHT_INSTANCE_INDEX 6

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

layout( location = 0)  rayPayloadEXT Payload payload;

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

	payload.hitValue = vec3( 0.0 );
	payload.depth = 1;
	payload.rayDirection = rayDir;
	payload.bEnvMap = false;

	traceRayEXT(
		topLevelAS,                         // topLevel
		gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
		0, 1, 0,                            // sbtRecordOffset, sbtRecordStride, missIndex
		g.cameraPos, 0.0, rayDir, 100.0,    // origin, tmin, direction, tmax
		0);                                 // payload

    vec4 loadColor = imageLoad(image, ivec2(gl_LaunchIDEXT.xy));
    vec3 prevColor = vec3(loadColor.x, loadColor.y, loadColor.z);
    vec3 blendColor = (prevColor * (SAMPLE_COUNT - 1) + payload.hitValue) / SAMPLE_COUNT;
    
    float exposure = 1.66f; // might be uniform
    vec3 hdrColor = payload.hitValue; // pathtracer input
    vec3 ldrColor = 1.0 - exp(-exposure * hdrColor);
    vec3 finalColor = pow(ldrColor, vec3(1/2.2, 1/2.2, 1/2.2)); // simple gamma correction
    
	imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4(finalColor, 0.0 ) );
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

#define PI 3.1415926535
#define EPS 1e-6

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
layout( binding = 2 ) uniform CameraProperties
{
	vec3 cameraPos;
	float yFov_degree;
} g;
layout(set = 0, binding = 4) uniform sampler2D environmentMap;
layout(binding = 5, scalar) buffer EnvImportanceSamplingBuffer {
    EnvImportanceSampleData data[];
};

vec3 sampleEnvDirection(uvec2 pixel, uint sampleIndex, uint depth, out float pdf)
{
    float marginalY = random(pixel, sampleIndex, depth, 0);
    float conditionalX = random(pixel, sampleIndex, depth, 1);

    ivec2 texSize = textureSize(environmentMap, 0);
    int width = texSize.x;
    int height = texSize.y;

    // ---- Marginal CDF 이진 탐색 (Y 방향) ----
    int yLow = 0;
    int yHigh = height - 1;
    while (yLow < yHigh) {
        int yMid = (yLow + yHigh) / 2;
        float cdf = data[yMid * width].marginal_cdf;
        if (cdf < marginalY)
            yLow = yMid + 1;
        else
            yHigh = yMid;
    }
    int y = yLow;

    // ---- Conditional CDF 이진 탐색 (X 방향) ----
    int xLow = 0;
    int xHigh = width - 1;
    while (xLow < xHigh) {
        int xMid = (xLow + xHigh) / 2;
        float cdf = data[y * width + xMid].conditional_cdf;
        if (cdf < conditionalX)
            xLow = xMid + 1;
        else
            xHigh = xMid;
    }
    int x = xLow;

    // ---- UV to Direction ----
    float u = float(x) / float(width - 1);
    float v = float(y) / float(height - 1);
    float phi = 2.0 * PI * u;
    float theta = PI * v;
    float sinTheta = sin(theta);

    vec3 dir = vec3(sinTheta * cos(phi), cos(theta), sinTheta * sin(phi));

    // ---- PDF with solid angle correction ----
    float texelPdf = data[y * width + x].conditional_pdf * data[y * width + x].marginal_pdf;
    pdf = width * height * texelPdf / (2.0 * PI * PI * sinTheta);

    return normalize(dir);
}

layout(location = 0) rayPayloadInEXT  Payload payload;
hitAttributeEXT vec2 attribs;

void main()
{
    if (payload.depth >= MAX_DEPTH) {
        payload.hitValue = vec3(0);
        return;
    }
    
    if (payload.bEnvMap)
    {
        payload.bEnvMap = false;
        return ;
    }
    
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

    vec3 accumulated = vec3(0.0);
    uint depthBeforeTrace = payload.depth;
    
    
    for (uint i = 0; i < SAMPLE_COUNT; ++i) {
        float pdf_env;
        vec3 sampleDir = sampleEnvDirection(gl_LaunchIDEXT.xy, i, depthBeforeTrace, pdf_env);
        float cosTheta = max(0.0, dot(worldNormal, sampleDir));
        if (cosTheta <= 0.f) continue;
       
        payload.hitValue = vec3(0.0);
        payload.depth = depthBeforeTrace + 1;
        payload.rayDirection = sampleDir;
        payload.bEnvMap = true;
        traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 1, 0, worldPos, 0.0001, sampleDir, 1000.0, 0);
        if (payload.bEnvMap == false) continue;
        
        vec3 Li = payload.hitValue;
        vec3 brdf = color / PI;
        accumulated += (Li * brdf * cosTheta) / pdf_env;
    }

    vec3 finalColor = accumulated / float(SAMPLE_COUNT);
    payload.hitValue = finalColor;
}
#endif

#if SHADOW_MISS_SHADER
//=========================
//	SHADOW MISS SHADER
//=========================
layout(location = 0) rayPayloadInEXT  Payload payload;

void main()
{
	payload.hitValue = vec3(0);
}
#endif

#if ENVIRONMENT_MISS_SHADER
//=========================
//   ENVIRONMENT MISS SHADER
//=========================
layout(location = 0) rayPayloadInEXT  Payload payload;
layout(set = 0, binding = 4) uniform sampler2D environmentMap;

void main()
{
    vec3 dir = normalize(payload.rayDirection); // ray direction 넘겨줘야 함
    vec2 uv = vec2(
        atan(dir.z, dir.x) / (2.0 * PI) + 0.5,
        acos(clamp(dir.y, -1.0, 1.0)) / PI
    );
    payload.hitValue = texture(environmentMap, uv).rgb;
}
#endif