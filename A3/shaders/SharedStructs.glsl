struct RayPayload
{
    vec3 rayDirection;
    vec3 radiance;
    vec3 throughput;
    vec3 desiredPosition;
    uint depth;
    uint rngState;
    bool bEnvMap;
};

// struct ShadowPayload
// {
//     vec3 desiredPosition;
// };

struct LightData
{
    mat4 transform;
    float emittance;
    uint triangleCount;
};

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
   uint64_t cumulativeTriangleAreaAddress;
};

struct EnvImportanceSampleData {
    vec3 dir;
    float pdf;
};