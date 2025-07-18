// 0.0 ~ 1.0 사이의 float 반환
float RandomValue(inout uint state) {
    state *= (state + 195439) * (state + 124395) * (state + 845921);
    return float(state) / 4294967295.0;  // float 변환 추가!
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
    // 더 안정적인 tangent space 생성
    vec3 up = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = normalize(cross(normal, tangent));
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

/////////////////////////////////////////////////////////////////////////////////////////////

// Improved random number generator
uint pcg_hash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random(inout uint rngState) {
    rngState = pcg_hash(rngState);
    return float(rngState) / float(0xffffffffu);
}

/////////////////////////////////////////////////////////////////////////////////////////////

vec3 toneMapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 toneMapReinhard(vec3 color) {
    return color / (1.0 + color);
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

uint random2( uvec2 pixel, uint sampleIndex, uint depth, uint axis )
{
    return generateSeed( pixel, sampleIndex, depth, axis );
}

/////////////////////////////////////////////////////////////////////////////////////////////

bool finite(float v)
{
    // 값이 NaN 도 아니고, Inf(±무한) 도 아니면 finite
    return !(isnan(v) || isinf(v));
}

float powerHeuristic(float pdfA, float pdfB)
{
    // 1) 음수, 비정상 값 가드
    if (!finite(pdfA) || !finite(pdfB)) return 0.0;
    pdfA = max(pdfA, 0.0);
    pdfB = max(pdfB, 0.0);

    // 2) 둘 다 “사실상 0” 이면 그냥 0.5 리턴
    const float EPS = 1e-6;          // sub-normal 보정보다 약간 큰 값
    float maxPdf = max(pdfA, pdfB);
    if (maxPdf < EPS) return 0.5;    // or 0.0, 1.0 → 취향에 맞게

    // 3) 오버플로 방지용 정규화
    float a = pdfA / maxPdf;
    float b = pdfB / maxPdf;

    // 4) 다시 0/0 방지
    float a2 = a * a;
    float b2 = b * b;
    float denom = max(a2 + b2, EPS); // EPS 덕에 절대 0 안 됨

    return a2 / denom;
}


/////////////////////////////////////////////////////////////////////////////////////////////

mat3 rotateY(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat3(
        c, 0.0, -s,
        0.0, 1.0, 0.0,
        s, 0.0, c
    );
}

/////////////////////////////////////////////////////////////////////////////////////////////

vec4 getEnvImportanceData(uint x, uint y)
{
    return texelFetch( envImportanceData, ivec2( x, y ), 0 );
}

vec3 sampleEnvDirection(uvec2 pixel, uint sampleIndex, uint depth, out float pdf)
{
    ivec2 texSize = textureSize(environmentMap, 0);
    uint width = texSize.x;
    uint height = texSize.y;

    uint x = random2( pixel, sampleIndex, depth, 1 ) & ( width - 1 );   // random % width
    uint y = random2( pixel, sampleIndex, depth, 0 ) & ( height - 1 );  // random % height
    vec4 pixelValue = getEnvImportanceData(x, y);
    pdf = max(pixelValue.w, 1e-6);

    vec3 dir = pixelValue.xyz;
    float rot = gImguiParam.envmapRotDeg * (PI / 180.0);
    dir = rotateY(-rot) * dir;

    // @TODO: pre-calculate on cpu
    return normalize( dir );
}

