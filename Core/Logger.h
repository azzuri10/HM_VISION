#pragma once

#include <QDebug>
#include <QString>

#include <functional>
#include <mutex>
#include <vector>

namespace HMVision
{
class Logger
{
public:
    enum class Level
    {
        Debug,
        Info,
        Warning,
        Error
    };

    using LogSink = std::function<void(Level level, const QString& message)>;

    static void debug(const QString& message);
    static void info(const QString& message);
    static void warning(const QString& message);
    static void error(const QString& message);
    static void registerSink(LogSink sink);

private:
    static void log(Level level, const QString& message);
    static std::mutex& sinkMutex();
    static std::vector<LogSink>& sinks();
};
} // namespace HMVision
