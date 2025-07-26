#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_query : require
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_samplerless_texture_functions : enable
//#extension GL_EXT_descriptor_indexing : enable

#define PI 3.1415926535897932384626433832795
#define ENV_MISS_IDX 0
#define SHADOW_MISS_IDX 1
#define MIRROR_ROUGH 0.015
#define INF_CLAMP 1e30

#include "shaders/SharedStructs.glsl"
#include "shaders/Bindings.glsl"
#include "shaders/Sampler.glsl"
#include "shaders/NEELightSampling.glsl"
#include "shaders/BRDF.glsl"

#if RAY_GENERATION_SHADER
//=========================
//   RAY GENERATION SHADER
//=========================
layout(location = 0) rayPayloadEXT RayPayload gPayload;
void main()
{
    const vec3 cameraZ = g.cameraFront;
    const vec3 cameraX = normalize(cross(cameraZ, vec3(0.0, 1.0, 0.0)));
    const vec3 cameraY = -cross(cameraX, cameraZ);
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

    gPayload.radiance = vec3( 0.0 );
    gPayload.depth = 0;
    gPayload.transmissionDepth = 0;
    gPayload.desiredPosition = vec3( 0.0 );
    gPayload.rngState = rngState;
    gPayload.rayDirection = normalize(rayDir);
    gPayload.pdfBRDF = 0.0;
    gPayload.visibility = 1.0;

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

    // bool isinfcolor = isinf(finalfinalColor.x) || isinf(finalfinalColor.y) || isinf(finalfinalColor.z);
    // if (isinfcolor) finalfinalColor = vec3(1.0);
    // else finalfinalColor = vec3(0.0);

    // bool isnancolor = isnan(finalfinalColor.x) || isnan(finalfinalColor.y) || isnan(finalfinalColor.z);
    // if (isnancolor) finalfinalColor = vec3(1.0);
    // else finalfinalColor = vec3(0.0);

    imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( finalfinalColor, 1.0 ) );
}
#endif

#if BRUTE_FORCE_LIGHT_ONLY_CLOSEST_HIT_SHADER
//=========================
//   BRUTE FORCE LIGHT ONLY CLOSEST HIT SHADER
//=========================

layout( shaderRecordEXT ) buffer CustomData
{
   vec3 color;
   float metallic;
   float roughness;
} gCustomData;

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

    vec2 uv0 = attrBuf.a[index.x].uv.xy;
    vec2 uv1 = attrBuf.a[index.y].uv.xy;
    vec2 uv2 = attrBuf.a[index.z].uv.xy;
    vec2 uv = w * uv0 + u * uv1 + v * uv2;

    vec3 worldPos = (gl_ObjectToWorldEXT * vec4(position, 1.0)).xyz;
    vec3 worldNormal = normalize(transpose(inverse(mat3(gl_ObjectToWorldEXT))) * normal);

    LightData light = gLightBuffer.lights[0];

    const vec3 lightEmittance = vec3(light.emittance); // emittance per point

    MaterialParameter material = MaterialBuffer(objDesc.materialAddress).mat;
    const TextureParameter baseColorTexture = material._baseColorTexture;
    const vec4 baseColor = texture(sampler2D(textures[nonuniformEXT(baseColorTexture)], linearSampler), uv);

    const vec3 color = material._baseColorFactor.xyz * baseColor.xyz;
    const float metallic = clamp(gCustomData.metallic, 0.0, 1.0);
    const float roughness = clamp(gCustomData.roughness, MIRROR_ROUGH, 1.0);
    const float alpha = roughness * roughness;

    const float prob = mix(0.2, 0.8, roughness);
    const float probGGX = (1 - prob);
    const float probCos = prob;

    vec3 emit = vec3(0.0);
    if (gl_InstanceCustomIndexEXT == gLightBuffer.lightIndex[0])
        emit = lightEmittance;

    uint numSampleByDepth = (gPayload.depth == 0 ? gImguiParam.numSamples : 1);

    vec3 temp = vec3(0.0);
    if (gPayload.depth < gImguiParam.maxDepth) {
        for (uint i=0; i < numSampleByDepth; ++i) {
            const vec3 viewDir = -gPayload.rayDirection;
            vec3 rayDir = vec3(0.0);
            vec3 halfDir = vec3(0.0);

            float pdfSel = 1.0;
            float weight = 1.0;
            vec3 brdf = vec3(0.0);

            float a = random(gPayload.rngState);
            float r3 = random(gPayload.rngState);
            float r4 = random(gPayload.rngState);
            vec2 seed = vec2(r3, r4);

            bool isGGX = (a >= prob);
            float pdfGGXVal = 0.0;
            float pdfCosineVal = 0.0;

            if (isGGX) {
                mat3 TBN = computeTBN(worldNormal);
                vec3 viewDirLocal = normalize(transpose(TBN) * viewDir); // viewDir -> world2local
                vec3 halfDirLocal = sampleGGXVNDF(viewDirLocal, alpha, alpha, seed);
                halfDir = normalize(TBN * halfDirLocal);  // halfDir local -> world
                rayDir = reflect(-viewDir, halfDir);

                pdfGGXVal = pdfGGXVNDF(worldNormal, viewDir, halfDir, alpha);
                pdfCosineVal = max(dot(worldNormal, rayDir), 1e-6) / PI;
            } else {
                rayDir = RandomCosineHemisphere(worldNormal, seed);
                pdfCosineVal = max(dot(worldNormal, rayDir), 1e-6) / PI;

                halfDir = normalize(viewDir + rayDir);
                pdfGGXVal = pdfGGXVNDF(worldNormal, viewDir, halfDir, alpha);
            }

            pdfGGXVal = probGGX * pdfGGXVal;
            pdfCosineVal = probCos * pdfCosineVal;

            weight = isGGX ? powerHeuristic(pdfGGXVal, pdfCosineVal)
                            : powerHeuristic(pdfCosineVal, pdfGGXVal);
            pdfSel = isGGX ? pdfGGXVal : pdfCosineVal;
            brdf = calculateBRDF(worldNormal, viewDir, rayDir, halfDir, color, metallic, alpha);
            
            gPayload.rayDirection = rayDir;
            gPayload.depth++;
            traceRayEXT(
                topLevelAS,                         // topLevel
                gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
                0, 1, SHADOW_MISS_IDX,              // sbtRecordOffset, sbtRecordStride, missIndex
                worldPos, 0.0001, rayDir, 100.0,  	// origin, tmin, direction, tmax
                0);                                 // payload
            gPayload.depth--;
            
            vec3 reflectedRadiance = gPayload.radiance;

            float transmissionFactor = getTransmissionFactor(material, gImguiParam.transmissionFactor);
            vec3 transmittedRadiance = vec3(0.0);
            
            if (transmissionFactor > 0.0) {
                if (gPayload.transmissionDepth >= 12) {
                    vec3 refractedDir = calculateRefractedDirection(-viewDir, worldNormal, material._ior);
                    transmittedRadiance = getEmitFromEnvmap(refractedDir) * color * transmissionFactor * 0.3;
                } else {
                    float ior = material._ior;
                    if (ior <= 0.0) ior = 1.5;
                    
                    vec3 refractedDir = calculateRefractedDirection(-viewDir, worldNormal, ior);
                
                float fresnelReflectance = calculateFresnelReflectance(viewDir, worldNormal, ior);
                float transmissionCoeff = calculateTransmissionCoeff(viewDir, worldNormal, ior);
                
                                gPayload.rayDirection = refractedDir;
                gPayload.depth++;
                gPayload.transmissionDepth++;
                traceRayEXT(
                    topLevelAS,
                    gl_RayFlagsOpaqueEXT, 0xff,
                    0, 1, SHADOW_MISS_IDX,
                    worldPos, 0.0001, refractedDir, 100.0,
                    0);
                gPayload.depth--;
                gPayload.transmissionDepth--;
                
                    transmittedRadiance = gPayload.radiance * color * transmissionFactor * transmissionCoeff;
                }
            }

            const float cos_p = max(dot(worldNormal, rayDir), 1e-6);
            temp += brdf * reflectedRadiance * cos_p * weight / pdfSel;
            temp += transmittedRadiance;
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
   float metallic;
   float roughness;
} gCustomData;

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

	const float eps = 1e-4;
    const vec3 lightEmittance = vec3(light.emittance);
    const float lightArea = getLightArea();

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

    const vec3 color = gCustomData.color;
    const float metallic = clamp(gCustomData.metallic, 0.0, 1.0);
    const float roughness = clamp(gCustomData.roughness, MIRROR_ROUGH, 1.0);
    const float alpha = roughness * roughness;

    const float prob = mix(0.2, 0.8, roughness);
    const float probGGX = (1 - prob);
    const float probCos = prob;

    const vec3 viewDir = -gPayload.rayDirection;


    MaterialParameter material = MaterialBuffer(objDesc.materialAddress).mat;

	//////////////////////////////////////////////////////////////// Direct Light

	vec3 tempRadianceD = vec3(0.0);
	const uint numSampleByDepth = (gPayload.depth == 0 ? gImguiParam.numSamples : 1);
	for (int i=0; i < numSampleByDepth; ++i) {
        // NEE Sampling
		uint triangleIdx = binarySearchTriangleIdx(lightArea, gPayload.rngState);

		vec3 pointOnTriangle, normalOnTriangle, pointOnTriangleWorld, normalOnTriangleWorld;
		uniformSamplePointOnTriangle(triangleIdx,
									 pointOnTriangle,
									 normalOnTriangle,
									 pointOnTriangleWorld,
									 normalOnTriangleWorld,
                                     gPayload.rngState);

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
		const float cos_q = max(dot(normalOnTriangleWorld, -shadowRayDir), 1e-6);
        const float cos_p = max(dot(worldNormal, shadowRayDir), 1e-6);
        const float pdfLight = dot(r, r) / (cos_q * lightArea);

        vec3 halfDir = normalize(viewDir + shadowRayDir);
        vec3 brdf = calculateBRDF(worldNormal, viewDir, shadowRayDir, halfDir, color, metallic, alpha);
        
        float pdfGGX = pdfGGXVNDF(worldNormal, viewDir, halfDir, alpha);
        float pdfCos = cos_p / PI;
        float pdfBRDF = probGGX * pdfGGX + probCos * pdfCos;

        float w = powerHeuristic(pdfLight, pdfBRDF);

        tempRadianceD += brdf * lightEmittance * visibility * cos_p * w / pdfLight;
	}
	tempRadianceD *= (1 / float(numSampleByDepth)); // average

	//////////////////////////////////////////////////////////////// Indirect Light

	vec3 tempRadianceI = vec3(0.0);

	for (uint i=0; i < numSampleByDepth; ++i)
	{
        vec3 rayDir = vec3(0.0);
        vec3 halfDir = vec3(0.0);

        float pdfSel = 1.0;
        float weight = 1.0;
        vec3 brdf = vec3(0.0);

        float a = random(gPayload.rngState);
        float r3 = random(gPayload.rngState);
        float r4 = random(gPayload.rngState);
        vec2 seed = vec2(r3, r4);

        bool isGGX = (a >= prob);
        float pdfGGXVal = 0.0;
        float pdfCosineVal = 0.0;

        if (isGGX) {
            mat3 TBN = computeTBN(worldNormal);
            vec3 viewDirLocal = normalize(transpose(TBN) * viewDir); // viewDir -> world2local
            vec3 halfDirLocal = sampleGGXVNDF(viewDirLocal, alpha, alpha, seed);
            halfDir = normalize(TBN * halfDirLocal);  // halfDir local -> world
            rayDir = reflect(-viewDir, halfDir);

            pdfGGXVal = pdfGGXVNDF(worldNormal, viewDir, halfDir, alpha);
            pdfCosineVal = max(dot(worldNormal, rayDir), 1e-6) / PI;
        }
        else {
            rayDir = RandomCosineHemisphere(worldNormal, seed);
            pdfCosineVal = max(dot(worldNormal, rayDir), 1e-6) / PI;

            halfDir = normalize(viewDir + rayDir);
            pdfGGXVal = pdfGGXVNDF(worldNormal, viewDir, halfDir, alpha);
        }

        pdfGGXVal = probGGX * pdfGGXVal;
        pdfCosineVal = probCos * pdfCosineVal;

        weight = isGGX ? powerHeuristic(pdfGGXVal, pdfCosineVal)
                        : powerHeuristic(pdfCosineVal, pdfGGXVal);

        pdfSel = isGGX ? pdfGGXVal : pdfCosineVal;
        brdf = calculateBRDF(worldNormal, viewDir, rayDir, halfDir, color, metallic, alpha);
        
        gPayload.rayDirection = rayDir;
        gPayload.depth++;
        traceRayEXT(
            topLevelAS,                         // topLevel
            gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
            0, 1, SHADOW_MISS_IDX,              // sbtRecordOffset, sbtRecordStride, missIndex
            worldPos, 0.0001, rayDir, 100.0,  	// origin, tmin, direction, tmax
            0);                                 // payload
        gPayload.depth--;
        
        vec3 reflectedRadiance = gPayload.radiance;

        float transmissionFactor = getTransmissionFactor(material, gImguiParam.transmissionFactor);
        vec3 transmittedRadiance = vec3(0.0);
        
        if (transmissionFactor > 0.0) {
            float ior = material._ior;
            if (ior <= 0.0) ior = 1.5;
            
            vec3 refractedDir = calculateRefractedDirection(-viewDir, worldNormal, ior);
            
            float transmissionCoeff = calculateTransmissionCoeff(viewDir, worldNormal, ior);
            
                        gPayload.rayDirection = refractedDir;
            gPayload.depth++;
            traceRayEXT(
                topLevelAS,
                gl_RayFlagsOpaqueEXT, 0xff,
                0, 1, SHADOW_MISS_IDX,
                worldPos, 0.0001, refractedDir, 100.0,
                0);
            gPayload.depth--;
            
            transmittedRadiance = gPayload.radiance * color * transmissionFactor * transmissionCoeff;
        }

        const float cos_p = max(dot(worldNormal, rayDir), 1e-6);
        tempRadianceI += brdf * reflectedRadiance * cos_p * weight / pdfSel;
        tempRadianceI += transmittedRadiance;
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
   float metallic;
   float roughness;
} gCustomData;

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

    const vec3 color = gCustomData.color;
    const float metallic = clamp(gCustomData.metallic, 0.0, 1.0);
    const float roughness = clamp(gCustomData.roughness, MIRROR_ROUGH, 1.0);
    const float alpha = roughness * roughness;
    const vec3 viewDir = -gPayload.rayDirection;

    // Adaptive probability based on roughness
    const float prob = mix(0.2, 0.8, roughness);
    const float probGGX = (1 - prob);
    const float probCos = prob;

    MaterialParameter material = MaterialBuffer(objDesc.materialAddress).mat;

    vec3 temp = vec3(0.0);
    uint numSampleByDepth = (gPayload.depth == 0 ? gImguiParam.numSamples : 1);
    if (gPayload.depth < gImguiParam.maxDepth) {
        for (uint i=0; i < numSampleByDepth; ++i) {
            vec3 rayDir = vec3(0.0);
            vec3 halfDir = vec3(0.0);

            float a = random(gPayload.rngState);
            float r3 = random(gPayload.rngState);
            float r4 = random(gPayload.rngState);
            vec2 seed = vec2(r3, r4);

            bool isGGX = (a >= prob);
            float pdfGGXVal = 0.0;
            float pdfCosineVal = 0.0;

            if (isGGX) {
                mat3 TBN = computeTBN(worldNormal);
                vec3 viewDirLocal = normalize(transpose(TBN) * viewDir);
                vec3 halfDirLocal = sampleGGXVNDF(viewDirLocal, alpha, alpha, seed);
                halfDir = normalize(TBN * halfDirLocal);
                rayDir = reflect(-viewDir, halfDir);

                pdfGGXVal = pdfGGXVNDF(worldNormal, viewDir, halfDir, alpha);
                pdfCosineVal = max(dot(worldNormal, rayDir), 1e-6) / PI;
            }
            else {
                rayDir = RandomCosineHemisphere(worldNormal, seed);
                pdfCosineVal = max(dot(worldNormal, rayDir), 1e-6) / PI;

                halfDir = normalize(viewDir + rayDir);
                pdfGGXVal = pdfGGXVNDF(worldNormal, viewDir, halfDir, alpha);
            }

            const float pdfBRDF = probGGX * pdfGGXVal + probCos * pdfCosineVal;
            vec3 brdf = calculateBRDF(worldNormal, viewDir, rayDir, halfDir, color, metallic, alpha);
            

            gPayload.rayDirection = rayDir;
            gPayload.pdfBRDF = pdfBRDF;
            gPayload.depth++;
            traceRayEXT(
                topLevelAS,                         // topLevel
                gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
                0, 1, ENV_MISS_IDX,                 // sbtRecordOffset, sbtRecordStride, missIndex
                worldPos, 0.0001, rayDir, 100.0,  	// origin, tmin, direction, tmax
                0);                                 // payload
            gPayload.depth--;
            
            vec3 reflectedRadiance = gPayload.radiance;

            float transmissionFactor = getTransmissionFactor(material, gImguiParam.transmissionFactor);
            vec3 transmittedRadiance = vec3(0.0);
            
            if (transmissionFactor > 0.0) {
                if (gPayload.transmissionDepth >= 12) {
                    vec3 refractedDir = calculateRefractedDirection(-viewDir, worldNormal, material._ior);
                    transmittedRadiance = getEmitFromEnvmap(refractedDir) * color * transmissionFactor * 0.3;
                } else {
                    float ior = material._ior;
                    if (ior <= 0.0) ior = 1.5;
                    
                    vec3 refractedDir = calculateRefractedDirection(-viewDir, worldNormal, ior);
                
                    float transmissionCoeff = calculateTransmissionCoeff(viewDir, worldNormal, ior);
                
                                gPayload.rayDirection = refractedDir;
                gPayload.depth++;
                traceRayEXT(
                    topLevelAS,
                    gl_RayFlagsOpaqueEXT, 0xff,
                    0, 1, ENV_MISS_IDX,
                    worldPos, 0.0001, refractedDir, 100.0,
                    0);
                gPayload.depth--;
                
                    transmittedRadiance = gPayload.radiance * color * transmissionFactor * transmissionCoeff;
                }
            }

            const float cos_p = max(dot(worldNormal, rayDir), 1e-6);
            temp += brdf * reflectedRadiance * cos_p / pdfBRDF;
            temp += transmittedRadiance;
        }
        temp *= 1.0 / float(numSampleByDepth);
    }
    gPayload.radiance = temp;
}

#endif

#if NEE_ENV_MAP_CLOSEST_HIT_SHADER
//=========================
//   NEE ENVIRONMENT MAP CLOSEST HIT SHADER
//=========================

layout( shaderRecordEXT ) buffer CustomData
{
   vec3 color;
   float metallic;
   float roughness;
} gCustomData;

layout(location = 0) rayPayloadInEXT RayPayload gPayload;
hitAttributeEXT vec2 attribs;

/////////////////////////////////////////////////////////////////////////////////////////////

void main()
{
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

    vec2 uv0 = attrBuf.a[index.x].uv.xy;
    vec2 uv1 = attrBuf.a[index.y].uv.xy;
    vec2 uv2 = attrBuf.a[index.z].uv.xy;
    vec2 uv = w * uv0 + u * uv1 + v * uv2;

    vec3 worldPos = (gl_ObjectToWorldEXT * vec4(position, 1.0)).xyz;
    vec3 worldNormal = normalize(transpose(inverse(mat3(gl_ObjectToWorldEXT))) * normal);

    const float eps = 1e-4;

    MaterialParameter material = MaterialBuffer(objDesc.materialAddress).mat;
    const TextureParameter baseColorTexture = material._baseColorTexture;
    const vec4 baseColor = texture(sampler2D(textures[nonuniformEXT(baseColorTexture)], linearSampler), uv);

    const vec3 color = material._baseColorFactor.xyz * baseColor.xyz;
    const float metallic = clamp(gCustomData.metallic, 0.0, 1.0);
    const float roughness = clamp(gCustomData.roughness, MIRROR_ROUGH, 1.0);
    const float alpha = roughness * roughness;

    const float prob = mix(0.2, 0.8, roughness);
    const float probGGX = (1 - prob);
    const float probCos = prob;

    const vec3 viewDir = -gPayload.rayDirection;

	//////////////////////////////////////////////////////////////// Direct Light

	vec3 tempRadianceD = vec3(0.0);
	uint numSampleByDepth = (gPayload.depth == 0 ? gImguiParam.numSamples : 1);

	for (uint i=0; i < numSampleByDepth; ++i)
	{
        float pdfEnv;
        vec3 rayDir = sampleEnvDirection(gPayload.rngState, pdfEnv);

		gPayload.rayDirection = rayDir;
        gPayload.visibility = 1.0;
		traceRayEXT(
			topLevelAS,                         // topLevel
			gl_RayFlagsNoOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT,    // rayFlags
            0xff,                               // cullMask
			0, 1, SHADOW_MISS_IDX,              // sbtRecordOffset, sbtRecordStride, missIndex // miss shader should do nothing
			worldPos, eps, rayDir, 100.0,  		// origin, tmin, direction, tmax
			0);                                 // gPayload

        const vec3 emit = getEmitFromEnvmap(rayDir);

        float cos_p = max(dot(worldNormal, rayDir), 1e-6);
        vec3 halfDir = normalize(viewDir + rayDir);
        vec3 brdf = calculateBRDF(worldNormal, viewDir, rayDir, halfDir, color, metallic, alpha);
        
        float pdfGGX = pdfGGXVNDF(worldNormal, viewDir, halfDir, alpha);
        float pdfCos = cos_p / PI;
        float pdfBRDF = probGGX * pdfGGX + probCos * pdfCos;

        float w = powerHeuristic(pdfEnv, pdfBRDF);

        float charFunc = 0.0;
        if (dot(viewDir, worldNormal) > 0.0) charFunc = 1.0;
        
        float transmissionFactor = getTransmissionFactor(material, gImguiParam.transmissionFactor);
        if (transmissionFactor > 0.0) {
            vec3 transmissionDir = -viewDir;
            vec3 transmittedEnvColor = getEmitFromEnvmap(transmissionDir);
            vec3 transmittedRadiance = transmittedEnvColor * color * transmissionFactor;
            
            brdf = mix(brdf, vec3(0.1), transmissionFactor);
            tempRadianceD += transmittedRadiance * gPayload.visibility * charFunc * w / pdfEnv;
        }

        tempRadianceD += brdf * emit * cos_p * gPayload.visibility * charFunc * w / pdfEnv;
	}
	tempRadianceD *= (1.0 / float(numSampleByDepth));

	//////////////////////////////////////////////////////////////// Indirect Light

	vec3 tempRadianceI = vec3(0.0);

	for (uint i=0; i < numSampleByDepth; ++i)
	{
        vec3 rayDir = vec3(0.0);
        vec3 halfDir = vec3(0.0);

        float a = random(gPayload.rngState);
        float r3 = random(gPayload.rngState);
        float r4 = random(gPayload.rngState);
        vec2 seed = vec2(r3, r4);

        bool isGGX = (a >= prob);
        float pdfGGXVal = 0.0;
        float pdfCosineVal = 0.0;
        float cos_p = 0.0;

        if (isGGX) {
            mat3 TBN = computeTBN(worldNormal);
            vec3 viewDirLocal = normalize(transpose(TBN) * viewDir); // viewDir -> world2local
            vec3 halfDirLocal = sampleGGXVNDF(viewDirLocal, alpha, alpha, seed);
            halfDir = normalize(TBN * halfDirLocal);  // halfDir local -> world
            rayDir = reflect(-viewDir, halfDir);
            cos_p = max(dot(worldNormal, rayDir), 1e-6);

            pdfGGXVal = pdfGGXVNDF(worldNormal, viewDir, halfDir, alpha);
            pdfCosineVal = cos_p / PI;
        }
        else {
            rayDir = RandomCosineHemisphere(worldNormal, seed);
            cos_p = max(dot(worldNormal, rayDir), 1e-6);
            pdfCosineVal = cos_p / PI;

            halfDir = normalize(viewDir + rayDir);
            pdfGGXVal = pdfGGXVNDF(worldNormal, viewDir, halfDir, alpha);
        }

        pdfGGXVal = probGGX * pdfGGXVal;
        pdfCosineVal = probCos * pdfCosineVal;

        const float weight = isGGX ? powerHeuristic(pdfGGXVal, pdfCosineVal)
                                   : powerHeuristic(pdfCosineVal, pdfGGXVal);

        const float pdfBRDF = isGGX ? pdfGGXVal : pdfCosineVal;
        vec3 brdf = calculateBRDF(worldNormal, viewDir, rayDir, halfDir, color, metallic, alpha);
        
		gPayload.rayDirection = rayDir;
        gPayload.pdfBRDF = pdfBRDF;
		gPayload.depth++;
		traceRayEXT(
			topLevelAS,                         // topLevel
			gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
			0, 1, ENV_MISS_IDX,                 // sbtRecordOffset, sbtRecordStride, missIndex
			worldPos, eps, rayDir, 100.0,  		// origin, tmin, direction, tmax
			0);                                 // gPayload
		gPayload.depth--;
		
		vec3 reflectedRadiance = gPayload.radiance;

        float transmissionFactor = getTransmissionFactor(material, gImguiParam.transmissionFactor);
        vec3 transmittedRadiance = vec3(0.0);
        
        if (transmissionFactor > 0.0) {
            float ior = material._ior;
            if (ior <= 0.0) ior = 1.5;
            
            vec3 refractedDir = calculateRefractedDirection(-viewDir, worldNormal, ior);
            
            float transmissionCoeff = calculateTransmissionCoeff(viewDir, worldNormal, ior);
            
                        gPayload.rayDirection = refractedDir;
            gPayload.depth++;
            traceRayEXT(
                topLevelAS,
                gl_RayFlagsOpaqueEXT, 0xff,
                0, 1, ENV_MISS_IDX,
                worldPos, eps, refractedDir, 100.0,
                0);
            gPayload.depth--;
            
            transmittedRadiance = gPayload.radiance * color * transmissionFactor * transmissionCoeff;
        }

        tempRadianceI += brdf * reflectedRadiance * cos_p * weight / pdfBRDF;
        tempRadianceI += transmittedRadiance; // radiance weighted for the last bounce
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
    // for local lights
    vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    gPayload.desiredPosition = worldPos;

    // for envmap
    gPayload.visibility = 0.0;
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

    const vec3 Le = getEmitFromEnvmap(gPayload.rayDirection);

    float weight = 1.0;
    // get envmap pdf for MIS
    if (gPayload.depth > 0)
    {
        const vec2 uv = getUVfromRay(gPayload.rayDirection);
        const float pdfEnv = getEnvPdf(uv.x, uv.y);
        weight = powerHeuristic(gPayload.pdfBRDF, pdfEnv);
    }
    gPayload.radiance = weight * Le;
}
#endif