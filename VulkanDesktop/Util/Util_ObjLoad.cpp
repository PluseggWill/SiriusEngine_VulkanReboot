#include "Util_ObjLoad.h"

#include "Util_Logger.h"

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#endif
#include <tiny_obj_loader.h>

bool UtilObjLoad::LoadTriangulated( const std::string& aResolvedPath, Util_ObjAttrib& aOutAttrib, std::vector< Util_ObjShape >& aOutShapes, std::string& aOutError ) {
    aOutAttrib = {};
    aOutShapes.clear();
    aOutError.clear();

    tinyobj::attrib_t                  attrib;
    std::vector< tinyobj::shape_t >    shapes;
    std::vector< tinyobj::material_t > materials;
    std::string                        warn;
    std::string                        error;

    if ( !tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &error, aResolvedPath.c_str() ) ) {
        aOutError = warn + error;
        UtilLogger::Error( "OBJ", "Failed to load OBJ: " + aResolvedPath + " (" + aOutError + ")" );
        return false;
    }
    if ( !warn.empty() ) {
        UtilLogger::Warn( "OBJ", "OBJ load warnings for " + aResolvedPath + ": " + warn );
    }

    aOutAttrib.myVertices  = std::move( attrib.vertices );
    aOutAttrib.myNormals   = std::move( attrib.normals );
    aOutAttrib.myTexcoords = std::move( attrib.texcoords );

    aOutShapes.reserve( shapes.size() );
    for ( const tinyobj::shape_t& shape : shapes ) {
        Util_ObjShape outShape{};
        outShape.myIndices.reserve( shape.mesh.indices.size() );
        for ( const tinyobj::index_t& index : shape.mesh.indices ) {
            Util_ObjIndex outIndex{};
            outIndex.myVertexIndex   = index.vertex_index;
            outIndex.myNormalIndex   = index.normal_index;
            outIndex.myTexcoordIndex = index.texcoord_index;
            outShape.myIndices.push_back( outIndex );
        }
        aOutShapes.push_back( std::move( outShape ) );
    }

    return true;
}
