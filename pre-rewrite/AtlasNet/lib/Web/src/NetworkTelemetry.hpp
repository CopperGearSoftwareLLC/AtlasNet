#pragma once
#include <vector>
#include <string>

class NetworkTelemetry {
public:
    NetworkTelemetry() = default;
    
    void GetLivePingIDs(std::vector<std::string>& out_live_ids, std::vector<std::string>& out_health);
    void GetAllTelemetry(std::vector<std::vector<std::string>>& out_telemetry);
    void GetLivePingUploadSpeed(float &out_upload_kbps);
    void GetLivePingDownloadSpeed(float &out_download_kbps);
};