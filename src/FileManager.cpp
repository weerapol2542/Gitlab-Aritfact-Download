// src/FileManager.cpp
#include "../include/FileManager.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <chrono>
#include <iostream>
#include <drogon/HttpClient.h>

FileManager& FileManager::getInstance() {
    static FileManager instance;
    return instance;
}

bool FileManager::createDirectory(const std::string& path) {
    try {
        std::filesystem::path abs_path = std::filesystem::absolute(path);
        std::cout << "Creating directory at: " << abs_path << std::endl;

        if (!std::filesystem::exists(abs_path)) {
            std::cout << "Directory doesn't exist, creating..." << std::endl;
            return std::filesystem::create_directories(abs_path);
        }
        
        std::cout << "Directory already exists" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error creating directory: " << e.what() << std::endl;
        return false;
    }
}

bool FileManager::removeDirectory(const std::string& path) {
    if (!validatePath(path)) {
        return false;
    }

    try {
        return std::filesystem::remove_all(path) > 0;
    } catch (const std::exception& e) {
        std::cout << "Error removing directory: " << e.what() << std::endl;
        return false;
    }
}

bool FileManager::fileExists(const std::string& path) const {
    if (!validatePath(path)) {
        return false;
    }
    return std::filesystem::exists(path);
}

std::vector<std::string> FileManager::listFiles(
    const std::string& directory,
    const std::string& pattern
) const {
    std::vector<std::string> matching_files;

    if (!validatePath(directory) || !std::filesystem::exists(directory)) {
        return matching_files;
    }

    try {
        std::regex file_pattern(pattern);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (std::regex_match(filename, file_pattern)) {
                    matching_files.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Error listing files: " << e.what() << std::endl;
    }

    return matching_files;
}

std::string FileManager::startDownload(
    const std::string& url,
    const DownloadConfig& config
) {
    if (!validatePath(config.destination_path)) {
        return "";
    }

    std::string download_id = generateDownloadId();
    
    DownloadStatus initial_status{
        download_id,
        0,
        0,
        "pending",
        "Initiating download...",
        0.0
    };

    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);
        downloads_[download_id] = initial_status;
        active_downloads_[download_id] = true;
    }

    // Start asynchronous download
    auto download_task = [this, url, config, download_id]() {
        try {
            createDirectory(config.destination_path);

            auto client = drogon::HttpClient::newHttpClient(url);
            auto req = drogon::HttpRequest::newHttpRequest();
            req->setMethod(drogon::Get);

            auto promise = std::make_shared<std::promise<drogon::HttpResponsePtr>>();
            auto future = promise->get_future();

            client->sendRequest(req,
                [promise](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) {
                    promise->set_value(resp);
                });

            auto response = future.get();

            if (!response || response->getStatusCode() != 200) {
                updateDownloadStatus(download_id, {
                    download_id,
                    0,
                    0,
                    "failed",
                    "Failed to download file",
                    0.0
                });
                return;
            }

            const auto& body = response->getBody();
            size_t total_size = body.length();
            size_t bytes_written = 0;
            
            std::filesystem::path destination = std::filesystem::path(config.destination_path) / 
                                              std::filesystem::path(url).filename();

            std::ofstream file(destination, std::ios::binary);
            if (!file) {
                updateDownloadStatus(download_id, {
                    download_id,
                    total_size,
                    0,
                    "failed",
                    "Failed to create output file",
                    0.0
                });
                return;
            }

            const size_t chunk_size = 8192;

            for (size_t i = 0; i < body.length(); i += chunk_size) {
                if (!active_downloads_[download_id]) {
                    file.close();
                    std::filesystem::remove(destination);
                    return;
                }

                size_t remaining = body.length() - i;
                size_t current_chunk = (remaining < chunk_size) ? remaining : chunk_size;
                
                file.write(body.data() + i, current_chunk);
                bytes_written += current_chunk;

                double progress = (bytes_written * 100.0) / total_size;

                updateDownloadStatus(download_id, {
                    download_id,
                    total_size,
                    bytes_written,
                    "downloading",
                    "Download in progress...",
                    progress
                });

                if (config.progress_callback) {
                    config.progress_callback(getDownloadStatus(download_id));
                }
            }

            file.close();

            updateDownloadStatus(download_id, {
                download_id,
                total_size,
                total_size,
                "completed",
                "Download completed successfully",
                100.0
            });

        } catch (const std::exception& e) {
            updateDownloadStatus(download_id, {
                download_id,
                0,
                0,
                "failed",
                std::string("Download failed: ") + e.what(),
                0.0
            });
        }
    };

    std::thread(download_task).detach();
    return download_id;
}

FileManager::DownloadStatus FileManager::getDownloadStatus(const std::string& download_id) const {
    std::lock_guard<std::mutex> lock(downloads_mutex_);
    
    auto it = downloads_.find(download_id);
    if (it != downloads_.end()) {
        return it->second;
    }

    return DownloadStatus{
        download_id,
        0,
        0,
        "not_found",
        "Download not found",
        0.0
    };
}

bool FileManager::cancelDownload(const std::string& download_id) {
    std::lock_guard<std::mutex> lock(downloads_mutex_);
    
    auto it = active_downloads_.find(download_id);
    if (it != active_downloads_.end()) {
        it->second = false;
        return true;
    }
    return false;
}

void FileManager::cleanupDownload(const std::string& download_id) {
    std::lock_guard<std::mutex> lock(downloads_mutex_);
    downloads_.erase(download_id);
    active_downloads_.erase(download_id);
}

bool FileManager::validatePath(const std::string& path) const {
    if (path.empty()) {
        return false;
    }

    try {
        std::filesystem::path fs_path(path);
        return !fs_path.empty();
    } catch (const std::exception&) {
        return false;
    }
}

std::string FileManager::generateDownloadId() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    return "dl_" + std::to_string(timestamp);
}

void FileManager::updateDownloadStatus(
    const std::string& download_id,
    const DownloadStatus& status
) {
    std::lock_guard<std::mutex> lock(downloads_mutex_);
    downloads_[download_id] = status;
}