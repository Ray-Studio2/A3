#pragma once

#include "EngineTypes.h"
#include "RenderResource.h"
#include <string>
#include <unordered_map>

namespace A3
{
enum EShaderStage
{
    SS_Vertex,
    SS_Fragment,
    SS_Compute,
    SS_RayGeneration,
    SS_Miss,
    SS_ClosestHit,
    SS_AnyHit,
    SS_Intersection,
    SS_COUNT
};

enum EShaderResourceDescriptor
{
    SRD_UniformBuffer,
    SRD_StorageImage,
    SRD_StorageBuffer,
    SRD_AccelerationStructure,
    SRD_Sampler, 
    SRD_ImageSampler,
};

struct ShaderResourceDescriptor
{
    EShaderResourceDescriptor type;
    uint32 index;
};

struct ShaderDesc
{
    EShaderStage type;
    std::string fileName;
    std::string prefix;
    // @TODO: Move descriptors to PSO so that we can save memory and iterate less on render backend
    std::vector<ShaderResourceDescriptor> descriptors;
    
    // Does not support yet
    std::string entry;

    bool operator==( const ShaderDesc& other ) const
    {
        return type == other.type 
            && prefix.compare(other.prefix) == 0 
            && fileName.compare( other.fileName ) == 0;
    }
};

struct ShaderDescHash
{
    int64 operator()( const ShaderDesc& desc ) const
    {
        return std::hash<int32>()( desc.type )
            ^ ( std::hash<std::string>()( desc.prefix ) << 1 )
            ^ ( std::hash<std::string>()( desc.fileName ) << 2 );
    }
};

class Shader
{
public:
    Shader() {}

private:
    IShaderModuleRef module;
};

class ShaderCache
{
public:
    IShaderModule* addShaderModule( const ShaderDesc& desc, IShaderModuleRef&& shader )
    {
        if( !cache.contains( desc ) )
            cache.emplace( desc, std::move( shader ) );

        return cache.at( desc ).get();
    }

    IShaderModule* getShaderModule( const ShaderDesc& desc )
    {
		if( cache.contains( desc ) )
			return cache.at( desc ).get();
		else
			return nullptr;
    }

private:
    // @TODO: Replace the key with uid
    std::unordered_map<ShaderDesc, IShaderModuleRef, ShaderDescHash> cache;
};
}
