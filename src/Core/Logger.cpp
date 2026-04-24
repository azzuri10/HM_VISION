#include "../../include/Core/Logger.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace HMVision
{
Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::info(const std::string& message)
{
    getInstance().log(Level::Info, message, "", 0);
}

void Logger::warning(const std::string& message)
{
    getInstance().log(Level::Warning, message, "", 0);
}

void Logger::error(const std::string& message)
{
    getInstance().log(Level::Error, message, "", 0);
}

Logger::~Logger()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open())
    {
        m_file.flush();
        m_file.close();
    }
}

void Logger::initialize(const Config& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_activeDate = makeCurrentDate();
    m_rotationIndex = 0;
    m_initialized = true;

    if (m_config.fileOutput)
    {
        std::filesystem::create_directories(m_config.logDirectory);
        openLogFileIfNeeded();
    }
}

void Logger::setLevel(Level level)
{
    m_level.store(level);
}

Logger::Level Logger::level() const
{
    return m_level.load();
}

void Logger::log(Level level, const std::string& message, const char* file, int line)
{
    if (level < m_level.load())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized)
    {
        initialize();
    }

    rotateIfNeeded();
    const std::string record = makeTimestamp() + " [" + levelToString(level) + "] " + file + ":"
        + std::to_string(line) + " - " + message;

    if (m_config.consoleOutput)
    {
        std::cout << record << std::endl;
    }

    if (m_config.fileOutput)
    {
        openLogFileIfNeeded();
        if (m_file.is_open())
        {
            m_file << record << std::endl;
            m_file.flush();
        }
    }

    if (level == Level::Fatal)
    {
        throw std::runtime_error(record);
    }
}

void Logger::rotateIfNeeded()
{
    if (!m_config.fileOutput)
    {
        return;
    }

    const std::string today = makeCurrentDate();
    const bool dateChanged = m_config.rotateDaily && today != m_activeDate;
    const bool sizeExceeded = m_config.maxFileSizeBytes > 0
        && currentFileSize() >= m_config.maxFileSizeBytes;

    if (!dateChanged && !sizeExceeded)
    {
        return;
    }

    if (m_file.is_open())
    {
        m_file.flush();
        m_file.close();
    }

    if (dateChanged)
    {
        m_activeDate = today;
        m_rotationIndex = 0;
    }
    else
    {
        ++m_rotationIndex;
    }
}

void Logger::openLogFileIfNeeded()
{
    if (m_file.is_open())
    {
        return;
    }

    const std::string path = currentLogPath();
    m_file.open(path, std::ios::out | std::ios::app);
}

std::string Logger::makeCurrentDate() const
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

std::string Logger::makeTimestamp() const
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string Logger::levelToString(Level level) const
{
    switch (level)
    {
    case Level::Debug:
        return "DEBUG";
    case Level::Info:
        return "INFO";
    case Level::Warning:
        return "WARNING";
    case Level::Error:
        return "ERROR";
    case Level::Fatal:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

std::string Logger::currentLogPath() const
{
    std::filesystem::path dir(m_config.logDirectory);
    std::filesystem::path base(m_config.baseFileName);

    std::string stem = base.stem().string();
    std::string ext = base.extension().string();
    if (ext.empty())
    {
        ext = ".log";
    }

    std::ostringstream fileName;
    if (m_config.rotateDaily)
    {
        fileName << stem << "_" << m_activeDate;
    }
    else
    {
        fileName << stem;
    }

    if (m_rotationIndex > 0)
    {
        fileName << "_" << m_rotationIndex;
    }
    fileName << ext;

    return (dir / fileName.str()).string();
}

std::size_t Logger::currentFileSize() const
{
    const std::filesystem::path path(currentLogPath());
    if (!std::filesystem::exists(path))
    {
        return 0;
    }
    return static_cast<std::size_t>(std::filesystem::file_size(path));
}
} // namespace HMVision
