#include "Util_Logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace {
std::mutex         gLogMutex;
std::ofstream      gLogFile;
bool               gIsInitialized = false;
const std::string  kDefaultLogPath = "Logs/engine_log.txt";

const char* ToString( const UtilLogger::LogLevel aLevel ) {
    switch ( aLevel ) {
    case UtilLogger::LogLevel::Info:
        return "INFO";
    case UtilLogger::LogLevel::Warning:
        return "WARN";
    case UtilLogger::LogLevel::Error:
        return "ERROR";
    case UtilLogger::LogLevel::Debug:
        return "DEBUG";
    default:
        return "UNKNOWN";
    }
}

std::string NowTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto t   = std::chrono::system_clock::to_time_t( now );

    std::tm tmLocal{};
#ifdef _WIN32
    localtime_s( &tmLocal, &t );
#else
    localtime_r( &t, &tmLocal );
#endif

    const auto milliseconds = std::chrono::duration_cast< std::chrono::milliseconds >( now.time_since_epoch() ) % 1000;

    std::ostringstream oss;
    oss << std::put_time( &tmLocal, "%Y-%m-%d %H:%M:%S" ) << "." << std::setw( 3 ) << std::setfill( '0' ) << milliseconds.count();
    return oss.str();
}
}  // namespace

void UtilLogger::Init( const std::string& aLogFilePath ) {
    std::lock_guard< std::mutex > lock( gLogMutex );
    if ( gIsInitialized ) {
        return;
    }

    const std::string logPath = aLogFilePath.empty() ? kDefaultLogPath : aLogFilePath;
    std::filesystem::path logFilePath( logPath );
    if ( logFilePath.has_parent_path() ) {
        std::error_code ec;
        std::filesystem::create_directories( logFilePath.parent_path(), ec );
    }
    gLogFile.open( logPath, std::ios::out | std::ios::app );

    if ( !gLogFile.is_open() ) {
        std::cerr << "[LOGGER] Failed to open log file: " << logPath << std::endl;
        return;
    }

    gIsInitialized = true;
    gLogFile << "[" << NowTimestamp() << "] [INFO] [LOGGER] Logger initialized. Output: " << logPath << std::endl;
}

void UtilLogger::Shutdown() {
    std::lock_guard< std::mutex > lock( gLogMutex );
    if ( !gIsInitialized ) {
        return;
    }

    gLogFile << "[" << NowTimestamp() << "] [INFO] [LOGGER] Logger shutdown." << std::endl;
    gLogFile.flush();
    gLogFile.close();
    gIsInitialized = false;
}

void UtilLogger::Log( LogLevel aLevel, const std::string& aCategory, const std::string& aMessage ) {
    std::lock_guard< std::mutex > lock( gLogMutex );
    if ( !gIsInitialized ) {
        return;
    }

    gLogFile << "[" << NowTimestamp() << "] [" << ToString( aLevel ) << "] [" << aCategory << "] " << aMessage << std::endl;
}

void UtilLogger::Info( const std::string& aCategory, const std::string& aMessage ) {
    Log( LogLevel::Info, aCategory, aMessage );
}

void UtilLogger::Warn( const std::string& aCategory, const std::string& aMessage ) {
    Log( LogLevel::Warning, aCategory, aMessage );
}

void UtilLogger::Error( const std::string& aCategory, const std::string& aMessage ) {
    Log( LogLevel::Error, aCategory, aMessage );
}

void UtilLogger::Debug( const std::string& aCategory, const std::string& aMessage ) {
    Log( LogLevel::Debug, aCategory, aMessage );
}
