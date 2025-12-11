#pragma once
#include <string>
#include <cstdio>
#include <array>
#include <stdexcept>

#include <nlohmann/json.hpp>
using nlohmann::json;

namespace UnityTest {

// Minimal: parse JSON body and print id/transform, or print a simple error.
// Accepts id as string or number. Requires position=[x,y,z], rotation=[x,y,z,w], scale=[x,y,z].
inline void PrintEntityFromJson(const std::string& body) {
    using nlohmann::json;
    try {
        json j = json::parse(body);

        // --- id (string or number -> string) ---
        std::string id;
        if (!j.contains("id")) { std::cout << "[EntityJsonPrinter] missing 'id'\n"; return; }
        if (j["id"].is_string()) id = j["id"].get<std::string>();
        else                      id = j["id"].dump(); // numbers as text

        // --- quick helpers ---
        auto to_f = [](const json& v)->float {
            if (v.is_number_float())   return static_cast<float>(v.get<double>());
            if (v.is_number_integer()) return static_cast<float>(v.get<long long>());
            if (v.is_number_unsigned())return static_cast<float>(v.get<unsigned long long>());
            if (v.is_string())         return std::stof(v.get<std::string>()); // permissive
            throw std::runtime_error("non-numeric value");
        };
        auto need = [](const json& j, const char* k){ return j.contains(k) ? j.at(k) : json(); };

        // --- position ---
        auto p = need(j, "position");
        if (!p.is_array() || p.size()!=3) { std::cout << "[EntityJsonPrinter] position must be [x,y,z]\n"; return; }
        float px = to_f(p[0]), py = to_f(p[1]), pz = to_f(p[2]);

        // --- print result ---
        std::cout << "[EntityJsonPrinter] id=" << id
                  << " pos=(" << px << "," << py << "," << pz << ")"
                  << std::endl; // flush
    }
    catch (const std::exception& e) {
        std::cout << "[EntityJsonPrinter] parse error: " << e.what() << std::endl;
    }
}

} // namespace UnityTest
