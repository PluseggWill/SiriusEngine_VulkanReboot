#pragma once

#include <string>

namespace UtilLogger {
// Ordered for min-level filtering: Debug is most verbose (lowest), Error is least.
enum class LogLevel {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
};

// Default: <repo>/Logs/engine_runtime_log.txt (truncated each run). Init calls VulkanDesktop/Scripts/RotateEngineLogs.bat to archive prior logs.
void Init( const std::string& aLogFilePath = "" );
void SetMinLogLevel( LogLevel aMinLevel );
LogLevel GetMinLogLevel();
void Shutdown();

void Log( LogLevel aLevel, const std::string& aCategory, const std::string& aMessage );

void Info( const std::string& aCategory, const std::string& aMessage );
void Warn( const std::string& aCategory, const std::string& aMessage );
void Error( const std::string& aCategory, const std::string& aMessage );
void Debug( const std::string& aCategory, const std::string& aMessage );
}  // namespace UtilLogger
