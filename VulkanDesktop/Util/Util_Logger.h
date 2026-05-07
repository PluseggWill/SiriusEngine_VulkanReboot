#pragma once

#include <string>

namespace UtilLogger {
enum class LogLevel {
    Info,
    Warning,
    Error,
    Debug,
};

void Init( const std::string& aLogFilePath );
void Shutdown();

void Log( LogLevel aLevel, const std::string& aCategory, const std::string& aMessage );

void Info( const std::string& aCategory, const std::string& aMessage );
void Warn( const std::string& aCategory, const std::string& aMessage );
void Error( const std::string& aCategory, const std::string& aMessage );
void Debug( const std::string& aCategory, const std::string& aMessage );
}  // namespace UtilLogger
