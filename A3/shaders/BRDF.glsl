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
    vec3 f_spec = num / max(denom, 1e-6);

    vec3 f_diff = (1.0 - metallic) * color * (1/PI);

    return clamp(f_diff + f_spec, 0.0, 1e24);
}

vec3 calculateW(vec3 normal, vec3 viewDir, vec3 lightDir, vec3 halfDir, vec3 color, float metallic, float alpha)
{
    float dotHV = max(dot(viewDir, halfDir), 1e-6);

    const vec3 F0 = mix(vec3(0.04), color, metallic);

    vec3 F = Schlick_F(viewDir, halfDir, F0);
    float G2 = GGX_G2(normal, viewDir, lightDir, alpha);
    float G1 = GGX_G1(normal, viewDir, alpha);

    return F * G2 / G1;
}

// TODO: v는 local view vector, rayDir를 표면 normal에 따라 local로 변경
vec3 sampleGGXVNDF(vec3 v, float alpha_x, float alpha_y, vec2 seed) 
{
    float u1 = seed.x;
    float u2 = seed.y;

    // transforming the view direction to the hemisphere configuration
    vec3 v_h = normalize(vec3(alpha_x * v.x, alpha_y * v.y, v.z));

    float lenSq = v_h.x * v_h.x + v_h.y * v_h.y;
    float invsqrt = 1 / sqrt(lenSq);
    vec3 T1 = lenSq > 0 ? vec3(-v_h.y, v_h.x, 0) * invsqrt : vec3(1, 0, 0);
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
    float dotNV = max(dot(viewDir, normal), 1e-6);

    float num = G * D * dotHV;
    float denom = dotNV;
    float pdf_h = num / denom;

    return pdf_h / (4.0 * dotHV);
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
