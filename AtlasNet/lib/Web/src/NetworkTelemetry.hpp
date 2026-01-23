#pragma once
#include <vector>
#include <string>

class NetworkTelemetry {
public:
    NetworkTelemetry() = default;
    
    void GetLivePingIDs(std::vector<std::string>& out_live_ids);
    void GetLivePingUploadSpeed(float &out_upload_kbps);
    void GetLivePingDownloadSpeed(float &out_download_kbps);
};