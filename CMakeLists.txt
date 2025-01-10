cmake_minimum_required(VERSION 3.20...3.31)
project(gitlab_artifact_downloader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)

# กำหนดตำแหน่งไฟล์ซอร์สโค้ด
set(SOURCES
    src/main.cpp
    src/GitLabService.cpp
    src/FileManager.cpp
    src/ConfigManager.cpp
)

# สร้าง executable
add_executable(${PROJECT_NAME} ${SOURCES})

# เพิ่ม include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Link กับ libraries
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)

# คัดลอกไฟล์และโฟลเดอร์ที่จำเป็น
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/public
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/public
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config
)