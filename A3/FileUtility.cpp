#include "Utility.h"
#include "EngineTypes.h"
#include <fstream>
#include <vector>

using namespace A3;

void Utility::loadTextFile( std::string& outText, const std::string& filePath )
{
    std::ifstream file( filePath, std::ios::ate );
    if( !file.is_open() )
    {
        throw std::runtime_error( "failed to open file!" );
    }

    const uint64 fileSize = ( uint64 )file.tellg();
    outText.resize( fileSize );

    file.seekg( 0, std::ios::beg );
    file.read( outText.data(), fileSize );
    file.close();
}