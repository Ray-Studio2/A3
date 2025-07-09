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

    float dotN = max(dot(normal, dir), 1e-4); // 테두리가 0으로 수렴되는 걸 막아줌
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
    float dotNH = max(dot(normal, halfDir), 0.0);
    float dotNH2 = dotNH * dotNH;

    float num = a2;
    float denom = (dotNH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

vec3 calculateBRDF(vec3 normal, vec3 viewDir, vec3 lightDir, vec3 halfDir, vec3 color, float metallic, float alpha) // Cook-Torrence
{
    float dotNV = max(dot(normal, viewDir), 0.0);
    float dotNL = max(dot(normal, lightDir), 0.0);

    const vec3 F0 = mix(vec3(0.04), color, metallic);

    float ndf = GGX_D(normal, halfDir, alpha);
    float geometry = GGX_G2(normal, viewDir, lightDir, alpha);
    vec3 fresnel = Schlick_F(viewDir, halfDir, F0);

    vec3 num = ndf * geometry * fresnel;
    float denom = 4.0 * dotNV * dotNL;
    vec3 f_spec = num / max(denom, 1e-4);

    // vec3 fresnel_diff = Schlick_F(dotNV, F0);
    // vec3 kD = (vec3(1.0) - F0) * (1.0 - metallic);
    // vec3 f_diff = kD * color / PI;
    vec3 f_diff = (1.0 - metallic) * color * (1/PI);

    return clamp(f_diff + f_spec, 0.0, 1.0);
}

// // TODO: v는 local view vector, rayDir를 표면 normal에 따라 local로 변경
// vec3 sampleGGXVNDF(vec3 v, float alpha_x, float alpha_y, float u1, float u2)
// {
//     // transforming the view direction to the hemisphere configuration
//     vec3 v_h = normalize(vec3(alpha_x * v.x, alpha_y * v.y, v.z));

//     float lenSq = v_h.x * v_h.x + v_h.y * v_h.y;
//     float invsqrt = 1 / sqrt(lenSq);
//     vec3 T1 = lenSq > 0 ? vec3(-v_h.y, v_h.x, 0) * invsqrt : vec3(1, 0, 0);
//     // T1 is perpendicular to the view vector = normalize(Z x V_h)
//     // when v = (0,0,1), cross product is 0, so we pick x = (1,0,0) for a safe approach
//     vec3 T2 = cross(v_h, T1);

//     float r = sqrt(u1);
//     float phi = 2.0 * PI * u2;
//     float t1 = r * cos(phi);
//     float t2 = r * sin(phi);
//     float s = 0.5 * (1.0 + v_h.z);
//     t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

//     vec3 n_h = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * v_h;
//     vec3 n_e = normalize(vec3(alpha_x * n_h.x, alpha_y * n_h.y, max(0.0, n_h.z)));
//     return n_e; // h-vector
// }

// float pdfGGXVNDF(vec3 normal, vec3 viewDir, vec3 halfDir, float alpha)
// {
//     float G = GGX_G1(normal, viewDir, alpha);
//     float D = GGX_D(normal, halfDir, alpha);

//     float num = G * D * max(dot(viewDir, halfDir), 0.0);
//     float denom = max(dot(viewDir, normal), 0.0);
//     return num / denom;
// }

// vec3 computeLocalNormal(vec3 N_world, vec3 p0, vec3 p1, vec3 p2)
// {
//     // 1. 삼각형의 노멀 계산
//     vec3 N = normalize(cross(p1 - p0, p2 - p0));

//     // 2. 임의의 Tangent 벡터 계산
//     vec3 T = normalize(p1 - p0); // p1 - p0을 기준으로 사용
//     if (abs(dot(N, T)) > 0.99) // 노멀과 거의 평행하면 다른 방향 사용
//         T = normalize(p2 - p0);

//     // 3. Bitangent
//     vec3 B = normalize(cross(N, T));

//     // 4. TBN 행렬
//     mat3 TBN = transpose(mat3(T, B, N)); // 월드 → 로컬 변환

//     // 5. 월드 노멀 → 로컬 노멀
//     vec3 N_local = TBN * N_world;

//     return N_local;
// }
