#include "Gfx_DrawTemplate.h"

void Gfx_FillDrawTemplate( Gfx_DrawTemplate& aOut, const Gfx_DrawInstance& aDraw, uint32_t aIndexCount, uint32_t aInstanceDataOffset ) {
    // firstIndex/vertexOffset/firstInstance are 0 today; mesh-local IB starts at 0. §B will use mesh.myIndexCount for indexCount.
    aOut.myIndirect.indexCount           = aIndexCount;
    aOut.myIndirect.instanceCount        = 1;
    aOut.myIndirect.firstIndex           = 0;
    aOut.myIndirect.vertexOffset         = 0;
    aOut.myIndirect.firstInstance        = 0;
    aOut.myInstanceDataOffset            = aInstanceDataOffset;
    aOut.myMeshId                        = aDraw.myMeshId;
    aOut.myMaterialId                    = aDraw.myMaterialId;
    aOut.myEntityIndex                   = aDraw.myEntityIndex;
}
