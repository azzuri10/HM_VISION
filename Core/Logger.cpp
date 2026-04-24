#include "Logger.h"

namespace HMVision
{
void Logger::debug(const QString& message)
{
    log(Level::Debug, message);
}

void Logger::info(const QString& message)
{
    log(Level::Info, message);
}

void Logger::warning(const QString& message)
{
    log(Level::Warning, message);
}

void Logger::error(const QString& message)
{
    log(Level::Error, message);
}

void Logger::registerSink(LogSink sink)
{
    if (!sink)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(sinkMutex());
    sinks().push_back(std::move(sink));
}

void Logger::log(Level level, const QString& message)
{
    switch (level)
    {
    case Level::Debug:
        qDebug().noquote() << "[DEBUG]" << message;
        break;
    case Level::Info:
        qInfo().noquote() << "[INFO]" << message;
        break;
    case Level::Warning:
        qWarning().noquote() << "[WARN]" << message;
        break;
    case Level::Error:
        qCritical().noquote() << "[ERROR]" << message;
        break;
    }

    std::lock_guard<std::mutex> lock(sinkMutex());
    for (const auto& sink : sinks())
    {
        sink(level, message);
    }
}

std::mutex& Logger::sinkMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::vector<Logger::LogSink>& Logger::sinks()
{
    static std::vector<LogSink> s_sinks;
    return s_sinks;
}
} // namespace HMVision
