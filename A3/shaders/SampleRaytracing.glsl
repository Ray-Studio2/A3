#if SHADOW_MISS_SHADER
//=========================
//	SHADOW MISS SHADER
//=========================
layout( location = 1 ) rayPayloadInEXT bool isShadowed;

void main()
{
    isShadowed = false;  // 그림자에 가리지 않음
}
#endif

#if CLOSEST_HIT_SHADER
// ... existing code ...

void main()
{
    // ... existing code until worldPos calculation ...

    // Initialize random seed based on pixel position
    uint seed = uint(gl_LaunchIDEXT.x) * uint(1973) + uint(gl_LaunchIDEXT.y) * uint(9277);
    
    // Sample new direction based on sampling mode
    vec3 rayDir;
    if (g.samplingMode == 0) {
        // Uniform sampling
        rayDir = uniformSampleSphere(seed);
        if (dot(rayDir, worldNormal) < 0.0) rayDir = -rayDir;
    } else {
        // Cosine sampling
        rayDir = cosineSampleHemisphere(seed, worldNormal);
    }

    // Calculate PDF and lighting contribution
    float cosTheta = max(dot(worldNormal, rayDir), 0.0);
    float pdf = (g.samplingMode == 0) ? (1.0 / (4.0 * 3.14159265)) : (cosTheta / 3.14159265);

    // Add ambient light
    vec3 ambient = vec3(0.1);
    
    // Trace shadow ray
    bool isShadowed = true;
    traceRayEXT(
        topLevelAS,
        gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT,
        0xFF, 0, 0, 1,
        worldPos, 0.001, rayDir, 100.0,
        1
    );

    vec3 finalColor = ambient;
    if (!isShadowed) {
        if (g.samplingMode == 0) {
            // Uniform sampling
            finalColor += color * cosTheta / pdf;
        } else {
            // Cosine sampling (PDF already includes cosTheta)
            finalColor += color;
        }
    }
    
    hitValue = finalColor;
}
#endif 