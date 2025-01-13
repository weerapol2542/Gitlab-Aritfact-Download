##############################################################################################################################################################################################################################################
This project is still incomplete and has several aspects that need improvement. I welcome your feedback and will do my best to refine it. I apologize if you encounter any errors or unclear details. To run this project, you can clone it, open the project, launch PowerShell, and execute the following command:
.\build\debug\gitlab_artifact_downloader.exe  
No additional build process is required. If you encounter any issues, try restarting the server and saving the settings again.
For any other errors or concerns, please feel free to report them, and I will address them promptly.
##############################################################################################################################################################################################################################################
GitLab Artifact Downloader
An application designed to manage and download artifacts from GitLab pipelines with a modern web-based user interface and efficient download management capabilities.
Overview
GitLab Artifact Downloader streamlines the process of downloading artifacts from pipelines. With its user-friendly interface and advanced features for tracking, managing, and organizing downloads, this tool is ideal for development teams working with large artifacts or needing selective downloads from CI/CD pipelines.
Key Features
This application offers comprehensive artifact management capabilities:
Selective artifact downloads with pattern matching support
Real-time download progress tracking with status details
Pipeline filtering and search functionality
Configurable download locations and management settings
Pause, resume, and cancel downloads
Download history tracking and management
Concurrent file downloads
Automatic retry mechanism for failed downloads
Efficient handling of large artifacts
Supported Browsers
Google Chrome (recommended)
Mozilla Firefox
Microsoft Edge
Installation
Setting Up the Development Environment
Clone the project into your desired directory:
git clone https://github.com/weerapol2542/Gitlab-Aritfact-Download.git
Open the project using Visual Studio Code or your preferred IDE.
Open PowerShell or Terminal in the project folder and run the following command to start the application:
.\build\debug\gitlab_artifact_downloader.exe
Example:
C:\part_you\gitlab-artifact-downloader> .\build\debug\gitlab_artifact_downloader.exe
Successful output:
Loading config from: C:\Users\weerapol\Desktop\test-download\gitlab-artifact-downloader\build\debug\config\config.json
Settings loaded successfully
Server starting on port 3000...
Access the web user interface at:
http://localhost:3000/index.html
GitLab Configuration
Access the settings panel by clicking the gear icon and configure the following parameters:
GitLab URL: The URL of your GitLab instance
API Token: Your GitLab Personal Access Token
Project ID: The ID of your GitLab project
Download Path: Local directory to store the artifacts
Artifact Patterns: File patterns for selective downloads
Test the connection using the "Test Connection" button, and save the configuration.
User Guide
Downloading Artifacts
Selecting Pipelines
Browse available pipelines in the main user interface.
Use search and filter options to locate specific pipelines.
View pipeline details, including status, branch, and creation date.
Managing Downloads
Select the artifacts you want to download.
Track download progress in real-time.
View detailed transfer statistics.
Manage ongoing downloads with pause/resume functionality.
History and Management
Access the download history in the designated panel.
View successful and failed downloads.
Manage downloaded artifacts.
Clear history as needed.
Troubleshooting
Common Issues
Connection Problems
Verify the GitLab URL format and accessibility.
Ensure the API token has sufficient permissions.
Check network connectivity and firewall settings.
Download Problems
Confirm the artifact path exists in the pipeline.
Check for sufficient disk space.
Verify write permissions in the download directory.
Future Development
Integration with cloud storage systems
Advanced artifact filtering
Batch download scheduling
Custom notification system
Artifact compression options
Security Considerations
Secure API token storage using encryption
Enforce HTTPS communication
Restrict access to local file systems
Manage sessions for the web user interface
