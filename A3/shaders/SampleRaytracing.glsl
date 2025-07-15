#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_query : require
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define PI 3.1415926535897932384626433832795
#define ENV_MISS_IDX 0
#define SHADOW_MISS_IDX 1

#include "shaders/SharedStructs.glsl"
#include "shaders/Bindings.glsl"
#include "shaders/Sampler.glsl"

#if RAY_GENERATION_SHADER
//=========================
//   RAY GENERATION SHADER
//=========================
layout(location = 0) rayPayloadEXT RayPayload gPayload;
void main()
{
    const vec3 cameraX = vec3( 1, 0, 0 );
    const vec3 cameraY = vec3( 0, -1, 0 );
    const vec3 cameraZ = vec3( 0, 0, -1 );
    const float aspect_y = tan( radians( g.yFov_degree ) * 0.5 );
    const float aspect_x = aspect_y * float( gl_LaunchSizeEXT.x ) / float( gl_LaunchSizeEXT.y );
    
    // Better random seed generation
    uint pixelIndex = gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;
    uint seed = pixelIndex;
    if (gImguiParam.isProgressive != 0u)
        seed = pixelIndex + g.currentFrame * 1664525u;
    uint rngState = pcg_hash(seed);
    
    // Anti-aliasing jitter
    float r1 = random(rngState);
    float r2 = random(rngState);
    
    const vec2 screenCoord = vec2( gl_LaunchIDEXT.xy ) + vec2( r1, r2 );
    const vec2 ndc = screenCoord / vec2( gl_LaunchSizeEXT.xy ) * 2.0 - 1.0;
    vec3 rayDir = ndc.x * aspect_x * cameraX + ndc.y * aspect_y * cameraY + cameraZ;
    
    // Initialize payload for path tracing
    gPayload.radiance = vec3( 0.0 );
    gPayload.depth = 0;
    gPayload.desiredPosition = vec3( 0.0 );
    gPayload.rngState = rngState;
    gPayload.rayDirection = rayDir;
    gPayload.bEnvMap = false;
    
    // Single ray per pixel per frame
    traceRayEXT(
       topLevelAS,
       gl_RayFlagsOpaqueEXT, 0xff,
       0, 1, ENV_MISS_IDX,
       g.cameraPos, 0.0, rayDir, 100.0,
       0 );
    
    vec3 currentSample = gPayload.radiance;
    
    vec3 finalColor = currentSample;
    if ( gImguiParam.isProgressive != 0u ) {
        // Progressive accumulation
        vec3 previousAccumulation = vec3(0.0);
        if (g.currentFrame > 1) {
            previousAccumulation = imageLoad(accumulationImage, ivec2(gl_LaunchIDEXT.xy)).rgb;
        }
        
        // Proper incremental average
        vec3 accumulated = (previousAccumulation * float(g.currentFrame - 1) + currentSample) / float(g.currentFrame);
        
        // Store in accumulation buffer
        imageStore(accumulationImage, ivec2(gl_LaunchIDEXT.xy), vec4(accumulated, 1.0));
        finalColor = accumulated;
    }

    vec3 finalfinalColor = pow(1.0 - exp(-g.exposure * finalColor), vec3(1/2.2, 1/2.2, 1/2.2)); // simple gamma correction
    
   imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( finalfinalColor, 1.0 ) );
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////

float getLightArea() 
{
	ObjectDesc lightObjDesc = gObjectDescs.desc[gLightBuffer.lightIndex[0]];
	cumulativeTriangleAreaBuffer sum = cumulativeTriangleAreaBuffer(lightObjDesc.cumulativeTriangleAreaAddress);

	return sum.t[gLightBuffer.lights[0].triangleCount];
}

/////////////////////////////////////////////////////////////////////////////////////////////

#if BRUTE_FORCE_LIGHT_ONLY_CLOSEST_HIT_SHADER
//=========================
//   BRUTE FORCE LIGHT ONLY CLOSEST HIT SHADER
//=========================

layout( shaderRecordEXT ) buffer CustomData
{
   vec3 color;
};

layout(location = 0) rayPayloadInEXT RayPayload gPayload;
hitAttributeEXT vec2 attribs;

void main()
{
    ObjectDesc objDesc = gObjectDescs.desc[gl_InstanceCustomIndexEXT];

    IndexBuffer indexBuffer = IndexBuffer(objDesc.indexDeviceAddress);
    uint base = gl_PrimitiveID * 3u;
    uvec3 index = uvec3(indexBuffer.i[base + 0], 
                        indexBuffer.i[base + 1], 
                        indexBuffer.i[base + 2]);

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

    LightData light = gLightBuffer.lights[0];

    const vec3 lightEmittance = vec3(light.emittance); // emittance per point
    const float lightArea = getLightArea();
    const vec3 brdf_p = color / PI;

    vec3 emit = vec3(0.0);
    vec3 temp = vec3(0.0);

    if (gl_InstanceCustomIndexEXT == gLightBuffer.lightIndex[0])
        emit = lightEmittance;

    uint tempDepth = gPayload.depth;
    uint numSampleByDepth = (gPayload.depth == 0 ? gImguiParam.numSamples : 1);
    if (gPayload.depth < gImguiParam.maxDepth) {
        for (uint i=0; i < numSampleByDepth; ++i) {
                float r3 = random(gPayload.rngState);
                float r4 = random(gPayload.rngState);
                vec2 seed = vec2(r3, r4);
                vec3 rayDir = RandomCosineHemisphere(worldNormal, seed);

                gPayload.depth++;
                traceRayEXT(
                    topLevelAS,                         // topLevel
                    gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
                    0, 1, SHADOW_MISS_IDX,              // sbtRecordOffset, sbtRecordStride, missIndex
                    worldPos, 0.0001, rayDir, 100.0,  	// origin, tmin, direction, tmax
                    0);                                 // payload 
                gPayload.depth = tempDepth;

            // const float cos_p = max(dot(worldNormal, rayDir), 1e-6);
            // const float pdf_p = cos_p / PI; // 샘플별 pdf
            temp += color * gPayload.radiance;
        }
        temp *= 1.0 / float(numSampleByDepth);
    }
    gPayload.radiance = emit + temp;
}

#endif

#if NEE_LIGHT_ONLY_CLOSEST_HIT_SHADER
//=========================
//   NEE LIGHT ONLY CLOSEST HIT SHADER
//=========================

layout( shaderRecordEXT ) buffer CustomData
{
   vec3 color;
};

layout(location = 0) rayPayloadInEXT RayPayload gPayload;
hitAttributeEXT vec2 attribs;

/////////////////////////////////////////////////////////////////////////////////////////////

uint binarySearchTriangleIdx(const float lightArea) 
{
	ObjectDesc lightObjDesc = gObjectDescs.desc[gLightBuffer.lightIndex[0]];
	cumulativeTriangleAreaBuffer sum = cumulativeTriangleAreaBuffer(lightObjDesc.cumulativeTriangleAreaAddress);
	float target = random(gPayload.rngState) * lightArea;

	uint l = 1;
	uint r = gLightBuffer.lights[0].triangleCount;
	while (l <= r) {
		uint mid = l + ((r - l) / 2);
		if (sum.t[mid - 1] < target && target <= sum.t[mid])
			return mid;
		else if (target > sum.t[mid]) l = mid + 1;
		else r = mid - 1;
	}
}

void uniformSamplePointOnTriangle(uint triangleIdx, 
								  out vec3 pointOnTriangle, 
								  out vec3 normalOnTriangle, 
								  out vec3 pointOnTriangleWorld, 
								  out vec3 normalOnTriangleWorld) 
{
	ObjectDesc lightObjDesc = gObjectDescs.desc[gLightBuffer.lightIndex[0]];

	IndexBuffer lightIndexBuffer = IndexBuffer(lightObjDesc.indexDeviceAddress);
	uint base = triangleIdx * 3u;
	uvec3 index = uvec3(lightIndexBuffer.i[base + 0],
                        lightIndexBuffer.i[base + 1],
	                    lightIndexBuffer.i[base + 2]);

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

	vec2 xi   = vec2(random(gPayload.rngState), random(gPayload.rngState));
	float su0 = sqrt(xi.x); // total needs to be 1
	float u  = 1.0 - su0;
	float v  = xi.y * su0;
	float w  = 1.0 - u - v;

	pointOnTriangle = (w * p0 + u * p1 + v * p2).xyz;
	normalOnTriangle = normalize(w * n0 + u * n1 + v * n2).xyz;

    mat4 localToWorld = transpose(gLightBuffer.lights[0].transform);
    pointOnTriangleWorld = (localToWorld * vec4(pointOnTriangle, 1.0f)).xyz;
	normalOnTriangleWorld = normalize(localToWorld * vec4(normalOnTriangle, 0.0f)).xyz;
}    

/////////////////////////////////////////////////////////////////////////////////////////////

void main()
{
    ObjectDesc objDesc = gObjectDescs.desc[gl_InstanceCustomIndexEXT];

    IndexBuffer indexBuffer = IndexBuffer(objDesc.indexDeviceAddress);

    uint base = gl_PrimitiveID * 3u;
    uvec3 index = uvec3(indexBuffer.i[base + 0], 
                        indexBuffer.i[base + 1], 
                        indexBuffer.i[base + 2]);

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

    LightData light = gLightBuffer.lights[0];

	const float eps = 1e-4;
    const vec3 lightEmittance = vec3(light.emittance);
    const float lightArea = getLightArea();
    const vec3 brdf_p = color / PI;

    if (gl_InstanceCustomIndexEXT == gLightBuffer.lightIndex[0]) {
        if (gPayload.depth == 0)
            gPayload.radiance = lightEmittance;
        else
            gPayload.radiance = vec3(0.0);
        return;
    }

	if (gPayload.depth >= gImguiParam.maxDepth) {
        gPayload.radiance = vec3(0.0);
		return;
    }

	//////////////////////////////////////////////////////////////// Direct Light
	vec3 tempRadianceD = vec3(0.0);
	uint numSampleByDepth = (gPayload.depth == 0 ? gImguiParam.numSamples : 1);

	for (int i=0; i < numSampleByDepth; ++i) {
		uint triangleIdx = binarySearchTriangleIdx(lightArea);

		vec3 pointOnTriangle, normalOnTriangle, pointOnTriangleWorld, normalOnTriangleWorld;
		uniformSamplePointOnTriangle(triangleIdx, 
									 pointOnTriangle, 
									 normalOnTriangle, 
									 pointOnTriangleWorld, 
									 normalOnTriangleWorld);

		float distToLight = length(pointOnTriangleWorld - worldPos);
		vec3 shadowRayDir = normalize(pointOnTriangleWorld - worldPos);

        gPayload.desiredPosition = vec3(0.0);
        traceRayEXT(
            topLevelAS,                          // topLevel
            gl_RayFlagsNoOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, // rayFlags
            0xff,                                // cullMask
            0, 1, SHADOW_MISS_IDX,               // sbtRecordOffset, sbtRecordStride, missIndex
            worldPos, 0.001f, shadowRayDir, 100.0,  // origin, tmin, direction, tmax
            0);                                  // payload

        float visibility = 0.0;    
        if (length(gPayload.desiredPosition - pointOnTriangleWorld) < 0.0001)
            visibility = 1.0;

		const vec3 r = pointOnTriangleWorld - worldPos;
		const float cos_q = max(dot(normalOnTriangleWorld, -shadowRayDir), 0.0);
        const float cos_p = max(dot(worldNormal, shadowRayDir), 0.0);
		const float P = cos_q / dot(r, r);

		tempRadianceD += brdf_p * cos_p * lightEmittance * visibility * P * lightArea;
	}
	tempRadianceD *= (1 / float(numSampleByDepth)); // average

	//////////////////////////////////////////////////////////////// Indirect Light

	uint tempDepth = gPayload.depth;
	vec3 tempRadianceI = vec3(0.0);

	for (uint i=0; i < numSampleByDepth; ++i)
	{
        float r3 = random(gPayload.rngState);
        float r4 = random(gPayload.rngState);
        vec2 seed = vec2(r3, r4);
        vec3 rayDir = RandomCosineHemisphere(worldNormal, seed);

		gPayload.depth += 1;
		traceRayEXT(
			topLevelAS,                         // topLevel
			gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
			0, 1, SHADOW_MISS_IDX,              // sbtRecordOffset, sbtRecordStride, missIndex
			worldPos, eps, rayDir, 100.0,  		// origin, tmin, direction, tmax
			0);                                 // gPayload 
		gPayload.depth = tempDepth;

        // const float cos_p = max(dot(worldNormal, rayDir), 1e-6);
        // const float pdf_p = cos_p / PI; // consine 샘플별 pdf
        tempRadianceI += color * gPayload.radiance;
	}
	tempRadianceI *= (1.0 / float(numSampleByDepth));
    
	gPayload.radiance = tempRadianceD + tempRadianceI;
}

#endif

#if BRUTE_FORCE_ENV_MAP_CLOSEST_HIT_SHADER
//=========================
//   BRUTE FORCE ENVIRONMENT MAP CLOSEST HIT SHADER
//=========================

layout( shaderRecordEXT ) buffer CustomData
{
   vec3 color;
};

layout(location = 0) rayPayloadInEXT RayPayload gPayload;
hitAttributeEXT vec2 attribs;

void main()
{
    ObjectDesc objDesc = gObjectDescs.desc[gl_InstanceCustomIndexEXT];

    IndexBuffer indexBuffer = IndexBuffer(objDesc.indexDeviceAddress);
    uint base = gl_PrimitiveID * 3u;
    uvec3 index = uvec3(indexBuffer.i[base + 0], 
                        indexBuffer.i[base + 1], 
                        indexBuffer.i[base + 2]);

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

    const vec3 brdf_p = color / PI;

    vec3 emit = vec3(0.0);
    vec3 temp = vec3(0.0);

    uint tempDepth = gPayload.depth;
    uint numSampleByDepth = (gPayload.depth == 0 ? gImguiParam.numSamples : 1);
    if (gPayload.depth < gImguiParam.maxDepth) {
        for (uint i=0; i < numSampleByDepth; ++i) {
                float r3 = random(gPayload.rngState);
                float r4 = random(gPayload.rngState);
                vec2 seed = vec2(r3, r4);
                vec3 rayDir = RandomCosineHemisphere(worldNormal, seed);
                gPayload.rayDirection = rayDir;

                gPayload.depth++;
                traceRayEXT(
                    topLevelAS,                         // topLevel
                    gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
                    0, 1, ENV_MISS_IDX,                 // sbtRecordOffset, sbtRecordStride, missIndex
                    worldPos, 0.0001, rayDir, 100.0,  	// origin, tmin, direction, tmax
                    0);                                 // payload 
                gPayload.depth = tempDepth;

            temp += color * gPayload.radiance;
        }
        temp *= 1.0 / float(numSampleByDepth);
    }

    gPayload.radiance = emit + temp;
}

#endif

#if NEE_ENV_MAP_CLOSEST_HIT_SHADER
//=========================
//   NEE ENVIRONMENT MAP CLOSEST HIT SHADER
//=========================

layout( shaderRecordEXT ) buffer CustomData
{
   vec3 color;
};

layout(location = 0) rayPayloadInEXT RayPayload gPayload;
hitAttributeEXT vec2 attribs;

vec3 sampleEnvDirection(uvec2 pixel, uint sampleIndex, uint depth, out float pdf)
{
    ivec2 texSize = textureSize(environmentMap, 0);
    uint width = texSize.x;
    uint height = texSize.y;

    uint x = random2( pixel, sampleIndex, depth, 1 ) & ( width - 1 );   // random % width
    uint y = random2( pixel, sampleIndex, depth, 0 ) & ( height - 1 );  // random % height
    vec4 pixelValue = imageLoad( envImportanceData, ivec2( x, y ) );
    pdf = pixelValue.w;

    vec3 dir = pixelValue.xyz;
    float rot = gImguiParam.envmapRotDeg * (PI / 180.0);
    dir = rotateY(-rot) * dir;

    // @TODO: pre-calculate on cpu
    return normalize( dir );
}

/////////////////////////////////////////////////////////////////////////////////////////////

void main()
{
    if (gPayload.bEnvMap)
    {
        gPayload.radiance = vec3(0.0); // visibility = 0.0
        return;
    }

    if( gPayload.depth >= gImguiParam.maxDepth )
    {
        gPayload.radiance = vec3( 0.0 );
        return;
    }

    ObjectDesc objDesc = gObjectDescs.desc[gl_InstanceCustomIndexEXT];

    IndexBuffer indexBuffer = IndexBuffer(objDesc.indexDeviceAddress);

    uint base = gl_PrimitiveID * 3u;
    uvec3 index = uvec3(indexBuffer.i[base + 0], 
                        indexBuffer.i[base + 1], 
                        indexBuffer.i[base + 2]);

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

    const vec3 brdf = color / PI;
    const vec3 viewDir = -gPayload.rayDirection;

	//////////////////////////////////////////////////////////////// Direct Light
	
	const float eps = 1e-4;
	vec3 tempRadianceD = vec3(0.0);
	uint numSampleByDepth = (gPayload.depth == 0 ? gImguiParam.numSamples : 1);

    gPayload.bEnvMap = true;

	for (uint i=0; i < numSampleByDepth; ++i)
	{
        float pdf_env;
        vec3 rayDir = sampleEnvDirection(gl_LaunchIDEXT.xy, i + gPayload.rngState, gPayload.depth, pdf_env);
        float cosTheta = max(0.0, dot(worldNormal, rayDir));
        
		gPayload.rayDirection = rayDir;
        gPayload.depth++;
		traceRayEXT(
			topLevelAS,                         // topLevel
			gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
			0, 1, ENV_MISS_IDX,                 // sbtRecordOffset, sbtRecordStride, missIndex
			worldPos, eps, rayDir, 100.0,  		// origin, tmin, direction, tmax
			0);                                 // gPayload 
			
        gPayload.depth--;

        float charFunc = 0.0;
        if (dot(viewDir, worldNormal) > 0.0) charFunc = 1.0;

        tempRadianceD += brdf * gPayload.radiance * cosTheta * charFunc / (pdf_env);
	}
	tempRadianceD *= (1.0 / float(numSampleByDepth)); 
	
	//////////////////////////////////////////////////////////////// Indirect Light

	vec3 tempRadianceI = vec3(0.0);

    gPayload.bEnvMap = false;

	for (uint i=0; i < numSampleByDepth; ++i)
	{
        float r3 = random(gPayload.rngState);
        float r4 = random(gPayload.rngState);
        vec2 seed = vec2(r3, r4);
        vec3 rayDir = RandomCosineHemisphere(worldNormal, seed);

		gPayload.rayDirection = rayDir;
		gPayload.depth++;
		traceRayEXT(
			topLevelAS,                         // topLevel
			gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
			0, 1, SHADOW_MISS_IDX,              // sbtRecordOffset, sbtRecordStride, missIndex
			worldPos, eps, rayDir, 100.0,  		// origin, tmin, direction, tmax
			0);                                 // gPayload 
		gPayload.depth--;

        // const float cos_p = max(dot(worldNormal, rayDir), 1e-6);
        // const float pdf_p = cos_p / PI; // consine 샘플별 pdf
        tempRadianceI += color * gPayload.radiance;
	}
	tempRadianceI *= (1.0 / float(numSampleByDepth)); 
    
	gPayload.radiance = tempRadianceD + tempRadianceI;
}

#endif

#if ANY_HIT_SHADER
//=========================
//   SHADOW MISS SHADER
//=========================
layout(location = 0) rayPayloadInEXT RayPayload gPayload;

void main()
{
    vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    gPayload.desiredPosition = worldPos;
}
#endif

#if SHADOW_MISS_SHADER
//=========================
//   SHADOW MISS SHADER
//=========================
layout(location = 0) rayPayloadInEXT RayPayload gPayload;
void main()
{
    gPayload.radiance = vec3(0.0);
}

#endif

#if LIGHT_ONLY_ENVIRONMENT_MISS_SHADER
//=========================
//   LIGHT ONLY ENVIRONMENT MISS SHADER
//=========================
layout(location = 0) rayPayloadInEXT RayPayload gPayload;

void main()
{
    gPayload.radiance = vec3(0.0);
}
#endif

#if ENV_MAP_ENVIRONMENT_MISS_SHADER
//=========================
//   ENV MAP ENVIRONMENT MISS SHADER
//=========================
layout(location = 0) rayPayloadInEXT RayPayload gPayload;

void main()
{
	if (gPayload.depth >= gImguiParam.maxDepth) {
        gPayload.radiance = vec3(0.0);
		return;
    }
    
    vec3 dir = normalize(gPayload.rayDirection);
    float rot = gImguiParam.envmapRotDeg * (PI / 180.0);
    dir = rotateY(rot) * dir; // envmap을 회전하는 것과 같음 (역방향 회전)
    vec2 uv = vec2(
        fract(atan(dir.z, dir.x) / (2.0 * PI)), // [−0.5,0.5) → [0,1)
        acos(clamp(dir.y, -1.0, 1.0)) / PI
    );
    gPayload.radiance = texture(environmentMap, uv).rgb;
}
#endif