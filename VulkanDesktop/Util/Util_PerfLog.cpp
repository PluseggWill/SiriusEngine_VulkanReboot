#include "Util_PerfLog.h"

#include "Util_EngineConfig.h"
#include "Util_ResolvePath.h"

#include <fstream>
#include <mutex>
#include <stdexcept>

namespace {

std::mutex    gPerfLogMutex;
std::ofstream gPerfLogStream;
bool          gPerfLogOpened = false;

void EnsurePerfLogOpen() {
    if ( gPerfLogOpened ) {
        return;
    }
    const std::string logical = UtilEngineConfig::GetPerfLogPath();
    if ( logical.empty() ) {
        return;
    }
    const std::string resolved = UtilResolvePath::Resolve( logical );
    gPerfLogStream.open( resolved, std::ios::out | std::ios::trunc );
    if ( !gPerfLogStream.is_open() ) {
        throw std::runtime_error( "UtilPerfLog: cannot open " + resolved );
    }
    gPerfLogOpened = true;
}

}  // namespace

namespace UtilPerfLog {

void AppendFrame( uint64_t aFrameIndex, float aFrameMs, uint32_t aDrawCalls, uint32_t aVisibleDraws, uint32_t aActiveViews, const char* aMaterialPath ) {
    if ( UtilEngineConfig::GetPerfLogPath().empty() ) {
        return;
    }

    std::lock_guard< std::mutex > lock( gPerfLogMutex );
    EnsurePerfLogOpen();

    const char* pathName = ( aMaterialPath != nullptr && aMaterialPath[ 0 ] != '\0' ) ? aMaterialPath : "Unknown";
    gPerfLogStream << "{\"schemaVersion\":1,\"frameIndex\":" << aFrameIndex << ",\"frameMs\":" << aFrameMs << ",\"drawCalls\":" << aDrawCalls
                   << ",\"visibleDraws\":" << aVisibleDraws << ",\"activeViews\":" << aActiveViews << ",\"materialPath\":\"" << pathName << "\"}\n";
    gPerfLogStream.flush();
}

}  // namespace UtilPerfLog
