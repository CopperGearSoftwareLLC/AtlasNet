#include "NetworkTelemetry.hpp"
#include <iostream>
#include <unordered_map>
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "Network/ConnectionTelemetry.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Network/NetworkIdentity.hpp"

void NetworkTelemetry::GetLivePingIDs(std::vector<std::string>& out_live_ids, std::vector<std::string>& out_health) {
    out_live_ids.clear();
    out_health.clear();

    std::unordered_map<std::string, double> all_pings;
    HealthManifest::Get().GetAllPings(all_pings);
    const double now = InternalDB::Get()->GetTimeNowSeconds();

    for (const auto& pair : all_pings) {
        if (pair.second <= now) {
            continue;
        }

        ByteReader br(pair.first);
        NetworkIdentity id;
        id.Deserialize(br);

        if (!id.IsInternal() || id.Type == NetworkIdentityType::eInvalid ||
            id.Type == NetworkIdentityType::eAtlasNetInitial) {
            continue;
        }

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
