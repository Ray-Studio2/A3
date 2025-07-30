struct RayPayload
{
    vec3 rayDirection;
    vec3 radiance;
    vec3 desiredPosition;
    uint depth;
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

struct BasicMaterial
{
    vec3 baseColor;
    float metallic;
    float alpha;    // roughness * roughness

    vec3 sheenColor;
    float sheenRoughness;
    // more parameters to be added...
};

struct DisneyMaterial
{
    vec3 baseColor;
    float metallic;
    float roughness;
    
    float subsurface;
    float specular;
    float specularTint;
    float anisotropic;
    float sheen;
    float sheenTint;
    float clearcoat;
    float clearcoatGloss;
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
	float _padding1;
	float _padding2;

	vec3 _emissiveFactor;
	TextureParameter _emissiveTexture;


    // ======== using sheen material ========
    // ======================================
    vec3 _sheenColorFactor;
    float _sheenRoughnessFactor;
    TextureParameter _sheenColorTexture;
    TextureParameter _sheenRoughnessTexture;
    // ======================================
}; 