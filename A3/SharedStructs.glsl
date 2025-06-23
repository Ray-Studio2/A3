struct RayPayload
{
    vec3 rayDirection;
    vec3 radiance;
    vec3 throughput;
    uint depth;
    uint rngState;
};

struct ShadowPayload
{
    bool inShadow;
};

struct LightData
{
    vec3 position;
    float radius;
    vec3 emission;
    float pad0;  // Alignment for std430
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
};