#include "Utility.h"
#include "MeshResource.h"
#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
// Optional. define TINYOBJLOADER_USE_MAPBOX_EARCUT gives robust triangulation. Requires C++11
//#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"

using namespace A3;

tinyobj::ObjReaderConfig    tinyObjConfig;
tinyobj::ObjReader          tinyObjReader;

void Utility::loadMeshFile( MeshResource& outMesh, const std::string& filePath )
{
    if( !tinyObjReader.ParseFromFile( filePath, tinyObjConfig ) )
    {
        if( !tinyObjReader.Error().empty() )
        {
            std::cerr << "TinyObjReader: " << tinyObjReader.Error();
        }
        exit( 1 );
    }
    
    if( !tinyObjReader.Warning().empty() )
    {
        std::cout << "TinyObjReader: " << tinyObjReader.Warning();
    }

    const tinyobj::attrib_t& attrib = tinyObjReader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = tinyObjReader.GetShapes();

    uint32 sumIdx = 1;
    for( const auto& shape : shapes )
    {
        outMesh.positions.reserve( shape.mesh.indices.size() + outMesh.positions.size() );
        outMesh.indices.reserve( shape.mesh.indices.size() + outMesh.indices.size() );

        outMesh.cumulativeTriangleArea.resize(shape.mesh.num_face_vertices.size() + outMesh.cumulativeTriangleArea.size() + 1);

        outMesh.triangleCount = shape.mesh.num_face_vertices.size();

        for( const auto& index : shape.mesh.indices )
        {
            VertexPosition positions;
            VertexAttributes attributes;

            positions.x = attrib.vertices[ 3 * index.vertex_index + 0 ];
            positions.y = attrib.vertices[ 3 * index.vertex_index + 1 ];
            positions.z = attrib.vertices[ 3 * index.vertex_index + 2 ];

            if( !attrib.normals.empty() && index.normal_index >= 0 )
            {
                attributes.normals[ 0 ] = attrib.normals[ 3 * index.normal_index + 0 ];
                attributes.normals[ 1 ] = attrib.normals[ 3 * index.normal_index + 1 ];
                attributes.normals[ 2 ] = attrib.normals[ 3 * index.normal_index + 2 ];
            }

            /*if( !attrib.colors.empty() )
            {
                v.color[ 0 ] = attrib.colors[ 3 * index.vertex_index + 0 ];
                v.color[ 1 ] = attrib.colors[ 3 * index.vertex_index + 1 ];
                v.color[ 2 ] = attrib.colors[ 3 * index.vertex_index + 2 ];
            }*/

            if( !attrib.texcoords.empty() && index.texcoord_index >= 0 )
            {
                attributes.uvs[ 0 ] = attrib.texcoords[ 2 * index.texcoord_index + 0 ];
                attributes.uvs[ 1 ] = attrib.texcoords[ 2 * index.texcoord_index + 1 ];
            }

            outMesh.positions.push_back( positions );
            outMesh.attributes.push_back( attributes );
            outMesh.indices.push_back( static_cast< uint32 >( outMesh.indices.size() ) ); // TODO: repeated vertex

            if (outMesh.indices.size() % 3 == 0) {
                const int idx = outMesh.indices.size() - 1;
                VertexPosition AB = outMesh.positions[idx - 0] - outMesh.positions[idx - 2];
                VertexPosition AC = outMesh.positions[idx - 1] - outMesh.positions[idx - 2];
                VertexPosition prod = AB.cross(AC);

                // area of triangle 0.5 * |crossproduct|
                // @TODO: does sqrt() matter for pdf?
                outMesh.cumulativeTriangleArea[sumIdx] = outMesh.cumulativeTriangleArea[sumIdx - 1] + prod.length();
                ++sumIdx;
            }
        }
    }
}