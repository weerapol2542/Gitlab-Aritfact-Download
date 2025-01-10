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

// In GitLabService.h, update the ArtifactDownloadJob struct
struct ArtifactDownloadJob {
    std::string job_id;
    std::string status;         // "started", "in_progress", "completed", "failed"
    size_t total_size;
    size_t downloaded_size;
    std::string message;
    std::string current_file;   // ชื่อไฟล์ที่กำลังดาวน์โหลด
    double progress;           // เปอร์เซ็นต์ความคืบหน้า
    std::time_t start_time;    // เวลาเริ่มต้นดาวน์โหลด
};

    static GitLabService& getInstance();
    std::vector<PipelineInfo> getRecentPipelines(int page = 1, int per_page = 10);
    std::vector<JobInfo> getPipelineJobs(int pipeline_id);
    std::string findJobIdForArtifact(int pipeline_id, const std::string& artifact_path);
    ArtifactDownloadJob startArtifactDownload(int pipeline_id, 
        const std::vector<std::string>& artifact_paths,
        const std::string& destination_path);
    ArtifactDownloadJob getDownloadStatus(const std::string& job_id);

private:

std::mutex log_mutex_;
    void logDownloadProgress(const std::string& stage, const std::string& message);
    GitLabService();
    ~GitLabService() = default;
    GitLabService(const GitLabService&) = delete;
    GitLabService& operator=(const GitLabService&) = delete;

    void downloadArtifactsAsync(
        int pipeline_id,
        const std::vector<std::string>& artifact_paths,
        const std::string& destination_path,
        const std::string& job_id
    );

    std::string buildApiUrl(const std::string& endpoint) const;
    drogon::HttpRequestPtr createAuthorizedRequest(drogon::HttpMethod method, const std::string& url) const;
    drogon::HttpRequestPtr createArtifactDownloadRequest(const std::string& url) const;  // เพิ่มบรรทัดนี้
    bool validateResponse(const drogon::HttpResponsePtr& response) const;
    
    std::shared_ptr<drogon::HttpClient> http_client_;
    std::unordered_map<std::string, ArtifactDownloadJob> download_jobs_;

    
};

