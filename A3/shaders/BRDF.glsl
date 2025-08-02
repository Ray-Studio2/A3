//////////////////////////////////////////////////////////////// BRDF methods

vec3 Schlick_F(vec3 viewDir, vec3 halfDir, vec3 F0)
{
    float dotHV = max(dot(viewDir, halfDir), 0.0);
    return F0 + (1.0 - F0) * pow(1.0 - dotHV, 5.0);
}

float GGX_G1(vec3 normal, vec3 dir, float alpha) // G1
{
    float r = (alpha + 1.0);
    float k = (r * r) / 8.0; // direct light
    // float k = (alpha * alpha) / 2.0; // IBL

    float dotN = max(dot(normal, dir), 1e-6); // 테두리가 0으로 수렴되는 걸 막아줌
    float num = dotN;
    float denom = dotN * (1.0 - k) + k;
    return num / denom;
}

// 빛 방향, 시선 방향 모두 고려
float GGX_G2(vec3 normal, vec3 viewDir, vec3 lightDir, float alpha) // G2
{
    float ggx_1 = GGX_G1(normal, viewDir, alpha);
    float ggx_2 = GGX_G1(normal, lightDir, alpha);
    return ggx_1 * ggx_2;
}

float GGX_D(vec3 normal, vec3 halfDir, float alpha)
{
    float a2 = alpha * alpha;
    float dotNH = max(dot(normal, halfDir), 1e-6);
    float dotNH2 = dotNH * dotNH;

    float num = a2;
    float denom = (dotNH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

vec3 calculateRefractedDirection(vec3 incidentDir, vec3 normal, float ior)
{
    float eta = 1.0 / ior; // Relative IOR (air to material)
    
    // Check if we're exiting the material (dot product < 0 means inside)
    if (dot(incidentDir, normal) > 0.0) {
        eta = ior; // Material to air
        normal = -normal; // Flip normal for exit
    }
    
    vec3 refracted = refract(incidentDir, normal, eta);
    
    // Handle total internal reflection
    if (length(refracted) == 0.0) {
        return reflect(incidentDir, normal);
    }
    
    return refracted;
}

float calculateFresnelReflectance(vec3 viewDir, vec3 normal, float ior)
{
    float cosTheta = abs(dot(viewDir, normal));
    float F0 = pow((1.0 - ior) / (1.0 + ior), 2.0);
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float calculateTransmissionCoeff(vec3 viewDir, vec3 normal, float ior)
{
    return 1.0 - calculateFresnelReflectance(viewDir, normal, ior);
}

// Separate alpha blending and transmission handling
bool shouldUseAlphaBlending(float alpha, float transmissionFactor)
{
    // Use alpha blending if alpha < 1.0 and no transmission
    return alpha < 1.0 && transmissionFactor <= 0.0;
}

bool shouldUseTransmission(float transmissionFactor)
{
    return transmissionFactor > 0.0;
}

// Alpha blending for traditional transparency
vec3 calculateAlphaBlending(vec3 surfaceColor, vec3 backgroundColor, float alpha)
{
    return mix(backgroundColor, surfaceColor, alpha);
}

// Precomputed transmission properties
struct TransmissionProperties
{
    float factor;
    float ior;
    float fresnelReflectance;
    float transmissionCoeff;
    bool isActive;
};

// Get precomputed transmission properties
TransmissionProperties getTransmissionProperties(MaterialParameter material, vec2 uv, vec3 viewDir, vec3 normal, float globalFactor)
{
    TransmissionProperties props;
    
    // Calculate effective transmission factor
    props.factor = material._transmissionFactor;
    if (material._transmissionTexture != 0xfffffff) {
        vec4 transmissionTexture = texture(sampler2D(textures[nonuniformEXT(material._transmissionTexture)], linearSampler), uv);
        props.factor *= transmissionTexture.r; // Use red channel for transmission
    }
    props.factor *= globalFactor;
    
    props.isActive = props.factor > 0.0;
    
    if (props.isActive) {
        // Use material's IOR
        props.ior = material._ior;
        
        // Precompute Fresnel values once
        props.fresnelReflectance = calculateFresnelReflectance(viewDir, normal, props.ior);
        props.transmissionCoeff = calculateTransmissionCoeff(viewDir, normal, props.ior);
    } else {
        props.ior = 1.0;
        props.fresnelReflectance = 0.0;
        props.transmissionCoeff = 0.0;
    }
    
    return props;
}

// Get transmission factor (simplified version without texture)
float getTransmissionFactor(MaterialParameter material, float globalFactor)
{
    // If global factor is set, use it as override (for debugging/testing)
    if (globalFactor > 0.0) {
        return globalFactor;
    }
    
    // Otherwise use material properties
    return material._transmissionFactor;
}

// Get transmission factor with texture support
float getTransmissionFactor(MaterialParameter material, vec2 uv)
{
    // // If global factor is set, use it as override (for debugging/testing)
    // if (globalFactor > 0.0) {
    //     return globalFactor;
    // }
    
    // Otherwise use material properties
    float transmissionFactor = material._transmissionFactor;
    
    // Sample transmission texture if available (assumes white texture has specific index)
    if (material._transmissionTexture != 0xfffffff) {
        vec4 transmissionTexture = texture(sampler2D(textures[nonuniformEXT(material._transmissionTexture)], linearSampler), uv);
        transmissionFactor *= transmissionTexture.r; // Use red channel for transmission
    }
    
    return transmissionFactor;
}

// Transmission BRDF for thin-surface materials (KHR_materials_transmission)
vec3 calculateTransmissionBRDF(vec3 normal, vec3 viewDir, vec3 color, float transmission)
{
    // For thin-surface transmission, reduce the surface reflection
    // The actual transmission is handled in the ray tracing logic
    float dotNV = abs(dot(normal, viewDir));
    vec3 F0 = vec3(0.04);
    vec3 fresnel = Schlick_F(viewDir, normal, F0);
    
    // Reduce surface contribution, allowing more light to pass through
    return color * (1.0 - fresnel) * transmission * 0.1; // Reduced surface contribution
}

// ======== using sheen material ========
// ======================================

// imwageworks county and kulla(2017) sheen shadowing masking function...
float ChalieShadowMaskingNumericallyFit(float x, float alpha_g)
{
    float one_minus_alpha_sq = (1.0 - alpha_g) * (1.0 - alpha_g);
    float a = mix(21.5473, 25.3245, one_minus_alpha_sq);
    float b = mix(3.82987, 3.32435, one_minus_alpha_sq);
    float c = mix(0.19823, 0.16801, one_minus_alpha_sq);
    float d = mix(-1.97760, -1.27393, one_minus_alpha_sq);
    float e = mix(-4.32054, -4.85967, one_minus_alpha_sq);

    return a / (1.0 + b * pow(x,c)) + d * x + e;
}

float LamdaSheen(float cos_theta, float alpha_g)
{
    return abs(cos_theta) < 0.5 ? exp(ChalieShadowMaskingNumericallyFit(cos_theta, alpha_g))
                                : exp(2.0 * ChalieShadowMaskingNumericallyFit(0.5, alpha_g) - ChalieShadowMaskingNumericallyFit(1.0 - cos_theta, alpha_g));
}

float CharlieV(float dotNV, float dotNL, float alpha_g)
{
    return 1.0 / ((1.0 + LamdaSheen(dotNV, alpha_g) + LamdaSheen(dotNL, alpha_g)) * (4.0 * dotNV * dotNL));
}

float CharlieD(float alpha_g, float dotNH) { // imageworks의 sheen 분포
    float inv_r = 1 / alpha_g;
    float cos2h = dotNH * dotNH;
    float sin2h = 1 - cos2h; // half vector가 normal에서 얼마나 떨어졌는가?

    // sheen 재질의 실린더 크기를 정함
    return (2 + inv_r) * pow(sin2h, inv_r * 0.5) / (2 * PI);
}

float Ashikhmin(float dotNV, float dotNL, float alpha_g)
{
    return 1.0 / (4 * (dotNL + dotNV - dotNL * dotNV));
}

float Max3(vec3 v)
{
    return max(max(v.x, v.y), v.z);
}

float E(float dotNV, float sheenRoughness)
{
    vec2 uv = vec2(clamp(dotNV, 0.0, 1.0), clamp(sheenRoughness, 0.0, 1.0));
    return texture(sampler2D(textures[nonuniformEXT(1)], linearSampler), uv).b;
}
// ======================================

vec3 calculateBRDF(vec3 normal, vec3 viewDir, vec3 lightDir, vec3 halfDir, BasicMaterial material, bool isGLTF) // Cook-Torrence
{
    const vec3 color            = material.baseColor;
    const float metallic        = material.metallic;
    const float alpha           = material.alpha;
    const vec3 sheenColor       = material.sheenColor;
    const float sheenRoughness  = material.sheenRoughness;

    float dotNV = max(dot(normal, viewDir), 1e-5);
    float dotNL = max(dot(normal, lightDir), 1e-5);

    const vec3 F0 = mix(vec3(0.04), color, metallic);

    float ndf = GGX_D(normal, halfDir, alpha);
    float geometry = GGX_G2(normal, viewDir, lightDir, alpha);
    vec3 fresnel = Schlick_F(viewDir, halfDir, F0);

    vec3 num = min(ndf * geometry * fresnel, INF_CLAMP);
    float denom = 4.0 * dotNV * dotNL;
    vec3 f_spec = num / max(denom, 1e-6);

    vec3 f_diff = (1 - fresnel) * (1.0 - metallic) * color * (1/PI);    // according to gltf spec
    
    vec3 brdf = f_diff + f_spec; 

    if (isGLTF) {
        // ======== using sheen material ========
        if(length(sheenColor) <= 0.0 || sheenRoughness <= 0)
            return brdf;
        
        float sheenBRDF = 0; 
        float dotNH = max(dot(normal, halfDir), 1e-5); // half vector과 normal이 얼마나 가까운가?
        float alpha_g = sheenRoughness * sheenRoughness; // roughness의 선형적 제어를 위함
        float Ds = CharlieD(alpha_g, dotNH);
        float Vs = CharlieV(dotNV, dotNL, alpha_g);

        sheenBRDF = Vs * Ds;

        float sheenAldedoScaling = min(1.0 - Max3(sheenColor) * E(alpha_g, dotNV)
                                    , 1.0 - Max3(sheenColor) * E(alpha_g, dotNL));

        //float sheenAldedoScaling = 1.0 - Max3(sheenColorFactor) * E(dotNV, alpha_g);
        // ======================================

        brdf = sheenColor * sheenBRDF + brdf * sheenAldedoScaling;
    }

    return brdf;
}

// vec3 calculateW(vec3 normal, vec3 viewDir, vec3 lightDir, vec3 halfDir, vec3 color, float metallic, float alpha)
// {
//     float dotHV = max(dot(viewDir, halfDir), 1e-6);

//     const vec3 F0 = mix(vec3(0.04), color, metallic);

//     vec3 F = Schlick_F(viewDir, halfDir, F0);
//     float G2 = GGX_G2(normal, viewDir, lightDir, alpha);
//     float G1 = GGX_G1(normal, viewDir, alpha);

//     return F * G2 / G1;
// }

// TODO: v는 local view vector, rayDir를 표면 normal에 따라 local로 변경
vec3 sampleGGXVNDF(vec3 v, float alpha_x, float alpha_y, vec2 seed) 
{
    float u1 = seed.x;
    float u2 = seed.y;

    // transforming the view direction to the hemisphere configuration
    vec3 v_h = normalize(vec3(alpha_x * v.x, alpha_y * v.y, v.z));

    float lenSq = v_h.x * v_h.x + v_h.y * v_h.y;
    vec3 T1;
    if (lenSq > 0.0) {
        float invsqrt = 1 / sqrt(lenSq);
        T1 = vec3(-v_h.y, v_h.x, 0.0) * invsqrt;
    } else {
        T1 = vec3(1.0, 0.0, 0.0);
    }
    // T1 is perpendicular to the view vector = normalize(Z x V_h)
    // when v = (0,0,1), cross product is 0, so we pick x = (1,0,0) for a safe approach
    vec3 T2 = cross(v_h, T1);

    float r = sqrt(u1);
    float phi = 2.0 * PI * u2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + v_h.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    vec3 n_h = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * v_h;
    vec3 n_e = normalize(vec3(alpha_x * n_h.x, alpha_y * n_h.y, max(0.0, n_h.z)));
    return n_e; // h-vector
}

float pdfGGXVNDF(vec3 normal, vec3 viewDir, vec3 halfDir, float alpha)
{
    float G = GGX_G1(normal, viewDir, alpha);
    float D = GGX_D(normal, halfDir, alpha);

    float dotHV = max(dot(viewDir, halfDir), 1e-6);
    float dotHN = max(dot(halfDir, normal), 1e-6);
    float dotNV = max(dot(viewDir, normal), 1e-6);

    float num = G * D * dotHV;
    float denom = dotNV;
    float pdf_h = num / denom;

    return min(pdf_h / (4.0 * dotHV), INF_CLAMP);
}

mat3 computeTBN(vec3 worldNormal)
{
    // worldNormal 은 이미 보간 & 정규화돼 있다고 가정
    vec3 N = worldNormal;

    // ① 노멀과 가장 평행하지 않은 전역축 선택
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0)
                                : vec3(0.0, 1.0, 0.0);

    // ② 직교 Tangent 1, 2
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);           // 이미 정규

    return mat3(T, B, N);
}

// ======== using sheen material ========
void GetSheenMaterial(MaterialParameter material, vec2 uv, out vec3 sheenColor, out float sheenRoughness)
{
    if(length(material._sheenColorFactor) > 0.0 || material._sheenRoughnessFactor > 0.0)
    {
        const TextureParameter sheenColorTexture = material._sheenColorTexture;
        const vec4 sheenColorTextureRGB = texture(sampler2D(textures[nonuniformEXT(sheenColorTexture)], linearSampler), uv);

        if(length(sheenColorTextureRGB.rgb) <= 0)
        sheenColor = material._sheenColorFactor;
        else
        sheenColor = material._sheenColorFactor * sheenColorTextureRGB.rgb;

        const TextureParameter sheenRoughnessTexture = material._sheenRoughnessTexture;
        const vec4 sheenRoughnsheenRoughnessTextureRGB = texture(sampler2D(textures[nonuniformEXT(sheenRoughnessTexture)], linearSampler), uv);


        if(length(sheenRoughnsheenRoughnessTextureRGB.rgb) <= 0)
        sheenRoughness = material._sheenRoughnessFactor;
        else
        sheenRoughness = material._sheenRoughnessFactor * sheenRoughnsheenRoughnessTextureRGB.a;
    }
    else
    {
        sheenColor = vec3(0);
        sheenRoughness = 0;
    }
}
// ======================================