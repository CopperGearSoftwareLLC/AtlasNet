#include "NetworkTelemetry.hpp"
#include <iostream>
#include <unordered_map>
#include "Database/HealthManifest.hpp"
#include "Serialize/ByteReader.hpp"
#include "InterlinkIdentifier.hpp"

void NetworkTelemetry::GetLivePingIDs(std::vector<std::string>& out_live_ids) {
    std::unordered_map<std::string, double> all_pings;

    HealthManifest::Get().GetAllPings(all_pings);

    for (const auto& pair : all_pings) {

        ByteReader br(pair.first);
        InterLinkIdentifier id;
        id.Deserialize(br);

        std::cerr << id.ToString() << std::endl;

        out_live_ids.push_back(id.ToString());
    }

    std::cerr << "Live Ping IDs: " << out_live_ids.size() << std::endl;
}

void NetworkTelemetry::GetLivePingUploadSpeed(float &out_upload_kbps) {
    //HealthManifest::Get().GetLivePingUploadSpeed(out_upload_kbps);
}

void NetworkTelemetry::GetLivePingDownloadSpeed(float &out_download_kbps) {
    //HealthManifest::Get().GetLivePingDownloadSpeed(out_download_kbps);
}