const API_BASE_URL = "http://localhost:3000/api";
let currentPage = 1;
const PER_PAGE = 10;
let searchDebounceTimer;


let isServerHealthy = true;
let serverCheckInterval;

async function checkServerHealth() {
    try {
        const response = await fetch(`${API_BASE_URL}/settings`, {
            method: 'GET',
            headers: {
                'Cache-Control': 'no-cache'
            }
        });
        
        if (!response.ok) {
            throw new Error('Server is not responding');
        }
        
        if (!isServerHealthy) {
            isServerHealthy = true;
            showToast('Server connection restored', 'success');
        }
    } catch (error) {
        if (isServerHealthy) {
            isServerHealthy = false;
            showToast('Lost connection to server. Please check if the server is running.', 'error');
        }
    }
}

// Start server health monitoring
function startServerHealthCheck() {
    checkServerHealth(); // Initial check
    serverCheckInterval = setInterval(checkServerHealth, 5000); // Check every 5 seconds
}

// Stop server health monitoring
function stopServerHealthCheck() {
    if (serverCheckInterval) {
        clearInterval(serverCheckInterval);
    }
}
// Toast Notifications
function showToast(message, type = "success") {
  const toast = document.createElement("div");
  toast.className = `toast toast-${type}`;
  toast.innerHTML = `
        <div class="toast-content">
            <div class="toast-message">${message}</div>
            <div class="toast-progress"></div>
        </div>
    `;

  const container = document.getElementById("toast-container");
  container.appendChild(toast);

  setTimeout(() => toast.classList.add("show"), 10);

  setTimeout(() => {
    toast.classList.remove("show");
    setTimeout(() => toast.remove(), 300);
  }, 3000);
}

// Utility Functions
function formatDate(dateString) {
  const date = new Date(dateString);
  const options = {
    year: "numeric",
    month: "long",
    day: "numeric",
    hour: "2-digit",
    minute: "2-digit",
    hour12: true,
  };
  return date.toLocaleString("en-US", options);
}

function formatSize(bytes) {
  if (!bytes) return "0 KB";
  const sizes = ["B", "KB", "MB", "GB"];
  const i = Math.floor(Math.log(bytes) / Math.log(1024));
  return `${(bytes / Math.pow(1024, i)).toFixed(2)} ${sizes[i]}`;
}

function formatSpeed(bytesPerSecond) {
  if (bytesPerSecond === 0) return "--";
  const units = ["B/s", "KB/s", "MB/s", "GB/s"];
  let value = bytesPerSecond;
  let unitIndex = 0;
  while (value >= 1024 && unitIndex < units.length - 1) {
    value /= 1024;
    unitIndex++;
  }
  return `${value.toFixed(2)} ${units[unitIndex]}`;
}

function createStatusTag(status) {
  const statusIcons = {
    success: "✓",
    completed: "✓",
    failed: "✕",
    running: "⟳",
    pending: "⋯",
  };
  const icon = statusIcons[status.toLowerCase()] || "?";
  return `
        <span class="status-tag status-${status.toLowerCase()}">
            <span class="status-icon">${icon}</span>
            ${status.charAt(0).toUpperCase() + status.slice(1)}
        </span>
    `;
}

// Settings Management
async function fetchSettings() {
  try {
    const response = await fetch(`${API_BASE_URL}/settings`);
    if (!response.ok) throw new Error("Failed to fetch settings");

    const settings = await response.json();
    updateSettingsForm(settings);
    return settings;
  } catch (error) {
    showToast("Failed to load settings", "error");
    console.error("Error:", error);
  }
}

function updateSettingsForm(settings) {
  const fields = ["gitlab-url", "api-token", "project-id", "download-path"];
  fields.forEach((id) => {
    const element = document.getElementById(id);
    if (element) {
      const key = id.replace("-", "_");
      element.value = settings[key] || localStorage.getItem(key) || "";
    }
  });

  const artifactPaths =
    settings.artifact_paths ||
    JSON.parse(localStorage.getItem("artifact_paths") || "[]");

  const textarea = document.querySelector(".artifact-pattern");
  textarea.value = artifactPaths.join("\n");
  textarea.style.height = "auto";
  textarea.style.height = textarea.scrollHeight + "px";
}

async function saveSettings(event) {
  if (event && event.preventDefault) {
    event.preventDefault();
  }

  try {
    const artifactPathsText = document.querySelector(".artifact-pattern").value;
    const artifactPaths = artifactPathsText
      .split("\n")
      .map((path) => path.trim())
      .filter((path) => path !== "");

    const downloadPath = document.getElementById("download-path").value.trim();
    if (!downloadPath) {
      throw new Error("Please select a download folder");
    }

    const settings = {
      gitlab_url: document.getElementById("gitlab-url").value.trim(),
      api_token: document.getElementById("api-token").value.trim(),
      project_id: document.getElementById("project-id").value.trim(),
      download_path: downloadPath,
      artifact_paths: artifactPaths,
    };

    const response = await fetch(`${API_BASE_URL}/settings`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "Cache-Control": "no-cache",
      },
      body: JSON.stringify(settings),
    });

    if (!response.ok) {
      const errorMessage = await response.text();
      throw new Error(`Failed to save settings: ${errorMessage}`);
    }

    Object.entries(settings).forEach(([key, value]) => {
      if (Array.isArray(value)) {
        localStorage.setItem(key, JSON.stringify(value));
      } else {
        localStorage.setItem(key, value);
      }
    });

    showToast("Settings saved successfully", "success");
    await loadPipelines();
  } catch (error) {
    console.error("Save settings error:", error);
    showToast("Failed to save settings: " + error.message, "error");
  }
}

// Pipeline Management
async function loadPipelines(showLoading = true) {
  const tableBody = document.getElementById("pipelines-table-body");
  if (showLoading) {
    tableBody.innerHTML =
      '<tr><td colspan="5" class="loading"><div class="spinner"></div>Loading pipelines...</td></tr>';
  }

  const searchTerm = document.getElementById("pipeline-search").value;
  const statusFilter = document.getElementById("filter-status").value;
  const gitlabUrl = localStorage.getItem("gitlab_url");
  const apiToken = localStorage.getItem("api_token");
  const projectId = localStorage.getItem("project_id");

  if (!gitlabUrl || !apiToken || !projectId) {
    showToast("Please configure GitLab settings first", "warning");
    return;
  }

  try {
    const queryParams = new URLSearchParams({
      per_page: PER_PAGE,
      page: currentPage,
      ...(searchTerm && { search: searchTerm }),
      ...(statusFilter && { status: statusFilter }),
    });

    const response = await fetch(
      `${gitlabUrl}/api/v4/projects/${projectId}/pipelines?${queryParams}`,
      {
        headers: { "PRIVATE-TOKEN": apiToken },
      }
    );

    if (!response.ok) throw new Error("Failed to fetch pipelines");

    const data = await response.json();
    renderPipelines(data, tableBody);
    updatePagination(response.headers);
  } catch (error) {
    tableBody.innerHTML =
      '<tr><td colspan="5" class="error">Failed to load pipelines</td></tr>';
    showToast("Failed to load pipelines", "error");
    console.error("Error:", error);
  }
}

function renderPipelines(pipelines, tableBody) {
  if (!pipelines.length) {
    tableBody.innerHTML =
      '<tr><td colspan="5" class="no-data">No pipelines found</td></tr>';
    return;
  }

  tableBody.innerHTML = pipelines
    .map(
      (pipeline) => `
        <tr class="pipeline-row" data-id="${pipeline.id}">
            <td class="pipeline-id">#${pipeline.id}</td>
            <td>${createStatusTag(pipeline.status)}</td>
            <td class="pipeline-branch">${pipeline.ref}</td>
            <td>${formatDate(pipeline.created_at)}</td>
            <td>
                 <button onclick="downloadArtifacts(${pipeline.id})" 
        class="btn primary download-btn" 
        ${pipeline.status !== "success" ? "disabled" : ""}>
    <span class="material-icons">download</span> Download
</button>
            </td>
        </tr>
    `
    )
    .join("");
}

// Search and Pagination
function setupSearch() {
  const searchInput = document.getElementById("pipeline-search");
  const statusFilter = document.getElementById("filter-status");

  searchInput.addEventListener("input", async (e) => {
    clearTimeout(searchDebounceTimer);
    const searchValue = e.target.value.trim();

    if (/^\d+$/.test(searchValue)) {
      try {
        const gitlabUrl = localStorage.getItem("gitlab_url");
        const apiToken = localStorage.getItem("api_token");
        const projectId = localStorage.getItem("project_id");

        const response = await fetch(
          `${gitlabUrl}/api/v4/projects/${projectId}/pipelines/${searchValue}`,
          {
            headers: { "PRIVATE-TOKEN": apiToken },
          }
        );

        if (response.ok) {
          const pipeline = await response.json();
          const tableBody = document.getElementById("pipelines-table-body");
          tableBody.innerHTML = `
                        <tr class="pipeline-row" data-id="${pipeline.id}">
                            <td class="pipeline-id">#${pipeline.id}</td>
                            <td>${createStatusTag(pipeline.status)}</td>
                            <td class="pipeline-branch">${pipeline.ref}</td>
                            <td>${formatDate(pipeline.created_at)}</td>
                            <td>
                                <button onclick="downloadArtifacts(${
                                  pipeline.id
                                })" 
        class="btn primary download-btn" 
        ${pipeline.status !== "success" ? "disabled" : ""}>
    <span class="material-icons">download</span> Download
</button>

                            </td>
                        </tr>
                    `;
        } else {
          showToast(`Pipeline #${searchValue} not found`, "warning");
        }
      } catch (error) {
        console.error("Error:", error);
        showToast("An error occurred while searching", "error");
      }
    } else {
      searchDebounceTimer = setTimeout(() => {
        currentPage = 1;
        loadPipelines(false);
      }, 300);
    }
  });

  statusFilter.addEventListener("change", () => {
    currentPage = 1;
    loadPipelines();
  });
}

function updatePagination(headers) {
  const totalPages = parseInt(headers.get("X-Total-Pages") || "1");
  const totalItems = parseInt(headers.get("X-Total") || "0");

  document.getElementById(
    "page-info"
  ).textContent = `Page ${currentPage} of ${totalPages} (${totalItems} pipelines)`;

  const prevBtn = document.getElementById("prev-page");
  const nextBtn = document.getElementById("next-page");

  prevBtn.disabled = currentPage <= 1;
  nextBtn.disabled = currentPage >= totalPages;

  document.querySelector(".pipelines-list").classList.add("fade");
  setTimeout(() => {
    document.querySelector(".pipelines-list").classList.remove("fade");
  }, 300);
}

// Download Management
async function downloadArtifacts(pipelineId) {
  const button = document.querySelector(
    `[data-id="${pipelineId}"] .download-btn`
  );
  button.disabled = true;
  button.innerHTML = '<span class="material-icons">download</span> Starting...';

  try {
    const downloadPath = localStorage.getItem("download_path");
    if (!downloadPath) {
      throw new Error("Download path not set. Please configure in settings.");
    }

    const artifactPathsText = document.querySelector(".artifact-pattern").value;
    const paths = artifactPathsText
      .split("\n")
      .map((path) => path.trim())
      .filter((path) => path !== "");

    if (paths.length === 0) {
      throw new Error("Please specify at least one artifact path");
    }

    const response = await fetch(`${API_BASE_URL}/artifacts/download`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        pipeline_id: pipelineId,
        artifacts: paths,
        download_path: downloadPath,
      }),
    });

    if (!response.ok) {
      throw new Error("Failed to start download");
    }

    let result = await response.json();

    if (typeof result === "string") {
      result = JSON.parse(result);
    }

    const jobId =
      result.job_id || result?.data?.job_id || result?.status?.job_id;

    if (!jobId || typeof jobId !== "string" || jobId.trim() === "") {
      throw new Error("Invalid response from API");
    }

    // Add support for partial download scenarios
    if (result.status === "partial_failure") {
      showToast(result.message, "warning");
    } else {
      showToast("Download started successfully", "success");
    }

    monitorDownload(jobId, pipelineId, paths);
  } catch (error) {
    console.error("Download error:", error);
    showToast(error.message, "error");
  } finally {
    button.disabled = false;
    button.innerHTML = '<span class="material-icons">download</span> Download';
  }
}

function handleDownloadCompletion(downloadDiv, status, artifactPaths, pipelineId, jobId) {
  if (status.status === "completed") {
      showToast("Download completed successfully", "success");
      setTimeout(() => {
          downloadDiv.classList.add("fade-out");
          setTimeout(() => downloadDiv.remove(), 500);
      }, 3000);

      addToDownloadHistory({
          pipelineId: pipelineId,
          jobId: jobId,
          artifacts: artifactPaths,
          status: "completed",
          completedAt: new Date().toISOString(),
          size: status.total_bytes,
      });
  } else if (status.status === "failed") {
      showToast(
          "Download failed: " + (status.message || "Unknown error"),
          "error"
      );
      downloadDiv.classList.add("failed");

      addToDownloadHistory({
          pipelineId: pipelineId,
          jobId: jobId,
          artifacts: artifactPaths,
          status: "failed",
          completedAt: new Date().toISOString(),
          message: status.message,
      });
  }
}

async function monitorDownload(jobId, pipelineId, artifactPaths) {
  const statusContainer = document.getElementById("active-downloads");
  const downloadDiv = document.createElement("div");
  downloadDiv.className = "download-status";
  downloadDiv.id = `download-${jobId}`;

  // Initial download status UI setup
  downloadDiv.innerHTML = `
      <div class="download-header">
          <div class="download-info">
              <span class="download-id">Pipeline #${pipelineId}</span>
              <span class="download-state">Starting...</span>
          </div>
          <div class="download-controls">
              <button class="btn secondary pause-btn" title="Pause Download">
                  <i class="fas fa-pause"></i>
              </button>
              <button class="btn secondary resume-btn" title="Resume Download" style="display: none;">
                  <i class="fas fa-play"></i>
              </button>
              <button class="btn secondary cancel-btn" title="Cancel Download">
                  <i class="fas fa-times"></i>
              </button>
          </div>
      </div>
      <div class="download-body">
          <div class="current-file">Initializing...</div>
          <div class="progress-container">
              <div class="progress-bar" style="width: 0%"></div>
              <div class="progress-text">0%</div>
          </div>
          <div class="download-details">
              <div class="download-info-left">
                  <span class="download-speed">Speed: --</span>
                  <span class="time-remaining">Time remaining: Calculating...</span>
              </div>
              <span class="download-size">0 KB / Unknown</span>
          </div>
      </div>
      <div class="error-message" style="display: none;">
          <i class="fas fa-exclamation-circle"></i>
          <div class="error-content"></div>
          <button class="btn secondary retry-btn" style="display: none;">
              <i class="fas fa-sync"></i> Retry
          </button>
      </div>
  `;

  statusContainer.insertBefore(downloadDiv, statusContainer.firstChild);

  let retryCount = 0;
  const MAX_RETRIES = 3;
  let lastUpdate = Date.now();
  let lastBytes = 0;
  let consecutiveErrors = 0;
  let isRetrying = false;

  const updateInterval = setInterval(async () => {
      if (!isServerHealthy) {
          showDownloadError(downloadDiv, "Server connection lost. Waiting for connection to resume...");
          return;
      }

      try {
          const response = await fetch(`${API_BASE_URL}/artifacts/status/${jobId}`);
          
          if (!response.ok) {
              throw new Error(`HTTP error! status: ${response.status}`);
          }

          const rawText = await response.text();
          let parsedData;

          try {
              if (rawText.startsWith('"') && rawText.endsWith('"')) {
                  const unescapedText = JSON.parse(rawText);
                  parsedData = JSON.parse(unescapedText);
              } else {
                  parsedData = JSON.parse(rawText);
              }
          } catch (e) {
              console.error("JSON parse error:", e);
              throw new Error("Invalid server response format");
          }

          // Reset error counters on successful response
          consecutiveErrors = 0;
          isRetrying = false;
          hideDownloadError(downloadDiv);

          // Check for stalled download
          const currentTime = Date.now();
          const timeSinceLastUpdate = (currentTime - lastUpdate) / 1000;
          
          if (parsedData.status === "in_progress" && timeSinceLastUpdate > 30 && 
              parsedData.bytes_downloaded === lastBytes) {
              showDownloadWarning(downloadDiv, "Download appears to be stalled. Checking connection...");
          }

          updateDownloadUI(downloadDiv, parsedData, lastBytes, lastUpdate);

          lastUpdate = currentTime;
          lastBytes = parsedData.bytes_downloaded || 0;

          if (["completed", "failed", "cancelled"].includes(parsedData.status)) {
              clearInterval(updateInterval);
              handleDownloadCompletion(downloadDiv, parsedData, artifactPaths, pipelineId, jobId);
          }

      } catch (error) {
          console.error("Status update error:", error);
          consecutiveErrors++;

          if (consecutiveErrors >= 3) {
              const errorMessage = `Download monitoring error: ${error.message}`;
              showDownloadError(downloadDiv, errorMessage, true);
              
              if (!isRetrying && retryCount < MAX_RETRIES) {
                  retryCount++;
                  isRetrying = true;
                  
                  showDownloadWarning(
                      downloadDiv, 
                      `Connection attempt ${retryCount}/${MAX_RETRIES}. Retrying in 5 seconds...`
                  );
                  
                  setTimeout(() => {
                      isRetrying = false;
                  }, 5000);
              } else if (retryCount >= MAX_RETRIES) {
                  clearInterval(updateInterval);
                  showDownloadError(
                      downloadDiv,
                      `Download failed after ${MAX_RETRIES} retry attempts. Please try again later.`,
                      false
                  );
              }
          }
      }
  }, 1000);

  setupDownloadControls(downloadDiv, jobId, updateInterval);
}

function createDownloadSummaryModal(summary) {
  const modal = document.createElement('div');
  modal.className = 'success-modal';
  
  // Add data attribute to track modal state
  modal.dataset.modalId = `download-summary-${Date.now()}`;

  const modalContent = `
      <div class="modal-content">
          <div class="modal-header">
              <h3>
                  <i class="fas ${getStatusIcon(summary.status)} ${getStatusClass(summary.status)}"></i>
                  ${getStatusTitle(summary.status)}
              </h3>
              <button class="modal-close" onclick="closeDownloadSummary(this)" aria-label="Close summary">&times;</button>
          </div>
          <div class="modal-body">
              ${summary.downloadedFiles.length > 0 ? `
                  <div class="file-section">
                      <h4><i class="fas fa-file"></i> Successfully Downloaded Files</h4>
                      <div class="file-list">
                          ${summary.downloadedFiles.map(file => `
                              <div class="file-item">
                                  <div class="file-status-icon">
                                      <i class="fas fa-check-circle text-success"></i>
                                  </div>
                                  <div class="file-details">
                                      <div class="file-info">
                                          <span class="file-name">${file.name}</span>
                                          <span class="file-status-tag status-success">Success</span>
                                      </div>
                                      <span class="file-size">${formatSize(file.size)}</span>
                                  </div>
                              </div>
                          `).join('')}
                      </div>
                  </div>
              ` : ''}

              ${summary.missingFiles.length > 0 ? `
                  <div class="file-section">
                      <h4><i class="fas fa-exclamation-triangle text-danger"></i> Failed Downloads</h4>
                      <div class="file-list">
                          ${summary.missingFiles.map(file => `
                              <div class="file-item">
                                  <div class="file-status-icon">
                                      <i class="fas fa-times-circle text-danger"></i>
                                  </div>
                                  <div class="file-details">
                                      <div class="file-info">
                                          <span class="file-name">${file}</span>
                                          <span class="file-status-tag status-not-found">Not Found</span>
                                      </div>
                                  </div>
                              </div>
                          `).join('')}
                      </div>
                  </div>
              ` : ''}

              <div class="summary-footer">
                  <div class="stats">
                      <div class="stat-item">
                          <span class="stat-label">Total Files Attempted:</span>
                          <span class="stat-value">${summary.totalFiles}</span>
                      </div>
                      <div class="stat-item">
                          <span class="stat-label">Successfully Downloaded:</span>
                          <span class="stat-value text-success">${summary.downloadedFiles.length}</span>
                      </div>
                      <div class="stat-item">
                          <span class="stat-label">Failed Downloads:</span>
                          <span class="stat-value text-danger">${summary.missingFiles.length}</span>
                      </div>
                      <div class="stat-item">
                          <span class="stat-label">Total Download Size:</span>
                          <span class="stat-value">${formatSize(summary.totalSize)}</span>
                      </div>
                  </div>
              </div>
          </div>
          <div class="modal-footer">
              <button class="btn primary" onclick="closeDownloadSummary(this)">Close</button>
          </div>
      </div>
  `;

  modal.innerHTML = modalContent;
  return modal;
}

function showDownloadError(downloadDiv, message, showRetry = true) {
  const errorElement = downloadDiv.querySelector(".error-message");
  const errorContent = downloadDiv.querySelector(".error-content");
  const retryButton = downloadDiv.querySelector(".retry-btn");
  
  errorContent.textContent = message;
  errorElement.style.display = "flex";
  errorElement.className = "error-message";
  
  if (showRetry) {
      retryButton.style.display = "inline-flex";
      retryButton.onclick = () => retryDownload(downloadDiv);
  } else {
      retryButton.style.display = "none";
  }
}

function showDownloadWarning(downloadDiv, message) {
  const errorElement = downloadDiv.querySelector(".error-message");
  const errorContent = downloadDiv.querySelector(".error-content");
  
  errorContent.textContent = message;
  errorElement.style.display = "flex";
  errorElement.className = "error-message warning";
}

function hideDownloadError(downloadDiv) {
  const errorElement = downloadDiv.querySelector(".error-message");
  errorElement.style.display = "none";
}

function updateDownloadUI(downloadDiv, status, lastBytes, lastUpdate) {
  if (!downloadDiv) return;

  const elements = {
      state: downloadDiv.querySelector(".download-state"),
      currentFile: downloadDiv.querySelector(".current-file"),
      progressBar: downloadDiv.querySelector(".progress-bar"),
      progressText: downloadDiv.querySelector(".progress-text"),
      speed: downloadDiv.querySelector(".download-speed"),
      size: downloadDiv.querySelector(".download-size"),
      timeRemaining: downloadDiv.querySelector(".time-remaining")
  };

  // Handle completion states
  if (status.status === "completed" || status.status === "partial_success" || status.status === "failed") {
      let downloadSummary;
      try {
          const messageLines = status.message.split('\n');
          const jsonStr = messageLines[messageLines.length - 1];
          const downloadInfo = JSON.parse(jsonStr);
          
          downloadSummary = {
              totalFiles: downloadInfo.total_files || 0,
              downloadedFiles: downloadInfo.downloaded_files || [],
              missingFiles: downloadInfo.missing_files || [],
              totalSize: downloadInfo.total_size || 0,
              status: status.status
          };
      } catch (error) {
          console.warn('Failed to parse download info:', error);
          downloadSummary = {
              totalFiles: 0,
              downloadedFiles: [],
              missingFiles: status.message ? [status.message] : ["Unknown error"],
              totalSize: 0,
              status: "failed"
          };
      }

      const modal = createDownloadSummaryModal(downloadSummary);
      document.body.appendChild(modal);

      elements.state.textContent = getStatusText(status.status);
      elements.progressBar.style.width = "100%";
      elements.progressText.textContent = "100%";
      
      return;
  }

  // Update ongoing download progress
  if (status.current_file) {
      elements.currentFile.textContent = `Downloading: ${status.current_file}`;
  }

  if (status.bytes_downloaded !== undefined && status.total_bytes) {
      const progress = (status.bytes_downloaded / status.total_bytes) * 100;
      elements.progressBar.style.width = `${progress}%`;
      elements.progressText.textContent = `${progress.toFixed(1)}%`;
      elements.size.textContent = `${formatSize(status.bytes_downloaded)} / ${formatSize(status.total_bytes)}`;

      // Calculate and display speed and time remaining
      if (lastUpdate && lastBytes !== undefined) {
          const timeDiff = (Date.now() - lastUpdate) / 1000;
          const bytesDiff = status.bytes_downloaded - lastBytes;
          const speed = bytesDiff / timeDiff;
          elements.speed.textContent = `Speed: ${formatSpeed(speed)}`;

          // Calculate time remaining
          if (speed > 0) {
              const remainingBytes = status.total_bytes - status.bytes_downloaded;
              const timeRemaining = remainingBytes / speed;
              elements.timeRemaining.textContent = `Time remaining: ${formatTimeRemaining(timeRemaining)}`;
          } else {
              elements.timeRemaining.textContent = 'Time remaining: Calculating...';
          }
      }
  }
}


function getStatusText(status) {
  const statusMap = {
      'completed': 'Download Complete',
      'partial_success': 'Partially Complete',
      'failed': 'Download Failed'
  };
  return statusMap[status] || status;
}
function closeDownloadSummary(button) {
  const modal = button.closest('.success-modal');
  if (modal) {
      // Add fade-out class for animation
      modal.classList.add('fade-out');
      
      // Remove modal after animation completes
      setTimeout(() => {
          try {
              if (modal.parentNode) {
                  modal.parentNode.removeChild(modal);
              }
          } catch (error) {
              console.error('Error removing modal:', error);
          }
      }, 300);
  }
}

function getStatusIcon(status) {
  const icons = {
      'completed': 'fa-check-circle',
      'partial_success': 'fa-exclamation-circle',
      'failed': 'fa-times-circle'
  };
  return icons[status] || 'fa-info-circle';
}

function getStatusClass(status) {
  const classes = {
      'completed': 'text-success',
      'partial_success': 'text-warning',
      'failed': 'text-danger'
  };
  return classes[status] || 'text-info';
}

function getStatusTitle(status) {
  const titles = {
      'completed': 'Download Complete',
      'partial_success': 'Partial Download Complete',
      'failed': 'Download Failed'
  };
  return titles[status] || 'Download Status';
}


async function pauseDownload(jobId) {
  try {
    const response = await fetch(`${API_BASE_URL}/artifacts/pause/${jobId}`, {
      method: "POST",
    });
    if (!response.ok) {
      throw new Error("Failed to pause download");
    }
    showToast("Download paused", "info");
  } catch (error) {
    console.error("Pause error:", error);
    showToast(error.message, "error");
  }
}

async function resumeDownload(jobId) {
  try {
    const response = await fetch(`${API_BASE_URL}/artifacts/resume/${jobId}`, {
      method: "POST",
    });
    if (!response.ok) {
      throw new Error("Failed to resume download");
    }
    showToast("Download resumed", "info");
  } catch (error) {
    console.error("Resume error:", error);
    showToast(error.message, "error");
  }
}

async function cancelDownload(jobId) {
  try {
    const response = await fetch(`${API_BASE_URL}/artifacts/cancel/${jobId}`, {
      method: "POST",
    });
    if (!response.ok) {
      throw new Error("Failed to cancel download");
    }
    showToast("Download cancelled", "info");
  } catch (error) {
    console.error("Cancel error:", error);
    showToast(error.message, "error");
  }
}

function setupDownloadControls(downloadDiv, jobId, updateInterval) {
  const pauseBtn = downloadDiv.querySelector(".pause-btn");
  const resumeBtn = downloadDiv.querySelector(".resume-btn");
  const cancelBtn = downloadDiv.querySelector(".cancel-btn");
  const retryBtn = downloadDiv.querySelector(".retry-btn");

  if (pauseBtn) {
      pauseBtn.addEventListener("click", async () => {
          try {
              await pauseDownload(jobId);
              pauseBtn.style.display = "none";
              resumeBtn.style.display = "inline-block";
          } catch (error) {
              showToast(`Failed to pause download: ${error.message}`, "error");
          }
      });
  }

  if (resumeBtn) {
      resumeBtn.addEventListener("click", async () => {
          try {
              await resumeDownload(jobId);
              resumeBtn.style.display = "none";
              pauseBtn.style.display = "inline-block";
          } catch (error) {
              showToast(`Failed to resume download: ${error.message}`, "error");
          }
      });
  }

  if (cancelBtn) {
      cancelBtn.addEventListener("click", async () => {
          if (confirm("Are you sure you want to cancel this download?")) {
              try {
                  await cancelDownload(jobId);
                  clearInterval(updateInterval);
                  downloadDiv.remove();
              } catch (error) {
                  showToast(`Failed to cancel download: ${error.message}`, "error");
              }
          }
      });
  }

  if (retryBtn) {
      retryBtn.addEventListener("click", () => retryDownload(downloadDiv));
  }
}

// Test Connection
async function testConnection() {
  const gitlabUrl = document.getElementById("gitlab-url").value;
  const apiToken = document.getElementById("api-token").value;
  const projectId = document.getElementById("project-id").value;

  if (!gitlabUrl || !apiToken || !projectId) {
    showToast("Please fill in all the required fields.", "warning");
    return;
  }

  try {
    const response = await fetch(`${gitlabUrl}/api/v4/projects/${projectId}`, {
      headers: { "PRIVATE-TOKEN": apiToken },
    });

    if (response.ok) {
      const project = await response.json();
      showToast(`Connection successful: ${project.name}`, "success");
    } else {
      const error = await response.json();
      showToast(`Connection failed: ${error.message}`, "error");
    }
  } catch (error) {
    showToast("Unable to connect to GitLab.", "error");
    console.error("Error:", error);
  }
}

// Download History Management
function addToDownloadHistory(download) {
    try {
        const history = getDownloadHistory();
        
        // แยกเฉพาะไฟล์ที่ดาวน์โหลดสำเร็จ
        const successfulFiles = download.artifacts.filter(file => !download.message || !download.message.includes(file));
        
        const historyEntry = {
            pipelineId: download.pipelineId,
            artifacts: successfulFiles, // เก็บเฉพาะไฟล์ที่ดาวน์โหลดสำเร็จ
            status: download.status,
            downloadedAt: new Date().toISOString(),
            size: download.size || 0,
        };

        history.unshift(historyEntry);
        const trimmedHistory = history.slice(0, 100);
        localStorage.setItem("download_history", JSON.stringify(trimmedHistory));
        updateHistoryUI();
    } catch (error) {
        console.error("Error saving download history:", error);
    }
}

function getDownloadHistory() {
  try {
    const history = localStorage.getItem("download_history");
    return history ? JSON.parse(history) : [];
  } catch (error) {
    console.error("Error loading download history:", error);
    return [];
  }
}

function updateHistoryUI() {
  const historyTable = document.getElementById("history-table-body");
  if (!historyTable) return;

  const history = getDownloadHistory();

  if (history.length === 0) {
    historyTable.innerHTML = `
            <tr>
                <td colspan="5" class="text-center">No download history</td>
            </tr>
        `;
    return;
  }

  historyTable.innerHTML = history
    .map((item) => {
      const artifacts = Array.isArray(item.artifacts)
        ? item.artifacts.join(", ")
        : item.artifacts || "N/A";

      return `
            <tr>
                <td>${item.pipelineId || "N/A"}</td>
                <td>${artifacts}</td>
                <td>${createStatusTag(item.status || "unknown")}</td>
                <td>${formatDate(item.downloadedAt)}</td>
                <td>${formatSize(item.size)}</td>
            </tr>
        `;
    })
    .join("");
}

// Event Handlers
function handlePatternKeydown(event) {
  const textarea = event.target;
  textarea.style.height = "auto";
  textarea.style.height = textarea.scrollHeight + "px";

  if (event.key === "Enter") {
    event.preventDefault();
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    const value = textarea.value;

    textarea.value = value.substring(0, start) + "\n" + value.substring(end);
    textarea.selectionStart = textarea.selectionEnd = start + 1;
    textarea.style.height = textarea.scrollHeight + "px";
  }
}

// Initialize Application
document.addEventListener("DOMContentLoaded", () => {
  startServerHealthCheck();
  const settingsModal = document.getElementById("settings-modal");
  const openSettingsBtn = document.getElementById("open-settings");
  const closeSettingsBtn = document.getElementById("close-settings");
  const apiTokenInput = document.getElementById("api-token");
  const downloadPathInput = document.getElementById("download-path");

  // Load saved download path
  const savedPath = localStorage.getItem("download_path");
  if (savedPath) {
    downloadPathInput.value = savedPath;
  }

  // Save path when changed
  downloadPathInput.addEventListener('change', () => {
    const path = downloadPathInput.value.trim();
    if (path) {
      localStorage.setItem("download_path", path);
    }
  });

  // Settings Modal
  openSettingsBtn.addEventListener("click", () => {
    settingsModal.style.display = "flex";
  });

  closeSettingsBtn.addEventListener("click", () => {
    settingsModal.style.display = "none";
  });

  settingsModal.addEventListener("click", (e) => {
    if (e.target === settingsModal) {
      settingsModal.style.display = "none";
    }
  });

  // Initialize components
  fetchSettings();
  loadPipelines();
  setupSearch();
  updateHistoryUI();

  // Event listeners for pipelines
  document
    .getElementById("settings-form")
    .addEventListener("submit", saveSettings);
  document
    .getElementById("refresh-pipelines")
    .addEventListener("click", () => loadPipelines());
  document
    .getElementById("test-connection")
    .addEventListener("click", testConnection);

  // Pagination handlers
  document.getElementById("prev-page").addEventListener("click", () => {
    if (currentPage > 1) {
      currentPage--;
      loadPipelines();
    }
  });

  document.getElementById("next-page").addEventListener("click", () => {
    currentPage++;
    loadPipelines();
  });

  // Clear completed downloads
  document.getElementById("clear-completed")?.addEventListener("click", () => {
    const activeDownloads = document.getElementById("active-downloads");
    const completedItems = Array.from(
      activeDownloads.querySelectorAll(".download-status")
    ).filter((item) => {
      const stateText = item
        .querySelector(".download-state")
        .textContent.trim()
        .toLowerCase();
      return stateText === "complete" || stateText === "completed";
    });

    if (completedItems.length === 0) {
      showToast("No completed downloads available to clear", "info");
      return;
    }

    completedItems.forEach((download) => {
      download.classList.add("fade-out");
      setTimeout(() => download.remove(), 300);
    });

    showToast(
      `Cleared ${completedItems.length} completed download(s)`,
      "success"
    );
  });
});

// Folder Selection
async function handleFolderSelection() {
  return new Promise((resolve) => {
      const input = document.createElement('input');
      input.type = 'file';
      input.webkitdirectory = true;
      input.directory = true;

      input.addEventListener('change', function(e) {
          if (e.target.files.length > 0) {
              const file = e.target.files[0];
              // ใช้ file.path เพื่อรับ path เต็มของไฟล์
              let folderPath = file.path;
              
              if (folderPath) {
                  // ตัดชื่อไฟล์ออกจาก path
                  folderPath = folderPath.substring(0, folderPath.lastIndexOf('\\'));
                  
                  // ตรวจสอบและลบ \ ที่อยู่ท้าย path ถ้ามี
                  if (folderPath.endsWith('\\')) {
                      folderPath = folderPath.slice(0, -1);
                  }
                  
                  resolve(folderPath);
              }
          }
      });

      input.click();
  });
}
// Status formatting
function formatStatus(status) {
  const statusMap = {
    started: "Starting...",
    in_progress: "Downloading",
    completed: "Complete",
    failed: "Failed",
    paused: "Paused",
  };
  return statusMap[status] || status;
}

// Path validation
function validatePath(path) {
  if (!path) return false;

  const windowsPathRegex =
    /^[a-zA-Z]:\\(?:[^\\/:*?"<>|\r\n]+\\)*[^\\/:*?"<>|\r\n]*$/;
  const unixPathRegex = /^\/(?:[^/]+\/)*[^/]*$/;

  return windowsPathRegex.test(path) || unixPathRegex.test(path);
}

// History Management
function clearDownloadHistory() {
  const confirmClear = confirm(
    "Are you sure you want to clear all download history? This action cannot be undone."
  );

  if (confirmClear) {
    try {
      localStorage.removeItem("download_history");

      const historyTable = document.getElementById("history-table-body");
      if (historyTable) {
        historyTable.innerHTML = `
                    <tr>
                        <td colspan="5" class="text-center">No download history</td>
                    </tr>
                `;
      }

      showToast("Download history cleared successfully", "success");
    } catch (error) {
      console.error("Error clearing history:", error);
      showToast("Failed to clear download history", "error");
    }
  }
}

function formatTimeRemaining(seconds) {
  if (!Number.isFinite(seconds) || seconds < 0) {
      return "--";
  }

  if (seconds < 60) {
      return `${Math.round(seconds)}s`;
  } else if (seconds < 3600) {
      const minutes = Math.floor(seconds / 60);
      const remainingSeconds = Math.round(seconds % 60);
      return `${minutes}m ${remainingSeconds}s`;
  } else {
      const hours = Math.floor(seconds / 3600);
      const minutes = Math.floor((seconds % 3600) / 60);
      return `${hours}h ${minutes}m`;
  }
}

// Initialize clear history button listener
document
  .getElementById("clear-history")
  ?.addEventListener("click", clearDownloadHistory);