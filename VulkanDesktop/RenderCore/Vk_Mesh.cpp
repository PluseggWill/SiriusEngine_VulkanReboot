#include "Vk_Mesh.h"

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#endif

void Mesh::loadMesh( const std::string& aPath ) {
    // Fill the data by loading model
    tinyobj::attrib_t                      attrib;
    std::vector< tinyobj::shape_t >        shapes;
    std::vector< tinyobj::material_t >     materials;
    std::string                            warn, error;
    std::unordered_map< Vertex, uint32_t > uniqueVertices{};

    if ( !tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &error, aPath.c_str() ) ) {
        throw std::runtime_error( warn + error );
    }

    for ( const auto& shape : shapes ) {
        for ( const auto& index : shape.mesh.indices ) {
            Vertex vertex{};
            vertex.pos      = { attrib.vertices[ 3 * index.vertex_index + 0 ], attrib.vertices[ 3 * index.vertex_index + 1 ], attrib.vertices[ 3 * index.vertex_index + 2 ] };
            vertex.texCoord = { attrib.texcoords[ 2 * index.texcoord_index + 0 ], 1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ] };
            vertex.color    = { 1.0f, 1.0f, 1.0f };

            if ( uniqueVertices.count( vertex ) == 0 ) {
                uniqueVertices[ vertex ] = static_cast< uint32_t >( myVertices.size() );
                myVertices.push_back( vertex );
            }

            myIndices.push_back( uniqueVertices[ vertex ] );
        }
    }
}