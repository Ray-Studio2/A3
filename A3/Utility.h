#pragma once

#include <string>

namespace A3
{
struct MeshResource;

namespace Utility
{
void loadMeshFile( MeshResource& outMesh, const std::string& filePath );

void loadTextFile( std::string& outText, const std::string& filePath );
}
}