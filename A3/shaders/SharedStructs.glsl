struct RayPayload
{
    vec3 rayDirection;
    vec3 radiance;
    vec3 desiredPosition;
    uint depth;
    uint transmissionDepth;
    uint rngState;
    float pdfBRDF;
    float visibility;
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
   uint64_t materialAddress;
};

struct EnvImportanceSampleData {
    vec3 dir;
    float pdf;
};

#define TextureParameter uint
struct MaterialParameter
{
	vec4 _baseColorFactor;

	TextureParameter _baseColorTexture;
	TextureParameter _normalTexture;
	TextureParameter _occlusionTexture;
	float _metallicFactor;

	float _roughnessFactor;
	TextureParameter _metallicRoughnessTexture;
	float _transmissionFactor;
	TextureParameter _transmissionTexture;
	float _ior;

	vec3 _emissiveFactor;
	TextureParameter _emissiveTexture;
};