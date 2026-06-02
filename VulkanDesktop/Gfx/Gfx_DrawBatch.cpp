#include "Gfx_DrawBatch.h"

void Gfx_BuildOpaqueDrawBatches( const std::vector< Gfx_DrawInstance >& aSortedDraws, std::vector< Gfx_BatchRun >& aOut ) {
    aOut.clear();
    if ( aSortedDraws.empty() ) {
        return;
    }

    aOut.reserve( aSortedDraws.size() );

    size_t runStart = 0;
    for ( size_t i = 1; i <= aSortedDraws.size(); ++i ) {
        const bool endOfRun =
            ( i == aSortedDraws.size() ) || ( Gfx_OpaqueBatchKey( aSortedDraws[ i ].mySortKey ) != Gfx_OpaqueBatchKey( aSortedDraws[ runStart ].mySortKey ) );
        if ( !endOfRun ) {
            continue;
        }

        Gfx_BatchRun run{};
        run.myFirstDrawIndex = static_cast< uint32_t >( runStart );
        run.myDrawCount      = static_cast< uint32_t >( i - runStart );
        run.myBatchKey       = Gfx_OpaqueBatchKey( aSortedDraws[ runStart ].mySortKey );
        aOut.push_back( run );
        runStart = i;
    }
}
