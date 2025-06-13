
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
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
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

vec3 RandomHemisphereNormal(vec3 normal, vec2 xi) {
    vec3 local = SampleUniformHemisphere(xi);
   // 기준축이 Z였던 것을 기준축을 normal로 바꾸겠다~ 는 회전 행렬
   // 예를 들어 Z축 기준으로 오른쪽을 향하는 vector였으면 이 회전 행렬을 곱하면 normal 기준으로 오른쪽을 향하는 vector가 된다~
    mat3 tbn = CreateTangentSpace(normal);
    return normalize(tbn * local);
}

// Cosine-weighted hemisphere sampling (+Z 기준)
vec3 SampleCosineHemisphere(vec2 xi) {
    float r = sqrt(xi.x); // 중요! √x를 써야 cos 분포됨
    float phi = 2.0 * 3.1415926 * xi.y;

    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = sqrt(max(0.0, 1.0 - x * x - y * y)); // z = cos(θ)

    return vec3(x, y, z); // 로컬 기준 방향
}

// 최종 함수: 월드 노말 기준 방향 반환
vec3 RandomCosineHemisphere(vec3 worldNormal, vec2 xi) {
    vec3 local = SampleCosineHemisphere(xi);       // Z축 기준 벡터
    mat3 tbn = CreateTangentSpace(worldNormal);    // Z → Normal
    return normalize(tbn * local);                 // 회전 후 정규화
}
