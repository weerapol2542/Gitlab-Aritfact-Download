#pragma once
#include <string>
#include <vector>
#include <memory>
#include <chrono>

class ConfigManager {
public:
    struct Settings {
        std::string gitlab_url;
        std::string api_token;
        std::string encrypted_api_token;
        std::string project_id;
        std::string download_path;
        std::vector<std::string> artifact_paths;
        std::time_t last_modified;

        Settings() : last_modified(std::time(nullptr)) {}
    };

    static ConfigManager& getInstance();
    bool loadSettings();
    bool saveSettings(const Settings& settings);
    const Settings& getSettings() const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    bool validateSettings(const Settings& settings) const;
    std::string getConfigFilePath() const;
    Settings current_settings_;
    static constexpr const char* CONFIG_FILE_NAME = "config.json";
};