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
        outMesh.triangleCount = shape.mesh.num_face_vertices.size();

        outMesh.positions.reserve( shape.mesh.indices.size() + outMesh.positions.size() );
        outMesh.indices.reserve( shape.mesh.indices.size() + outMesh.indices.size() );
        outMesh.cumulativeTriangleArea.resize( shape.mesh.num_face_vertices.size() 
                                               + outMesh.cumulativeTriangleArea.size() + 1);

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
                VertexPosition prod = 0.5f * AB.cross(AC);
                outMesh.cumulativeTriangleArea[sumIdx] = outMesh.cumulativeTriangleArea[sumIdx - 1] + prod.length();
                ++sumIdx;
            }
        }
        //std::cout << " " << "\n";

        /*if (attrib.normals.empty()) {
            for (size_t f = 0; f < shape.mesh.indices.size() / 3; f++) {
                tinyobj::index_t idx0 = shape.mesh.indices[3 * f + 0];
                tinyobj::index_t idx1 = shape.mesh.indices[3 * f + 1];
                tinyobj::index_t idx2 = shape.mesh.indices[3 * f + 2];

                float v0[3] = {
                    attrib.vertices[3 * idx0.vertex_index + 0],
                    attrib.vertices[3 * idx0.vertex_index + 1],
                    attrib.vertices[3 * idx0.vertex_index + 2]
                };
                float v1[3] = {
                    attrib.vertices[3 * idx1.vertex_index + 0],
                    attrib.vertices[3 * idx1.vertex_index + 1],
                    attrib.vertices[3 * idx1.vertex_index + 2]
                };
                float v2[3] = {
                    attrib.vertices[3 * idx2.vertex_index + 0],
                    attrib.vertices[3 * idx2.vertex_index + 1],
                    attrib.vertices[3 * idx2.vertex_index + 2]
                };

                float e1[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
                float e2[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };
                float normal[3] = {
                    e1[1] * e2[2] - e1[2] * e2[1],
                    e1[2] * e2[0] - e1[0] * e2[2],
                    e1[0] * e2[1] - e1[1] * e2[0]
                };

                float len = sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
                if (len > 0) {
                    normal[0] /= len;
                    normal[1] /= len;
                    normal[2] /= len;
                }

                for (int i = 0; i < 3; i++) {
                    VertexAttributes& attr = outMesh.attributes[outMesh.indices.size() - 3 + i];
                    attr.normals[0] = normal[0];
                    attr.normals[1] = normal[1];
                    attr.normals[2] = normal[2];
                    attr.normals[3] = 0.0f;
                }
            }
        }*/
    }
}