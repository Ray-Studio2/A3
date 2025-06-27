#include "Vulkan.h"
#include "VulkanResource.h"
#include "Utility.h"

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

using namespace A3;

std::vector<uint32_t> glsl2spv( glslang_stage_t stage, const char* shaderSource )
{
    const glslang_input_t input = {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = GLSLANG_TARGET_VULKAN_1_3,
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = GLSLANG_TARGET_SPV_1_5,
        .code = shaderSource,
        .default_version = 100,
        .default_profile = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = glslang_default_resource(),
    };

    glslang_shader_t* shader = glslang_shader_create( &input );

    if( !glslang_shader_preprocess( shader, &input ) )
    {
        printf( "GLSL preprocessing failed (%d)\n", stage );
        printf( "%s\n", glslang_shader_get_info_log( shader ) );
        printf( "%s\n", glslang_shader_get_info_debug_log( shader ) );
        printf( "%s\n", input.code );
        glslang_shader_delete( shader );
        return {};
    }

    if( !glslang_shader_parse( shader, &input ) )
    {
        printf( "GLSL parsing failed (%d)\n", stage );
        printf( "%s\n", glslang_shader_get_info_log( shader ) );
        printf( "%s\n", glslang_shader_get_info_debug_log( shader ) );
        printf( "%s\n", glslang_shader_get_preprocessed_code( shader ) );
        glslang_shader_delete( shader );
        return {};
    }

    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader( program, shader );

    if( !glslang_program_link( program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT ) )
    {
        printf( "GLSL linking failed (%d)\n", stage );
        printf( "%s\n", glslang_program_get_info_log( program ) );
        printf( "%s\n", glslang_program_get_info_debug_log( program ) );
        glslang_program_delete( program );
        glslang_shader_delete( shader );
        return {};
    }

    glslang_program_SPIRV_generate( program, stage );

    size_t size = glslang_program_SPIRV_get_size( program );
    std::vector<uint32_t> spvBirary( size );
    glslang_program_SPIRV_get( program, spvBirary.data() );

    const char* spirv_messages = glslang_program_SPIRV_get_messages( program );
    if( spirv_messages )
        printf( "(%d) %s\b", stage, spirv_messages );

    glslang_program_delete( program );
    glslang_shader_delete( shader );

    return spvBirary;
}

glslang_stage_t getGLSLangStage( EShaderStage stage )
{
    switch( stage )
    {
        case SS_Vertex:         return GLSLANG_STAGE_VERTEX;
        case SS_Fragment:       return GLSLANG_STAGE_FRAGMENT;
        case SS_Compute:        return GLSLANG_STAGE_COMPUTE;
        case SS_RayGeneration:  return GLSLANG_STAGE_RAYGEN;
        case SS_Miss:           return GLSLANG_STAGE_MISS;
        case SS_AnyHit:         return GLSLANG_STAGE_ANYHIT;
        case SS_ClosestHit:     return GLSLANG_STAGE_CLOSESTHIT;
    }

    // Should not reach here
    return GLSLANG_STAGE_VERTEX;
}

std::string translateShaderType( const ShaderDesc& desc )
{
    std::string outType;
    if( !desc.prefix.empty() )
    {
        outType = std::format( "{0}", desc.prefix );
    }

    switch( desc.type )
    {
        case SS_Vertex:         return outType.append( "VERTEX_SHADER" );
        case SS_Fragment:       return outType.append( "FRAGMENT_SHADER" );
        case SS_Compute:        return outType.append( "COMPUTE_SHADER" );
        case SS_RayGeneration:  return outType.append( "RAY_GENERATION_SHADER" );
        case SS_ClosestHit:     return outType.append( "CLOSEST_HIT_SHADER" );
        case SS_AnyHit:         return outType.append( "ANY_HIT_SHADER" );
        case SS_Miss:           return outType.append( "MISS_SHADER" );
    }

    // Should not reach here
    return outType;
}

std::string insertPredefines( const std::string& shaderText, const ShaderDesc& desc )
{
    std::string outText = std::format( "#version 460\n#define {0} 1\n", translateShaderType( desc ) );
    outText.append( shaderText );

    return outText;
}

IShaderModuleRef VulkanRenderBackend::createShaderModule( const ShaderDesc& desc )
{
    VulkanShaderModule* outModule = new VulkanShaderModule();

    std::string shaderText;
    Utility::loadTextFile( shaderText, desc.fileName );
    shaderText = insertPredefines( shaderText, desc );
    const std::vector<uint32> spv_blob = glsl2spv( getGLSLangStage( desc.type ), shaderText.c_str() );

    VkShaderModuleCreateInfo createInfo
    {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spv_blob.size() * sizeof( uint32 ),
        .pCode = spv_blob.data(),
    };

	if( vkCreateShaderModule( device, &createInfo, nullptr, &outModule->module ) != VK_SUCCESS )
	{
		throw std::runtime_error( "failed to create shader module!" );
	}

    return IShaderModuleRef( outModule );
}
