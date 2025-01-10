#pragma once
#include <string>
#include <vector>
#include <memory>
#include <future>
#include <unordered_map>
#include <drogon/HttpClient.h>
#include <nlohmann/json.hpp>
#include <mutex>
#include <iomanip>
#include <ctime>

class GitLabService {
public:
    struct PipelineInfo {
        int id;
        std::string status;
        std::string created_at;
        std::string ref;
        struct {
            std::string name;
            std::string username;
        } user;
    };

    struct JobInfo {
        int64_t id;
        std::string name;
        std::string status;
        std::vector<std::string> artifacts;
    };

    struct ArtifactDownloadJob {
        std::string job_id;
        std::string status;         // "started", "in_progress", "paused", "completed", "failed", "cancelled"
        size_t total_size;
        size_t downloaded_size;
        std::string message;
        std::string current_file;
        double progress;
        std::time_t start_time;
        bool should_pause{false};   // Control flag for pausing
        bool should_cancel{false};  // Control flag for cancelling
    };

    // Singleton instance
    static GitLabService& getInstance();

    // Pipeline operations
    std::vector<PipelineInfo> getRecentPipelines(int page = 1, int per_page = 10);
    std::vector<JobInfo> getPipelineJobs(int pipeline_id);
    
    // Download operations
    ArtifactDownloadJob startArtifactDownload(
        int pipeline_id,
        const std::vector<std::string>& artifact_paths,
        const std::string& destination_path);
    
    ArtifactDownloadJob getDownloadStatus(const std::string& job_id);
    
    // Download control functions
    void pauseDownload(const std::string& job_id);
    void resumeDownload(const std::string& job_id);
    void cancelDownload(const std::string& job_id);

private:
    // Constructor and assignment operators
    GitLabService();
    ~GitLabService() = default;
    GitLabService(const GitLabService&) = delete;
    GitLabService& operator=(const GitLabService&) = delete;

    // Mutexes for thread safety
    std::mutex download_mutex_;  
    std::mutex log_mutex_;
    
    // Member variables
    std::unordered_map<std::string, ArtifactDownloadJob> download_jobs_;
    std::shared_ptr<drogon::HttpClient> http_client_;
    
    // Helper functions
    void logDownloadProgress(const std::string& stage, const std::string& message);
    std::string buildApiUrl(const std::string& endpoint) const;
    drogon::HttpRequestPtr createAuthorizedRequest(drogon::HttpMethod method, const std::string& url) const;
    drogon::HttpRequestPtr createArtifactDownloadRequest(const std::string& url) const;
    bool validateResponse(const drogon::HttpResponsePtr& response) const;
};