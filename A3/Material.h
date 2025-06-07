#include "EngineTypes.h"

#include "SceneResource.h"
#include "Vector.h"
#include <unordered_map>

namespace A3
{
class IMaterial : public ISceneResource
{
protected:
    virtual ~IMaterial() {}

public:
    virtual int32 getShaderId() const = 0;
    virtual const Vec3& getBaseColor() const = 0;
};

class Material : public IMaterial
{
public:
    Material( int32 inShaderId, const Vec3& inBaseColor )
        : shaderId( inShaderId )
        , baseColor( inBaseColor )
    {}

    virtual int32 getShaderId() const { return shaderId; }

    virtual const Vec3& getBaseColor() const override { return baseColor; }

private:
	int32 shaderId;

    // @TODO: Replace with input parameters
    Vec3 baseColor;
};

class MaterialInstance : public IMaterial
{
public:
    MaterialInstance( Material* inMaterial, const Vec3& inBaseColor )
        : material( inMaterial )
        , baseColor( inBaseColor )
    {}

    virtual int32 getShaderId() const { return material->getShaderId(); }

    virtual const Vec3& getBaseColor() const override { return baseColor; }

private:
    Material* material;

    // @TODO: Replace with input parameters
    Vec3 baseColor;
};

//class MaterialCache
//{
//public:
//    void addMaterial( const Material&& material )
//    {
//        if( !materialCache.contains( material.getShaderId() ) )
//            materialCache.emplace( std::move( material ) );
//    }
//
//    Material* getMaterial( int32 shaderId )
//    {
//        if( materialCache.contains( shaderId ) )
//            return &materialCache.at( shaderId );
//        else
//            return nullptr;
//    }
//
//    void addMaterialInstance( const Material&& material )
//    {
//        if( !materialInstanceCache.contains( material.getShaderId() ) )
//            materialInstanceCache.emplace( std::move( material ) );
//    }
//
//    const std::vector<MaterialInstance>& getMaterialInstances( int32 shaderId )
//    {
//        if( materialInstanceCache.contains( shaderId ) )
//            return materialInstanceCache.at( shaderId );
//        else
//        {
//            return {};
//        }
//    }
//
//private:
//    std::unordered_map<int32, Material> materialCache;
//    std::unordered_map<int32, std::vector<MaterialInstance>> materialInstanceCache;
//};
}