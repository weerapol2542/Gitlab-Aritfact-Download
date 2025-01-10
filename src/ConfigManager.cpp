#include "../include/ConfigManager.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <iostream>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadSettings() {
    try {
        std::string config_path = getConfigFilePath();
        std::cout << "Loading config from: " << config_path << std::endl;

        if (!std::filesystem::exists(config_path)) {
            std::cout << "Config file not found. Creating default config..." << std::endl;

            Settings default_settings;
            default_settings.gitlab_url = "https://gitlab.com";
            default_settings.api_token = "";
            default_settings.encrypted_api_token = "";
            default_settings.project_id = "";
            default_settings.download_path = "downloads";
            default_settings.artifact_paths = {"deploy/artifact.zip"};
    
            default_settings.last_modified = std::time(nullptr);

            if (!saveSettings(default_settings)) {
                std::cout << "Failed to create default config" << std::endl;
                return false;
            }
        }

        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            std::cout << "Failed to open config file: " << config_path << std::endl;
            return false;
        }

        nlohmann::json j;
        config_file >> j;

        Settings settings;

        // ใช้ value() เพื่อจัดการค่าที่เป็น null หรือไม่มีใน JSON
        settings.gitlab_url = j.value("gitlab_url", "https://gitlab.com");
        settings.api_token = j.value("api_token", "");
        settings.encrypted_api_token = j.value("encrypted_api_token", "");
        settings.project_id = j.value("project_id", "");
        settings.download_path = j.value("download_path", "downloads");
        settings.artifact_paths = j.value("artifact_paths", std::vector<std::string>{"deploy/artifact.zip"});
       
        settings.last_modified = std::time(nullptr);

        if (settings.gitlab_url.empty() || settings.project_id.empty()) {
            std::cout << "Critical settings (gitlab_url or project_id) are missing." << std::endl;
            return false;
        }

        if (validateSettings(settings)) {
            current_settings_ = settings;
            std::cout << "Settings loaded successfully" << std::endl;
            return true;
        }

        std::cout << "Invalid settings in config file" << std::endl;
        return false;

    } catch (const std::exception& e) {
        std::cout << "Error loading settings: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::saveSettings(const Settings& settings) {
    try {
        if (!validateSettings(settings)) {
            std::cout << "Invalid settings" << std::endl;
            return false;
        }

        auto config_path = std::filesystem::path(getConfigFilePath());
        if (!std::filesystem::exists(config_path.parent_path())) {
            std::filesystem::create_directories(config_path.parent_path());
        }

        nlohmann::json j;
        j["gitlab_url"] = settings.gitlab_url;
        j["api_token"] = settings.api_token;
        j["encrypted_api_token"] = settings.encrypted_api_token;
        j["project_id"] = settings.project_id;
        j["download_path"] = settings.download_path;
        j["artifact_paths"] = settings.artifact_paths;
       

        std::ofstream config_file(config_path);
        if (!config_file.is_open()) {
            std::cout << "Failed to open config file for writing" << std::endl;
            return false;
        }

        config_file << j.dump(4);
        current_settings_ = settings;

        std::cout << "Settings saved successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error saving settings: " << e.what() << std::endl;
        return false;
    }
}

const ConfigManager::Settings& ConfigManager::getSettings() const {
    return current_settings_;
}

bool ConfigManager::validateSettings(const Settings& settings) const {
    if (settings.gitlab_url.empty() || 
        settings.project_id.empty() || 
        settings.download_path.empty()) {
        return false;
    }

    try {
        // ตรวจสอบความถูกต้องของ path
        std::filesystem::path download_path(settings.download_path);
 

        // สร้างโฟลเดอร์ถ้ายังไม่มี
        if (!std::filesystem::exists(download_path)) {
            std::filesystem::create_directories(download_path);
        }
     

    } catch (const std::exception& e) {
        std::cout << "Error validating paths: " << e.what() << std::endl;
        return false;
    }

    return true;
}

std::string ConfigManager::getConfigFilePath() const {
    try {
        std::filesystem::path exe_path = std::filesystem::current_path();
        std::filesystem::path config_path = exe_path / "config" / CONFIG_FILE_NAME;
        return config_path.string();
    } catch (const std::exception& e) {
        std::cout << "Error getting config file path: " << e.what() << std::endl;
        return "";
    }
}
