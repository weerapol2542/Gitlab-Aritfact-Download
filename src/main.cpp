#include <drogon/drogon.h>
#include "../include/GitLabService.h"
#include "../include/FileManager.h"
#include "../include/ConfigManager.h"
#include <iostream>

void setupControllers(drogon::HttpAppFramework &app)
{

    app.registerHandler("/api/{path}",
                        [](const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&callback)
                        {
                            auto resp = drogon::HttpResponse::newHttpResponse();
                            resp->setStatusCode(drogon::k204NoContent);
                            callback(resp);
                             return;
                        },
                        {drogon::Options});
    // GET /api/pipelines - Get recent pipelines
    app.registerHandler(
        "/api/pipelines",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback)
        {
            try
            {

                if (!ConfigManager::getInstance().loadSettings())
                {
                    std::cerr << "Failed to load configuration. Please check config.json" << std::endl;
                    return 1;
                }
                auto page = req->getParameter("page");
                if (page.empty())
                {
                    page = "1"; // กำหนดค่าเริ่มต้น
                }

                auto per_page = req->getParameter("per_page");
                if (per_page.empty())
                {
                    per_page = "10"; // กำหนดค่าเริ่มต้น
                }

                auto pipelines = GitLabService::getInstance().getRecentPipelines(
                    std::stoi(page),
                    std::stoi(per_page));

                nlohmann::json response;
                nlohmann::json pipeline_array = nlohmann::json::array();

                for (const auto &pipeline : pipelines)
                {
                    nlohmann::json pipeline_json;
                    pipeline_json["id"] = pipeline.id;
                    pipeline_json["status"] = pipeline.status;
                    pipeline_json["created_at"] = pipeline.created_at;
                    pipeline_json["ref"] = pipeline.ref;

                    nlohmann::json user;
                    user["name"] = pipeline.user.name;
                    user["username"] = pipeline.user.username;
                    pipeline_json["user"] = user;

                    pipeline_array.push_back(pipeline_json);
                }

                response["pipelines"] = pipeline_array;
                response["page"] = std::stoi(page);
                response["per_page"] = std::stoi(per_page);

                auto resp = drogon::HttpResponse::newHttpJsonResponse(response.dump());
                callback(resp);
            }
            catch (const std::exception &e)
            {
                nlohmann::json error;
                error["error"] = true;
                error["message"] = std::string("Failed to fetch pipelines: ") + e.what();

                auto error_resp = drogon::HttpResponse::newHttpJsonResponse(error.dump());
                error_resp->setStatusCode(drogon::k500InternalServerError);
                callback(error_resp);
            }
        },
        {drogon::Get});

    // POST /api/artifacts/download - Start artifact download
    app.registerHandler(
        "/api/artifacts/download",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback)
        {
            try
            {
                auto json = nlohmann::json::parse(std::string(req->getBody()));
                int pipeline_id = json["pipeline_id"].get<int>();
                auto artifacts = json["artifacts"].get<std::vector<std::string>>();

                FileManager::DownloadConfig config;
                config.destination_path = ConfigManager::getInstance().getSettings().download_path;
                config.file_patterns = artifacts;
                config.progress_callback = [](const FileManager::DownloadStatus &status)
                {
                    std::cout << "Download progress: " << status.progress_percentage << "%" << std::endl;
                };

                auto download_job = GitLabService::getInstance().startArtifactDownload(
                    pipeline_id,
                    artifacts,
                    config.destination_path);

                nlohmann::json response;
                response["job_id"] = download_job.job_id;
                response["status"] = download_job.status;
                response["estimated_size"] = download_job.total_size;

                auto resp = drogon::HttpResponse::newHttpJsonResponse(response.dump());
                callback(resp);
            }
            catch (const std::exception &e)
            {
                nlohmann::json error;
                error["error"] = true;
                error["message"] = std::string("Failed to start download: ") + e.what();

                auto error_resp = drogon::HttpResponse::newHttpJsonResponse(error.dump());
                error_resp->setStatusCode(drogon::k500InternalServerError);
                callback(error_resp);
            }
        },
        {drogon::Post});

    // GET /api/artifacts/status/{job_id}
    app.registerHandler(
    "/api/artifacts/status/{job_id}",
    [](const drogon::HttpRequestPtr &req,
       std::function<void(const drogon::HttpResponsePtr &)> &&callback,
       const std::string &job_id)
    {
        try {
            auto status = GitLabService::getInstance().getDownloadStatus(job_id);

            nlohmann::json response;
            response["status"] = status.status;
            response["progress"] = status.progress;
            response["bytes_downloaded"] = status.downloaded_size;
            response["total_bytes"] = status.total_size;
            response["message"] = status.message;
            response["current_file"] = status.current_file;
            // Add this line to ensure consistent file path reporting
            response["file_path"] = std::filesystem::path(status.current_file).string();
            response["start_time"] = status.start_time;



            auto resp = drogon::HttpResponse::newHttpJsonResponse(response.dump());
            callback(resp);
        } catch (const std::exception &e) {
            // Error handling remains the same
        }
    },
    {drogon::Get});



    app.registerHandler(
    "/api/artifacts/pause/{job_id}",
    [](const drogon::HttpRequestPtr &req,
       std::function<void(const drogon::HttpResponsePtr &)> &&callback,
       const std::string &job_id)
    {
        try {
            auto& job = GitLabService::getInstance().getDownloadStatus(job_id);
            job.status = (job.status == "paused") ? "in_progress" : "paused";
            
            nlohmann::json response;
            response["success"] = true;
            response["status"] = job.status;
            
            auto resp = drogon::HttpResponse::newHttpJsonResponse(response.dump());
            callback(resp);
        } catch (const std::exception &e) {
            // Error handling
            auto error_resp = drogon::HttpResponse::newHttpJsonResponse(
                nlohmann::json{{"error", true}, {"message", e.what()}}.dump()
            );
            error_resp->setStatusCode(drogon::k500InternalServerError);
            callback(error_resp);
        }
    },
    {drogon::Post});

    app.registerHandler(
    "/api/artifacts/cancel/{job_id}",
    [](const drogon::HttpRequestPtr &req,
       std::function<void(const drogon::HttpResponsePtr &)> &&callback,
       const std::string &job_id)
    {
        try {
            auto& job = GitLabService::getInstance().getDownloadStatus(job_id);
            job.status = "cancelled";
            
            nlohmann::json response;
            response["success"] = true;
            response["status"] = job.status;
            
            auto resp = drogon::HttpResponse::newHttpJsonResponse(response.dump());
            callback(resp);
        } catch (const std::exception &e) {
            // Error handling
            auto error_resp = drogon::HttpResponse::newHttpJsonResponse(
                nlohmann::json{{"error", true}, {"message", e.what()}}.dump()
            );
            error_resp->setStatusCode(drogon::k500InternalServerError);
            callback(error_resp);
        }
    },
    {drogon::Post});

    // GET /api/settings
    app.registerHandler(
        "/api/settings",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback)
        {
            try
            {
                const auto &settings = ConfigManager::getInstance().getSettings();

                nlohmann::json response;
                response["gitlab_url"] = settings.gitlab_url;
                response["project_id"] = settings.project_id;
                response["artifact_paths"] = settings.artifact_paths;
                response["download_path"] = settings.download_path;

                auto resp = drogon::HttpResponse::newHttpJsonResponse(response.dump());
                callback(resp);
            }
            catch (const std::exception &e)
            {
                nlohmann::json error;
                error["error"] = true;
                error["message"] = std::string("Failed to get settings: ") + e.what();

                auto error_resp = drogon::HttpResponse::newHttpJsonResponse(error.dump());
                error_resp->setStatusCode(drogon::k500InternalServerError);
                callback(error_resp);
            }
        },
        {drogon::Get});

    app.registerHandler("/", [](const drogon::HttpRequestPtr &req,
                                std::function<void(const drogon::HttpResponsePtr &)> &&callback)
                        {
    auto resp = drogon::HttpResponse::newFileResponse("index.html");
    callback(resp); });

    // POST /api/settings
    app.registerHandler(
        "/api/settings",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback)
        {
            try
            {
                auto json = nlohmann::json::parse(std::string(req->getBody()));

                ConfigManager::Settings settings = ConfigManager::getInstance().getSettings();

                if (json.contains("gitlab_url"))
                {
                    settings.gitlab_url = json["gitlab_url"].get<std::string>();
                }
                if (json.contains("project_id"))
                {
                    settings.project_id = json["project_id"].get<std::string>();
                }
                if (json.contains("api_token"))
                {
                    settings.api_token = json["api_token"].get<std::string>();
                }
                if (json.contains("download_path"))
                {
                    settings.download_path = json["download_path"].get<std::string>();
                }
                if (json.contains("artifact_paths"))
                {
                    settings.artifact_paths = json["artifact_paths"].get<std::vector<std::string>>();
                }

                if (ConfigManager::getInstance().saveSettings(settings))
                {
                    nlohmann::json response;
                    response["success"] = true;
                    response["message"] = "Settings updated successfully";

                    auto resp = drogon::HttpResponse::newHttpJsonResponse(response.dump());
                    callback(resp);
                }
                else
                {
                    nlohmann::json error;
                    error["error"] = true;
                    error["message"] = "Failed to save settings";

                    auto error_resp = drogon::HttpResponse::newHttpJsonResponse(error.dump());
                    error_resp->setStatusCode(drogon::k500InternalServerError);
                    callback(error_resp);
                }
            }
            catch (const std::exception &e)
            {
                nlohmann::json error;
                error["error"] = true;
                error["message"] = std::string("Failed to update settings: ") + e.what();

                auto error_resp = drogon::HttpResponse::newHttpJsonResponse(error.dump());
                error_resp->setStatusCode(drogon::k500InternalServerError);
                callback(error_resp);
            }
        },
        {drogon::Post});
}

int main() {
    try {
        // โหลด config ก่อนเริ่มเซิร์ฟเวอร์
        if (!ConfigManager::getInstance().loadSettings()) {
            std::cerr << "Failed to load configuration. Please check config.json" << std::endl;
            return 1;
        }

        auto& app = drogon::app();

        // ตั้งค่า CORS ให้ครอบคลุมมากขึ้น
        app.registerPostHandlingAdvice(
            [](const drogon::HttpRequestPtr&,
               const drogon::HttpResponsePtr& resp) {
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
                resp->addHeader("Access-Control-Max-Age", "3600");
            });

        // ตั้งค่า document root ให้ถูกต้อง
        app.setDocumentRoot("public");
        app.setUploadPath("uploads");

        setupControllers(app);
        app.addListener("0.0.0.0", 3000);
        app.setThreadNum(16);

        std::cout << "Server starting on port 3000..." << std::endl;
        app.run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}