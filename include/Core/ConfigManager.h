#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace HMVision
{
class ConfigManager
{
public:
    static ConfigManager& getInstance();

    bool loadConfig(const std::string& filePath = "./config/system_config.json");
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

    bool startHotReload(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    void stopHotReload();
    bool reloadIfChanged();

    std::string configPath() const;

    ~ConfigManager();

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

private:
    ConfigManager();

    nlohmann::json buildDefaultConfig() const;

    mutable std::mutex m_mutex;
    nlohmann::json m_config;
    std::string m_configPath = "./config/system_config.json";
    std::filesystem::file_time_type m_lastWriteTime{};
    std::atomic<bool> m_hotReloadRunning{false};
    std::thread m_hotReloadThread;
};
} // namespace HMVision
