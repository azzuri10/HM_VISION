#include "../../include/Core/ConfigManager.h"

#include <fstream>

namespace HMVision
{
ConfigManager& ConfigManager::getInstance()
{
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager()
{
    generateDefaultConfig();
}

ConfigManager::~ConfigManager()
{
    stopHotReload();
}

bool ConfigManager::loadConfig(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return false;
    }

    nlohmann::json parsed;
    try
    {
        file >> parsed;
    }
    catch (...)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = std::move(parsed);
    m_configPath = filePath;
    if (std::filesystem::exists(m_configPath))
    {
        m_lastWriteTime = std::filesystem::last_write_time(m_configPath);
    }
    return true;
}

bool ConfigManager::saveConfig(const std::string& filePath) const
{
    std::string targetPath = filePath;
    nlohmann::json snapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (targetPath.empty())
        {
            targetPath = m_configPath;
        }
        snapshot = m_config;
    }

    if (targetPath.empty())
    {
        return false;
    }

    const std::filesystem::path pathObj(targetPath);
    if (!pathObj.parent_path().empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(pathObj.parent_path(), ec);
    }

    std::ofstream out(targetPath);
    if (!out.is_open())
    {
        return false;
    }

    out << snapshot.dump(4);
    return true;
}

void ConfigManager::generateDefaultConfig()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = buildDefaultConfig();
}

bool ConfigManager::startHotReload(std::chrono::milliseconds interval)
{
    if (m_hotReloadRunning.load())
    {
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_configPath.empty() || !std::filesystem::exists(m_configPath))
        {
            return false;
        }
    }

    m_hotReloadRunning.store(true);
    m_hotReloadThread = std::thread([this, interval]() {
        while (m_hotReloadRunning.load())
        {
            reloadIfChanged();
            std::this_thread::sleep_for(interval);
        }
    });
    return true;
}

void ConfigManager::stopHotReload()
{
    m_hotReloadRunning.store(false);
    if (m_hotReloadThread.joinable())
    {
        m_hotReloadThread.join();
    }
}

bool ConfigManager::reloadIfChanged()
{
    std::string currentPath;
    std::filesystem::file_time_type knownWriteTime{};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentPath = m_configPath;
        knownWriteTime = m_lastWriteTime;
    }

    if (currentPath.empty() || !std::filesystem::exists(currentPath))
    {
        return false;
    }

    const auto newWriteTime = std::filesystem::last_write_time(currentPath);
    if (newWriteTime <= knownWriteTime)
    {
        return false;
    }

    return loadConfig(currentPath);
}

std::string ConfigManager::configPath() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_configPath;
}

nlohmann::json ConfigManager::buildDefaultConfig() const
{
    nlohmann::json config;
    config["system"] = {
        {"name", "HM_VISION"},
        {"threadCount", 4},
        {"logLevel", "INFO"},
    };

    config["cameras"] = nlohmann::json::array(
        {{{"id", "usb-0"},
             {"type", "USB"},
             {"ip", ""},
             {"params", {{"index", 0}, {"exposure", 1000}, {"gain", 1.0}}}}});

    config["algorithms"] = {
        {"default", "yolov8"},
        {"detector", {{"modelPath", "models/yolov8.engine"}, {"confThreshold", 0.25}, {"nmsThreshold", 0.45}}},
        {"ocr", {{"modelDir", "models/paddleocr"}, {"language", "zh_en"}}},
    };

    config["communication"] = {
        {"plc", {{"protocol", "modbus_tcp"}, {"ip", "192.168.1.100"}, {"port", 502}}},
        {"mes", {{"baseUrl", "http://127.0.0.1:8080/api"}, {"timeoutMs", 3000}}},
    };

    return config;
}
} // namespace HMVision
