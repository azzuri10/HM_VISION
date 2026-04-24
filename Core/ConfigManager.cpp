#include "ConfigManager.h"

#include <fstream>

namespace HMVision
{
ConfigManager& ConfigManager::getInstance()
{
    static ConfigManager instance;
    return instance;
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
    if (std::filesystem::exists(filePath))
    {
        m_lastWriteTime = std::filesystem::last_write_time(filePath);
    }
    return true;
}

bool ConfigManager::saveConfig(const std::string& filePath) const
{
    std::string path = filePath;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (path.empty())
        {
            path = m_configPath;
        }
    }

    if (path.empty())
    {
        return false;
    }

    nlohmann::json snapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        snapshot = m_config;
    }

    std::ofstream file(path);
    if (!file.is_open())
    {
        return false;
    }
    file << snapshot.dump(4);
    return true;
}

void ConfigManager::generateDefaultConfig()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = buildDefaultConfig();
}

bool ConfigManager::contains(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.contains(key);
}

SystemConfig ConfigManager::systemConfig() const
{
    const auto section = getValue<nlohmann::json>("system", buildDefaultConfig().at("system"));
    SystemConfig result;
    result.threadCount = section.value("threadCount", 4);
    result.logLevel = section.value("logLevel", std::string("INFO"));
    return result;
}

std::vector<CameraConfig> ConfigManager::cameraConfigs() const
{
    std::vector<CameraConfig> result;
    const auto array = getValue<nlohmann::json>("cameras", nlohmann::json::array());
    if (!array.is_array())
    {
        return result;
    }

    for (const auto& item : array)
    {
        CameraConfig cfg;
        cfg.id = item.value("id", std::string());
        cfg.type = item.value("type", std::string("USB"));
        cfg.ip = item.value("ip", std::string());
        cfg.params = item.value("params", nlohmann::json::object());
        result.push_back(std::move(cfg));
    }
    return result;
}

AlgorithmConfig ConfigManager::algorithmConfig() const
{
    const auto section = getValue<nlohmann::json>("algorithm", buildDefaultConfig().at("algorithm"));
    AlgorithmConfig cfg;
    cfg.modelPath = section.value("modelPath", std::string("models/yolov8n.onnx"));
    cfg.confidenceThreshold = section.value("confidenceThreshold", 0.25F);
    cfg.nmsThreshold = section.value("nmsThreshold", 0.45F);
    return cfg;
}

void ConfigManager::setSystemConfig(const SystemConfig& config)
{
    nlohmann::json section;
    section["threadCount"] = config.threadCount;
    section["logLevel"] = config.logLevel;
    setValue("system", section);
}

void ConfigManager::setCameraConfigs(const std::vector<CameraConfig>& cameras)
{
    nlohmann::json array = nlohmann::json::array();
    for (const auto& camera : cameras)
    {
        nlohmann::json item;
        item["id"] = camera.id;
        item["type"] = camera.type;
        item["ip"] = camera.ip;
        item["params"] = camera.params;
        array.push_back(std::move(item));
    }
    setValue("cameras", array);
}

void ConfigManager::setAlgorithmConfig(const AlgorithmConfig& config)
{
    nlohmann::json section;
    section["modelPath"] = config.modelPath;
    section["confidenceThreshold"] = config.confidenceThreshold;
    section["nmsThreshold"] = config.nmsThreshold;
    setValue("algorithm", section);
}

bool ConfigManager::startHotReload(std::chrono::milliseconds interval)
{
    if (m_hotReloadRunning.load())
    {
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_configPath.empty())
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
    std::string path;
    std::filesystem::file_time_type knownTime;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        path = m_configPath;
        knownTime = m_lastWriteTime;
    }

    if (path.empty() || !std::filesystem::exists(path))
    {
        return false;
    }

    const auto newTime = std::filesystem::last_write_time(path);
    if (newTime <= knownTime)
    {
        return false;
    }

    return loadConfig(path);
}

nlohmann::json ConfigManager::buildDefaultConfig() const
{
    nlohmann::json defaultConfig;
    defaultConfig["system"] = {
        {"threadCount", 4},
        {"logLevel", "INFO"},
    };

    defaultConfig["cameras"] = nlohmann::json::array(
        {{{"id", "usb-0"}, {"type", "USB"}, {"ip", ""}, {"params", {{"index", 0}}}}});

    defaultConfig["algorithm"] = {
        {"modelPath", "models/yolov8n.onnx"},
        {"confidenceThreshold", 0.25},
        {"nmsThreshold", 0.45},
    };
    return defaultConfig;
}
} // namespace HMVision
