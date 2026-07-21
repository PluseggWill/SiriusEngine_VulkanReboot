#pragma once

#include <cstdint>
#include <string>
#include <vector>

// CPU OBJ decode (tinyobjloader). Callers never include tiny_obj_loader or call tinyobj::* directly.
struct Util_ObjIndex {
    int myVertexIndex   = 0;
    int myNormalIndex   = -1;
    int myTexcoordIndex = -1;
};

struct Util_ObjShape {
    std::vector< Util_ObjIndex > myIndices;  // triangulated corners
};

struct Util_ObjAttrib {
    std::vector< float > myVertices;   // xyz packed
    std::vector< float > myNormals;    // xyz packed (may be empty)
    std::vector< float > myTexcoords;  // uv packed (may be empty)
};

namespace UtilObjLoad {

// Loads triangulated OBJ from a resolved filesystem path. On failure returns false and fills aOutError.
bool LoadTriangulated( const std::string& aResolvedPath, Util_ObjAttrib& aOutAttrib, std::vector< Util_ObjShape >& aOutShapes, std::string& aOutError );

}  // namespace UtilObjLoad
