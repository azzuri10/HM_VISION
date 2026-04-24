#pragma once

#include <atomic>
#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>

namespace HMVision
{
class Logger
{
public:
    enum class Level
    {
        Debug = 0,
        Info,
        Warning,
        Error,
        Fatal
    };

    struct Config
    {
        bool consoleOutput = true;
        bool fileOutput = true;
        std::string logDirectory = "./logs";
        std::string baseFileName = "hmvision.log";
        std::size_t maxFileSizeBytes = 10 * 1024 * 1024;
        bool rotateDaily = true;
    };

    static Logger& getInstance();

    void initialize(const Config& config = Config());
    void setLevel(Level level);
    Level level() const;

    void log(Level level, const std::string& message, const char* file, int line);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void rotateIfNeeded();
    void openLogFileIfNeeded();
    std::string makeCurrentDate() const;
    std::string makeTimestamp() const;
    std::string levelToString(Level level) const;
    std::string currentLogPath() const;
    std::size_t currentFileSize() const;

    mutable std::mutex m_mutex;
    std::ofstream m_file;
    Config m_config;
    std::atomic<Level> m_level{Level::Info};
    bool m_initialized = false;
    std::string m_activeDate;
    int m_rotationIndex = 0;
};
} // namespace HMVision
