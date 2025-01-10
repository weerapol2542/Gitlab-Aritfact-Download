#include "../include/GitLabService.h"
#include "../include/ConfigManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <drogon/utils/Utilities.h>
#undef min
#include <mutex>

void GitLabService::pauseDownload(const std::string &job_id)
{
    try
    {
        std::lock_guard<std::mutex> lock(download_mutex_);
        if (auto it = download_jobs_.find(job_id); it != download_jobs_.end())
        {
            auto &job = it->second;
            if (job.status == "in_progress")
            {
                job.should_pause = true;
                // ไม่ต้องเปลี่ยนสถานะที่นี่ ปล่อยให้ thread ดาวน์โหลดจัดการ
                std::cout << "[Download] Pausing download for job " << job_id << std::endl;
            }
        }
        else
        {
            throw std::runtime_error("Download job not found");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error pausing download: " << e.what() << std::endl;
        throw;
    }
}

void GitLabService::resumeDownload(const std::string &job_id)
{
    try
    {
        std::lock_guard<std::mutex> lock(download_mutex_);
        if (auto it = download_jobs_.find(job_id); it != download_jobs_.end())
        {
            auto &job = it->second;
            if (job.status == "paused")
            {
                job.should_pause = false;
                // ไม่ต้องเปลี่ยนสถานะที่นี่ ปล่อยให้ thread ดาวน์โหลดจัดการ
                std::cout << "[Download] Resuming download for job " << job_id << std::endl;
            }
        }
        else
        {
            throw std::runtime_error("Download job not found");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error resuming download: " << e.what() << std::endl;
        throw;
    }
}

void GitLabService::cancelDownload(const std::string &job_id)
{
    try
    {
        std::lock_guard<std::mutex> lock(download_mutex_);
        if (auto it = download_jobs_.find(job_id); it != download_jobs_.end())
        {
            auto &job = it->second;
            if (job.status != "completed" && job.status != "failed")
            {
                job.should_cancel = true;
                job.status = "cancelled";
                std::cout << "[Download] Cancelling download for job " << job_id << std::endl;
            }
        }
        else
        {
            throw std::runtime_error("Download job not found");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error cancelling download: " << e.what() << std::endl;
        throw;
    }
}

GitLabService &GitLabService::getInstance()
{
    static GitLabService instance;
    return instance;
}

GitLabService::GitLabService()
{
    const auto &settings = ConfigManager::getInstance().getSettings();
    std::string host = settings.gitlab_url;
    if (host.find("https://") == 0)
    {
        host = host.substr(8);
    }
    else if (host.find("http://") == 0)
    {
        host = host.substr(7);
    }
    if (host.back() == '/')
    {
        host.pop_back();
    }
    http_client_ = drogon::HttpClient::newHttpClient("https://" + host);
}

std::vector<GitLabService::PipelineInfo> GitLabService::getRecentPipelines(int page, int per_page)
{
    std::vector<PipelineInfo> pipelines;

    try
    {
        const auto &settings = ConfigManager::getInstance().getSettings();
        if (settings.gitlab_url.empty() || settings.api_token.empty() || settings.project_id.empty())
        {
            std::cout << "Invalid GitLab settings" << std::endl;
            return pipelines;
        }

        auto url = buildApiUrl("/projects/" +
                               drogon::utils::urlEncodeComponent(settings.project_id) +
                               "/pipelines?page=" + std::to_string(page) +
                               "&per_page=" + std::to_string(per_page));

        auto request = createAuthorizedRequest(drogon::Get, url);
        auto promise = std::make_shared<std::promise<drogon::HttpResponsePtr>>();
        auto future = promise->get_future();

        http_client_->sendRequest(
            request,
            [promise](drogon::ReqResult result, const drogon::HttpResponsePtr &response)
            {
                promise->set_value(response);
            });

        auto response = future.get();

        if (!validateResponse(response))
        {
            if (response)
            {
                std::cout << "Invalid response from GitLab API. Status code: "
                          << response->getStatusCode() << std::endl;
            }
            return pipelines;
        }

        auto json = nlohmann::json::parse(std::string(response->getBody()));

        for (const auto &pipeline_json : json)
        {
            PipelineInfo pipeline;
            pipeline.id = pipeline_json["id"].get<int>();
            pipeline.status = pipeline_json["status"].get<std::string>();
            pipeline.created_at = pipeline_json["created_at"].get<std::string>();
            pipeline.ref = pipeline_json["ref"].get<std::string>();

            if (pipeline_json.contains("user") && !pipeline_json["user"].is_null())
            {
                pipeline.user.name = pipeline_json["user"]["name"].get<std::string>();
                pipeline.user.username = pipeline_json["user"]["username"].get<std::string>();
            }

            pipelines.push_back(pipeline);
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "Error fetching pipelines: " << e.what() << std::endl;
    }

    return pipelines;
}

std::vector<GitLabService::JobInfo> GitLabService::getPipelineJobs(int pipeline_id)
{
    std::vector<JobInfo> jobs;
    try
    {
        const auto &settings = ConfigManager::getInstance().getSettings();
        auto url = buildApiUrl("/projects/" + settings.project_id +
                               "/pipelines/" + std::to_string(pipeline_id) + "/jobs");

        auto request = createAuthorizedRequest(drogon::Get, url);
        auto promise = std::make_shared<std::promise<drogon::HttpResponsePtr>>();
        auto future = promise->get_future();

        http_client_->sendRequest(
            request,
            [promise](drogon::ReqResult result, const drogon::HttpResponsePtr &response)
            {
                promise->set_value(response);
            });

        auto response = future.get();
        if (!validateResponse(response))
        {
            std::cout << "Failed to get valid response from jobs API" << std::endl;
            return jobs;
        }

        auto jobs_json = nlohmann::json::parse(std::string(response->getBody()));

        for (const auto &job_json : jobs_json)
        {
            JobInfo job;
            if (job_json.contains("id") && !job_json["id"].is_null())
            {
                job.id = job_json["id"].get<int64_t>();
            }
            else
            {
                continue;
            }

            job.name = job_json["name"].get<std::string>();
            job.status = job_json["status"].get<std::string>();

            if (job_json.contains("artifacts") && job_json["artifacts"].is_array())
            {
                for (const auto &artifact : job_json["artifacts"])
                {
                    if (artifact.contains("filename"))
                    {
                        job.artifacts.push_back(artifact["filename"].get<std::string>());
                    }
                }
            }
            jobs.push_back(job);
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "Error fetching pipeline jobs: " << e.what() << std::endl;
    }
    return jobs;
}

void GitLabService::logDownloadProgress(const std::string &stage, const std::string &message)
{
    std::time_t currentTime = std::time(nullptr);
    std::string timestamp = std::ctime(&currentTime);
    timestamp.pop_back(); // Remove trailing newline

    std::lock_guard<std::mutex> lock(log_mutex_); // Protect concurrent console output
    std::cout << timestamp << " [" << std::setw(10) << std::left << stage << "] "
              << message << std::endl;
}

GitLabService::ArtifactDownloadJob GitLabService::startArtifactDownload(
    int pipeline_id,
    const std::vector<std::string> &artifact_paths,
    const std::string &destination_path)
{
    std::string download_id = std::to_string(std::time(nullptr));

    ArtifactDownloadJob job{
        download_id,
        "started",
        0,
        0,
        "Initiating download...",
        "",
        0.0,
        std::time(nullptr),
        false, // should_pause
        false  // should_cancel
    };

    download_jobs_[download_id] = job;
    const auto &settings = ConfigManager::getInstance().getSettings();

    auto download_task = [this, pipeline_id, artifact_paths, destination_path, download_id, &settings]()
    {
        try
        {
            std::cout << "[Download Started] Pipeline #" << pipeline_id << std::endl;
            std::filesystem::create_directories(destination_path);
            auto jobs = getPipelineJobs(pipeline_id);
            size_t total_size = 0;
            size_t current_total_downloaded = 0;
            std::vector<std::string> missing_files;
            std::vector<std::pair<std::string, size_t>> downloaded_files;
            size_t file_index = 1;

            std::string temp_dir = destination_path + "\\temp_" + download_id;
            std::filesystem::create_directories(temp_dir);

            auto &job = download_jobs_[download_id];
            job.status = "in_progress";

            // First pass: Calculate total size
            for (const auto &artifact_path : artifact_paths)
            {
                std::cout << "[Checking] " << artifact_path << std::endl;
                bool artifact_found = false;

                for (const auto &job_info : jobs)
                {
                    try
                    {
                        auto url = buildApiUrl("/projects/" + settings.project_id +
                                               "/jobs/" + std::to_string(job_info.id) +
                                               "/artifacts/" + artifact_path);

                        auto request = createArtifactDownloadRequest(url);
                        auto promise = std::make_shared<std::promise<drogon::HttpResponsePtr>>();
                        auto future = promise->get_future();

                        http_client_->sendRequest(
                            request,
                            [promise](drogon::ReqResult result, const drogon::HttpResponsePtr &response)
                            {
                                promise->set_value(response);
                            });

                        auto response = future.get();

                        if (validateResponse(response))
                        {
                            artifact_found = true;
                            const auto &body = response->getBody();
                            total_size += body.length();
                            std::cout << "[Size] " << artifact_path << ": " << body.length() << " bytes" << std::endl;
                            break;
                        }
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "[Error] Checking artifact " << artifact_path
                                  << ": " << e.what() << std::endl;
                        continue;
                    }
                }

                if (!artifact_found)
                {
                    std::cout << "[Missing] " << artifact_path << std::endl;
                    missing_files.push_back(artifact_path);
                }
            }

            job.total_size = total_size;
            std::cout << "[Total Size] " << total_size << " bytes" << std::endl;

            // Second pass: Download files
            for (const auto &artifact_path : artifact_paths)
            {
                if (job.should_cancel)
                {
                    job.status = "cancelled";
                    if (std::filesystem::exists(temp_dir))
                    {
                        std::filesystem::remove_all(temp_dir);
                    }
                    return;
                }

                while (job.should_pause)
                {
                    job.status = "paused";
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    if (job.should_cancel)
                    {
                        job.status = "cancelled";
                        if (std::filesystem::exists(temp_dir))
                        {
                            std::filesystem::remove_all(temp_dir);
                        }
                        return;
                    }
                }

                job.status = "in_progress";

                for (const auto &job_info : jobs)
                {
                    try
                    {
                        auto url = buildApiUrl("/projects/" + settings.project_id +
                                               "/jobs/" + std::to_string(job_info.id) +
                                               "/artifacts/" + artifact_path);

                        auto request = createArtifactDownloadRequest(url);
                        auto promise = std::make_shared<std::promise<drogon::HttpResponsePtr>>();
                        auto future = promise->get_future();

                        http_client_->sendRequest(
                            request,
                            [promise](drogon::ReqResult result, const drogon::HttpResponsePtr &response)
                            {
                                promise->set_value(response);
                            });

                        auto response = future.get();

                        if (validateResponse(response))
                        {
                            std::filesystem::path artifact_fs_path(artifact_path);
                            auto save_path = std::filesystem::path(temp_dir) / artifact_fs_path.filename();
                            std::ofstream file(save_path.string(), std::ios::binary);

                            // ในฟังก์ชัน startArtifactDownload แก้ไขส่วนการดาวน์โหลด:

                            if (file.is_open())
                            {
                                const auto &body = response->getBody();
                                // เพิ่มขนาด chunk size เป็น 1MB
                                const size_t chunk_size = 1024 * 1024;
                                const char *data = body.data();
                                const size_t data_size = body.length();

                                job.current_file = artifact_fs_path.filename().string();

                                for (size_t i = 0; i < data_size; i += chunk_size)
                                {
                                    if (job.should_cancel)
                                    {
                                        file.close();
                                        if (std::filesystem::exists(temp_dir))
                                        {
                                            std::filesystem::remove_all(temp_dir);
                                        }
                                        job.status = "cancelled";
                                        return;
                                    }

                                    // ตรวจสอบสถานะ pause
                                    while (job.should_pause)
                                    {
                                        job.status = "paused";
                                        download_jobs_[download_id] = job; // อัพเดทสถานะทันที
                                        std::this_thread::sleep_for(std::chrono::milliseconds(100));

                                        if (job.should_cancel)
                                        {
                                            file.close();
                                            if (std::filesystem::exists(temp_dir))
                                            {
                                                std::filesystem::remove_all(temp_dir);
                                            }
                                            job.status = "cancelled";
                                            return;
                                        }
                                    }

                                    job.status = "in_progress";
                                    size_t current_chunk = std::min(chunk_size, data_size - i);
                                    file.write(data + i, current_chunk);
                                    job.downloaded_size = current_total_downloaded + i + current_chunk;

                                    double progress = (static_cast<double>(job.downloaded_size) / job.total_size) * 100.0;
                                    job.progress = progress;

                                    // อัพเดท UI ทุก 4MB
                                    if (i % (chunk_size * 4) == 0)
                                    {
                                        download_jobs_[download_id] = job;
                                    }
                                }

                                file.close();
                                current_total_downloaded += data_size;
                                downloaded_files.push_back({artifact_path, data_size});
                                break;
                            }
                            {
                                const auto &body = response->getBody();
                                const size_t chunk_size = 8192;
                                const char *data = body.data();
                                const size_t data_size = body.length();

                                job.current_file = artifact_fs_path.filename().string();

                                for (size_t i = 0; i < data_size; i += chunk_size)
                                {
                                    if (job.should_cancel)
                                    {
                                        file.close();
                                        if (std::filesystem::exists(temp_dir))
                                        {
                                            std::filesystem::remove_all(temp_dir);
                                        }
                                        job.status = "cancelled";
                                        return;
                                    }

                                    while (job.should_pause)
                                    {
                                        job.status = "paused";
                                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                        if (job.should_cancel)
                                        {
                                            file.close();
                                            if (std::filesystem::exists(temp_dir))
                                            {
                                                std::filesystem::remove_all(temp_dir);
                                            }
                                            job.status = "cancelled";
                                            return;
                                        }
                                    }

                                    job.status = "in_progress";
                                    size_t current_chunk = std::min(chunk_size, data_size - i);
                                    file.write(data + i, current_chunk);
                                    job.downloaded_size = current_total_downloaded + i + current_chunk;

                                    double progress = (static_cast<double>(job.downloaded_size) / job.total_size) * 100.0;
                                    job.progress = progress;

                                    // Update UI less frequently to reduce overhead
                                    if (i % (chunk_size * 4) == 0)
                                    {
                                        download_jobs_[download_id] = job;
                                    }
                                }

                                file.close();
                                current_total_downloaded += data_size;
                                downloaded_files.push_back({artifact_path, data_size});
                                break;
                            }
                        }
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "[Error] Downloading " << artifact_path
                                  << ": " << e.what() << std::endl;
                        continue;
                    }
                }
                file_index++;
            }

            // Create completion message and handle cleanup
            if (!downloaded_files.empty())
            {
                job.message = "Creating artifacts.zip...";
                std::cout << "[Creating] artifacts.zip" << std::endl;

                std::filesystem::path final_zip_path = std::filesystem::path(destination_path) / "artifacts.zip";
                std::string powershell_command = "powershell -Command \"Compress-Archive -Path '" +
                                                 std::filesystem::path(temp_dir).string() + "\\*' -DestinationPath '" +
                                                 final_zip_path.string() + "' -Force\"";

                int zip_result = std::system(powershell_command.c_str());
                if (zip_result != 0)
                {
                    throw std::runtime_error("Failed to create artifacts.zip");
                }

                // Create completion message
                std::stringstream message;
                if (!missing_files.empty())
                {
                    message << "Successfully downloaded " << downloaded_files.size() << " file(s). "
                            << "The following files were not found:\n";
                    for (const auto &file : missing_files)
                    {
                        message << "- " << file << "\n";
                    }
                    job.status = "partial_success";
                    std::cout << "[Status] Partial Success" << std::endl;
                }
                else
                {
                    message << "All files downloaded successfully.";
                    job.status = "completed";
                    std::cout << "[Status] Completed" << std::endl;
                }

                // Add download details
                nlohmann::json download_info;
                download_info["downloaded_files"] = nlohmann::json::array();
                for (const auto &[name, size] : downloaded_files)
                {
                    download_info["downloaded_files"].push_back({{"name", name},
                                                                 {"size", size}});
                }
                download_info["missing_files"] = missing_files;
                download_info["total_size"] = total_size;
                download_info["total_files"] = artifact_paths.size();

                job.progress = 100.0;
                job.message = message.str() + "\n" + download_info.dump();
            }
            else
            {
                throw std::runtime_error("No files were found in pipeline #" + std::to_string(pipeline_id));
            }

            // Cleanup
            if (std::filesystem::exists(temp_dir))
            {
                std::filesystem::remove_all(temp_dir);
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Error] Download task error: " << e.what() << std::endl;
            auto &job = download_jobs_[download_id];
            job.status = "failed";
            job.message = std::string("Download failed: ") + e.what();

            std::string temp_dir = destination_path + "\\temp_" + download_id;
            if (std::filesystem::exists(temp_dir))
            {
                std::filesystem::remove_all(temp_dir);
            }
        }
    };

    std::thread(download_task).detach();
    return job;
}

GitLabService::ArtifactDownloadJob GitLabService::getDownloadStatus(const std::string &job_id)
{
    std::lock_guard<std::mutex> lock(download_mutex_);
    if (auto it = download_jobs_.find(job_id); it != download_jobs_.end())
    {
        auto &job = it->second;

        // ลดการแสดง debug info เมื่อสถานะเป็น completed หรือ partial_success
        if (job.status != "completed" && job.status != "partial_success")
        {
            nlohmann::json debug_info;
            debug_info["job_id"] = job.job_id;
            debug_info["status"] = job.status;
            debug_info["progress"] = job.progress;
            debug_info["current_file"] = job.current_file;
            debug_info["downloaded_size"] = job.downloaded_size;
            debug_info["total_size"] = job.total_size;
            std::cout << "Sending status: " << debug_info.dump(2) << std::endl;
        }
        return job;
    }
    return ArtifactDownloadJob{
        job_id,
        "not_found",
        0,
        0,
        "Download job not found",
        "",
        0.0,
        std::time(nullptr)};
}

std::string GitLabService::buildApiUrl(const std::string &endpoint) const
{
    const auto &settings = ConfigManager::getInstance().getSettings();
    std::string base_url = settings.gitlab_url;

    if (!base_url.empty() && base_url.back() == '/')
    {
        base_url.pop_back();
    }

    return base_url + "/api/v4" + endpoint;
}

drogon::HttpRequestPtr GitLabService::createAuthorizedRequest(
    drogon::HttpMethod method,
    const std::string &url) const
{
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(method);

    size_t pos = url.find("/api/v4");
    if (pos != std::string::npos)
    {
        std::string path = url.substr(pos);
        request->setPath(path);
    }
    else
    {
        request->setPath(url);
    }

    const auto &settings = ConfigManager::getInstance().getSettings();
    request->addHeader("PRIVATE-TOKEN", settings.api_token);
    request->addHeader("Accept", "application/json");

    return request;
}

drogon::HttpRequestPtr GitLabService::createArtifactDownloadRequest(
    const std::string &url) const
{
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);

    size_t pos = url.find("/api/v4");
    if (pos != std::string::npos)
    {
        std::string path = url.substr(pos);
        request->setPath(path);
    }
    else
    {
        request->setPath(url);
    }

    const auto &settings = ConfigManager::getInstance().getSettings();
    request->addHeader("PRIVATE-TOKEN", settings.api_token);
    request->addHeader("Accept", "*/*");

    return request;
}

bool GitLabService::validateResponse(const drogon::HttpResponsePtr &response) const
{
    if (!response)
    {
        return false;
    }

    int status_code = response->getStatusCode();
    return status_code >= 200 && status_code < 300;
}