#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace HMVision
{
struct SystemConfig
{
    int threadCount = 4;
    std::string logLevel = "INFO";
};

struct CameraConfig
{
    std::string id;
    std::string type;
    std::string ip;
    nlohmann::json params = nlohmann::json::object();
};

struct AlgorithmConfig
{
    std::string modelPath;
    float confidenceThreshold = 0.25F;
    float nmsThreshold = 0.45F;
};

class ConfigManager
{
public:
    static ConfigManager& getInstance();

    bool loadConfig(const std::string& filePath);
    bool saveConfig(const std::string& filePath = "") const;
    void generateDefaultConfig();

    template <typename T>
    T getValue(const std::string& key, const T& defaultValue) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        try
        {
            if (m_config.contains(key))
            {
                return m_config.at(key).get<T>();
            }
        }
        catch (...)
        {
        }
        return defaultValue;
    }

    template <typename T>
    void setValue(const std::string& key, const T& value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config[key] = value;
    }

    bool contains(const std::string& key) const;

    SystemConfig systemConfig() const;
    std::vector<CameraConfig> cameraConfigs() const;
    AlgorithmConfig algorithmConfig() const;

    void setSystemConfig(const SystemConfig& config);
    void setCameraConfigs(const std::vector<CameraConfig>& cameras);
    void setAlgorithmConfig(const AlgorithmConfig& config);

    bool startHotReload(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    void stopHotReload();
    bool reloadIfChanged();

    ~ConfigManager();

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

private:
    ConfigManager() = default;

    nlohmann::json buildDefaultConfig() const;

    mutable std::mutex m_mutex;
    nlohmann::json m_config;
    std::string m_configPath;
    std::filesystem::file_time_type m_lastWriteTime{};
    std::atomic<bool> m_hotReloadRunning{false};
    std::thread m_hotReloadThread;
};
} // namespace HMVision
