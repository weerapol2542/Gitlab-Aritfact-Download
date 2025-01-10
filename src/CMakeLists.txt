add_executable(gitlab_artifact_downloader
    main.cpp
    GitLabService.cpp
    FileManager.cpp
    ConfigManager.cpp
)

target_link_libraries(gitlab_artifact_downloader PRIVATE
    Drogon::Drogon
)