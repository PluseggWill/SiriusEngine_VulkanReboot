#include "Gfx_MeshCpu.h"

#include "../Util/Util_ObjLoad.h"

#include <glm/glm.hpp>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::vector< glm::vec3 > ComputeSmoothNormals( const Util_ObjAttrib& aAttrib, const std::vector< Util_ObjShape >& aShapes ) {
    const size_t             vertexCount = aAttrib.myVertices.size() / 3;
    std::vector< glm::vec3 > normals( vertexCount, glm::vec3( 0.0f ) );

    for ( const Util_ObjShape& shape : aShapes ) {
        for ( size_t i = 0; i + 2 < shape.myIndices.size(); i += 3 ) {
            const Util_ObjIndex& idx0 = shape.myIndices[ i ];
            const Util_ObjIndex& idx1 = shape.myIndices[ i + 1 ];
            const Util_ObjIndex& idx2 = shape.myIndices[ i + 2 ];

            const glm::vec3 p0 = { aAttrib.myVertices[ 3 * idx0.myVertexIndex + 0 ], aAttrib.myVertices[ 3 * idx0.myVertexIndex + 1 ],
                                   aAttrib.myVertices[ 3 * idx0.myVertexIndex + 2 ] };
            const glm::vec3 p1 = { aAttrib.myVertices[ 3 * idx1.myVertexIndex + 0 ], aAttrib.myVertices[ 3 * idx1.myVertexIndex + 1 ],
                                   aAttrib.myVertices[ 3 * idx1.myVertexIndex + 2 ] };
            const glm::vec3 p2 = { aAttrib.myVertices[ 3 * idx2.myVertexIndex + 0 ], aAttrib.myVertices[ 3 * idx2.myVertexIndex + 1 ],
                                   aAttrib.myVertices[ 3 * idx2.myVertexIndex + 2 ] };

            const glm::vec3 faceNormal = glm::cross( p1 - p0, p2 - p0 );
            if ( glm::dot( faceNormal, faceNormal ) > 0.0f ) {
                normals[ idx0.myVertexIndex ] += faceNormal;
                normals[ idx1.myVertexIndex ] += faceNormal;
                normals[ idx2.myVertexIndex ] += faceNormal;
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
    Util_ObjAttrib                             attrib;
    std::vector< Util_ObjShape >               shapes;
    std::string                                error;
    std::unordered_map< Gfx_Vertex, uint32_t > uniqueVertices{};

    if ( !UtilObjLoad::LoadTriangulated( aPath, attrib, shapes, error ) ) {
        throw std::runtime_error( error.empty() ? "failed to load OBJ" : error );
    }

    const bool hasObjNormals   = !attrib.myNormals.empty();
    const auto computedNormals = hasObjNormals ? std::vector< glm::vec3 >{} : ComputeSmoothNormals( attrib, shapes );

    for ( const Util_ObjShape& shape : shapes ) {
        for ( const Util_ObjIndex& index : shape.myIndices ) {
            Gfx_Vertex vertex{};
            vertex.pos   = { attrib.myVertices[ 3 * index.myVertexIndex + 0 ], attrib.myVertices[ 3 * index.myVertexIndex + 1 ],
                             attrib.myVertices[ 3 * index.myVertexIndex + 2 ] };
            vertex.color = { 1.0f, 1.0f, 1.0f };
            if ( index.myTexcoordIndex >= 0 && !attrib.myTexcoords.empty() ) {
                vertex.texCoord = { attrib.myTexcoords[ 2 * index.myTexcoordIndex + 0 ], 1.0f - attrib.myTexcoords[ 2 * index.myTexcoordIndex + 1 ] };
            }
            else {
                vertex.texCoord = { 0.0f, 0.0f };
            }
            if ( index.myNormalIndex >= 0 ) {
                vertex.normal = { attrib.myNormals[ 3 * index.myNormalIndex + 0 ], attrib.myNormals[ 3 * index.myNormalIndex + 1 ],
                                  attrib.myNormals[ 3 * index.myNormalIndex + 2 ] };
            }
            else {
                vertex.normal = computedNormals[ index.myVertexIndex ];
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
