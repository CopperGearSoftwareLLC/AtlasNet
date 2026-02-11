#include "NetworkTelemetry.hpp"
#include <iostream>
#include <unordered_map>
#include "Database/HealthManifest.hpp"
#include "Telemetry/NetworkManifest.hpp"
#include "Network/ConnectionTelemetry.hpp"
#include "Serialize/ByteReader.hpp"
#include "Network/NetworkIdentity.hpp"

void NetworkTelemetry::GetLivePingIDs(std::vector<std::string>& out_live_ids, std::vector<std::string>& out_health) {
    std::unordered_map<std::string, double> all_pings;

    HealthManifest::Get().GetAllPings(all_pings);

    for (const auto& pair : all_pings) {

        ByteReader br(pair.first);
        //ByteReader br_val(pair.second);
        NetworkIdentity id;
        id.Deserialize(br);

        std::cerr << id.ToString() << std::endl;

        out_live_ids.push_back(id.ToString());
        out_health.push_back(std::to_string(pair.second));
    }

    //std::cerr << "Live Ping IDs: " << out_live_ids.size() << std::endl;
    //std::cerr << "Live Healths: " << out_health.size() << std::endl;
}

void NetworkTelemetry::GetAllTelemetry(std::vector<std::vector<std::string>>& out_telemetry) {
    NetworkManifest::Get().GetAllTelemetry(out_telemetry);


    //for (const auto& telemetry_entry : out_telemetry) {
    //    std::cerr << "Telemetry Entry:" << std::endl;
    //    for (const auto& field : telemetry_entry) {
    //        std::cerr << "  " << field << std::endl;
    //    }
    //}
}

void NetworkTelemetry::GetLivePingUploadSpeed(float &out_upload_kbps) {
    //HealthManifest::Get().GetLivePingUploadSpeed(out_upload_kbps);
}

void NetworkTelemetry::GetLivePingDownloadSpeed(float &out_download_kbps) {
    //HealthManifest::Get().GetLivePingDownloadSpeed(out_download_kbps);
}