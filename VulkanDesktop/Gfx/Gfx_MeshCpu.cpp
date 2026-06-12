#include "Gfx_MeshCpu.h"

#include <glm/glm.hpp>
#include <stdexcept>
#include <unordered_map>

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#endif

namespace {

std::vector< glm::vec3 > ComputeSmoothNormals( const tinyobj::attrib_t& attrib, const std::vector< tinyobj::shape_t >& shapes ) {
    const size_t             vertexCount = attrib.vertices.size() / 3;
    std::vector< glm::vec3 > normals( vertexCount, glm::vec3( 0.0f ) );

    for ( const auto& shape : shapes ) {
        for ( size_t i = 0; i + 2 < shape.mesh.indices.size(); i += 3 ) {
            const auto& idx0 = shape.mesh.indices[ i ];
            const auto& idx1 = shape.mesh.indices[ i + 1 ];
            const auto& idx2 = shape.mesh.indices[ i + 2 ];

            const glm::vec3 p0 = { attrib.vertices[ 3 * idx0.vertex_index + 0 ], attrib.vertices[ 3 * idx0.vertex_index + 1 ], attrib.vertices[ 3 * idx0.vertex_index + 2 ] };
            const glm::vec3 p1 = { attrib.vertices[ 3 * idx1.vertex_index + 0 ], attrib.vertices[ 3 * idx1.vertex_index + 1 ], attrib.vertices[ 3 * idx1.vertex_index + 2 ] };
            const glm::vec3 p2 = { attrib.vertices[ 3 * idx2.vertex_index + 0 ], attrib.vertices[ 3 * idx2.vertex_index + 1 ], attrib.vertices[ 3 * idx2.vertex_index + 2 ] };

            const glm::vec3 faceNormal = glm::cross( p1 - p0, p2 - p0 );
            if ( glm::dot( faceNormal, faceNormal ) > 0.0f ) {
                normals[ idx0.vertex_index ] += faceNormal;
                normals[ idx1.vertex_index ] += faceNormal;
                normals[ idx2.vertex_index ] += faceNormal;
            }
        }
    }

    for ( auto& normal : normals ) {
        if ( glm::dot( normal, normal ) > 0.0f ) {
            normal = glm::normalize( normal );
        }
        else {
            normal = glm::vec3( 0.0f, 1.0f, 0.0f );
        }
    }

    return normals;
}

Gfx_Bounds ComputeLocalBoundsFromVertices( const std::vector< Gfx_Vertex >& aVertices ) {
    if ( aVertices.empty() ) {
        return {};
    }
    glm::vec3 minPos = aVertices.front().pos;
    glm::vec3 maxPos = minPos;
    for ( const Gfx_Vertex& vertex : aVertices ) {
        minPos = glm::min( minPos, vertex.pos );
        maxPos = glm::max( maxPos, vertex.pos );
    }
    Gfx_Bounds bounds{};
    bounds.myMin = minPos;
    bounds.myMax = maxPos;
    return bounds;
}

}  // namespace

void Gfx_MeshCpu::LoadFromPath( const std::string& aPath ) {
    tinyobj::attrib_t                          attrib;
    std::vector< tinyobj::shape_t >            shapes;
    std::vector< tinyobj::material_t >         materials;
    std::string                                warn, error;
    std::unordered_map< Gfx_Vertex, uint32_t > uniqueVertices{};

    if ( !tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &error, aPath.c_str() ) ) {
        throw std::runtime_error( warn + error );
    }

    const bool hasObjNormals   = !attrib.normals.empty();
    const auto computedNormals = hasObjNormals ? std::vector< glm::vec3 >{} : ComputeSmoothNormals( attrib, shapes );

    for ( const auto& shape : shapes ) {
        for ( const auto& index : shape.mesh.indices ) {
            Gfx_Vertex vertex{};
            vertex.pos   = { attrib.vertices[ 3 * index.vertex_index + 0 ], attrib.vertices[ 3 * index.vertex_index + 1 ], attrib.vertices[ 3 * index.vertex_index + 2 ] };
            vertex.color = { 1.0f, 1.0f, 1.0f };
            if ( index.texcoord_index >= 0 && !attrib.texcoords.empty() ) {
                vertex.texCoord = { attrib.texcoords[ 2 * index.texcoord_index + 0 ], 1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ] };
            }
            else {
                vertex.texCoord = { 0.0f, 0.0f };
            }
            if ( index.normal_index >= 0 ) {
                vertex.normal = { attrib.normals[ 3 * index.normal_index + 0 ], attrib.normals[ 3 * index.normal_index + 1 ], attrib.normals[ 3 * index.normal_index + 2 ] };
            }
            else {
                vertex.normal = computedNormals[ index.vertex_index ];
            }

            if ( uniqueVertices.count( vertex ) == 0 ) {
                uniqueVertices[ vertex ] = static_cast< uint32_t >( myVertices.size() );
                myVertices.push_back( vertex );
            }

            myIndices.push_back( uniqueVertices[ vertex ] );
        }
    }

    myLocalBounds = ComputeLocalBoundsFromVertices( myVertices );
}
