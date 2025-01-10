// include/FileManager.h
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>

class FileManager {
public:
    struct DownloadStatus {
        std::string id;
        size_t total_bytes;
        size_t bytes_downloaded;
        std::string status;
        std::string message;
        double progress_percentage;
    };

    struct DownloadConfig {
        std::string destination_path;
        std::vector<std::string> file_patterns;
        std::function<void(const DownloadStatus&)> progress_callback;
    };

    static FileManager& getInstance();
    bool createDirectory(const std::string& path);
    bool removeDirectory(const std::string& path);
    bool fileExists(const std::string& path) const;
    std::vector<std::string> listFiles(const std::string& directory, const std::string& pattern) const;
    std::string startDownload(const std::string& url, const DownloadConfig& config);
    DownloadStatus getDownloadStatus(const std::string& download_id) const;
    bool cancelDownload(const std::string& download_id);
    void cleanupDownload(const std::string& download_id);

private:
    FileManager() = default;
    ~FileManager() = default;
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

    bool validatePath(const std::string& path) const;
    std::string generateDownloadId() const;
    void updateDownloadStatus(const std::string& download_id, const DownloadStatus& status);
    
    mutable std::mutex downloads_mutex_;
    std::unordered_map<std::string, DownloadStatus> downloads_;
    std::unordered_map<std::string, bool> active_downloads_;
};