// EntityJsonReciever.hpp
// Minimal, single-threaded HTTP listener using POSIX sockets.
// Accepts POST /entity with a JSON body
//
// This is "quick & dirty": good for testing. Not production-hardened.
// - Single-threaded: handles one request at a time
// - Blocking I/O
// - Parses only what's needed (headers + Content-Length + body)
// - Returns 200 JSON {"ok":true} on /entity, 404 otherwise

#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cerrno>

// POSIX sockets
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "EntityJsonPrinter.hpp"  // uses nlohmann/json

class EntityJsonReciever {
public:
    explicit EntityJsonReciever(std::string addr = "0.0.0.0", int port = 18080)
        : addr_(std::move(addr)), port_(port) {}

    bool start() {
        if (running_.exchange(true)) return true;

        // create socket
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) { perror("[EntityJsonReciever] socket"); running_ = false; return false; }

        // reuse addr
        int yes = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        // bind
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port_);
        sa.sin_addr.s_addr = inet_addr(addr_.c_str());
        if (::bind(listen_fd_, (sockaddr*)&sa, sizeof(sa)) < 0) {
            perror("[EntityJsonReciever] bind");
            ::close(listen_fd_);
            listen_fd_ = -1;
            running_ = false;
            return false;
        }

        // listen
        if (::listen(listen_fd_, 16) < 0) {
            perror("[EntityJsonReciever] listen");
            ::close(listen_fd_);
            listen_fd_ = -1;
            running_ = false;
            return false;
        }

        th_ = std::thread([this]{ loop(); });
        std::printf("[EntityJsonReciever] Listening on %s:%d\n", addr_.c_str(), port_);
        return true;
    }

    void stop() {
        if (!running_.exchange(false)) return;
        if (listen_fd_ >= 0) { ::shutdown(listen_fd_, SHUT_RDWR); ::close(listen_fd_); listen_fd_ = -1; }
        if (th_.joinable()) th_.join();
        std::printf("[EntityJsonReciever] Stopped.\n");
    }

    ~EntityJsonReciever() { stop(); }

private:
    static bool recv_all(int fd, void* buf, size_t len) {
        char* p = static_cast<char*>(buf);
        size_t got = 0;
        while (got < len) {
            ssize_t r = ::recv(fd, p + got, len - got, 0);
            if (r <= 0) return false;
            got += size_t(r);
        }
        return true;
    }

    void loop() {
        while (running_) {
            sockaddr_in cli{}; socklen_t clilen = sizeof(cli);
            int fd = ::accept(listen_fd_, (sockaddr*)&cli, &clilen);

            //std::cout << "[EntityJsonReciever] Accepted connection" << std::endl;


            if (fd < 0) {
                if (running_) perror("[EntityJsonReciever] accept");
                continue;
            }

            // Read request headers (up to a cap)
            std::string req; req.reserve(8192);
            char buf[2048];
            bool headers_done = false;
            size_t header_end = std::string::npos;

            
            
            // Read until "\r\n\r\n" found
            while (!headers_done) {
              ssize_t r = ::recv(fd, buf, sizeof(buf), 0);
              if (r <= 0) break;
              req.append(buf, buf + r);
              header_end = req.find("\r\n\r\n");
              if (header_end != std::string::npos) headers_done = true;
              if (req.size() > 1<<20) break; // 1MB cap
            }
            
            
            
            if (!headers_done) {
              ::close(fd);
              continue;
            }
            
            // Parse first line + headers
            auto first_line_end = req.find("\r\n");
            std::string first_line = req.substr(0, first_line_end);
            
            // method + target (very basic split)
            std::string method, target;
            {
              size_t sp1 = first_line.find(' ');
              size_t sp2 = (sp1==std::string::npos)?std::string::npos:first_line.find(' ', sp1+1);
              if (sp1 != std::string::npos) method = first_line.substr(0, sp1);
              if (sp2 != std::string::npos) target = first_line.substr(sp1+1, sp2 - (sp1+1));
            }

            // Find Content-Length
            size_t content_length = 0;
            {
              size_t pos = 0;
              while (true) {
                size_t line_start = (pos==0)? first_line_end + 2 : pos;
                size_t line_end = req.find("\r\n", line_start);
                if (line_end == std::string::npos || line_start >= header_end) break;
                std::string line = req.substr(line_start, line_end - line_start);
                  pos = line_end + 2;
                  
                  const char* key = "Content-Length:";
                  if (line.size() >= strlen(key) && strncasecmp(line.c_str(), key, strlen(key)) == 0) {
                    content_length = std::strtoul(line.c_str() + strlen(key), nullptr, 10);
                  }
                }
              }
              
            // Read body
            std::string body;
            size_t already = req.size() - (header_end + 4);
            if (content_length > 0) {
                body.reserve(content_length);
                // we may already have some bytes from the body in req
                if (already > 0) {
                    size_t take = std::min(already, content_length);
                    body.append(req.data() + header_end + 4, take);
                }
                // read the rest from socket
                size_t need = (content_length > already) ? (content_length - already) : 0;
                while (need > 0) {
                    ssize_t r = ::recv(fd, buf, std::min(sizeof(buf), need), 0);
                    if (r <= 0) break;
                    body.append(buf, buf + r);
                    need -= size_t(r);
                }
            }

            // Handle
            std::string resp;
            if (method == "POST" && target == "/entity") {
                UnityTest::PrintEntityFromJson(body);
                std::string ok = R"({"ok":true})";
                resp  = "HTTP/1.1 200 OK\r\n";
                resp += "Content-Type: application/json\r\n";
                resp += "Content-Length: " + std::to_string(ok.size()) + "\r\n";
                resp += "Connection: close\r\n\r\n";
                resp += ok;
            } else {
                std::string notfound = "Not found";
                resp  = "HTTP/1.1 404 Not Found\r\n";
                resp += "Content-Type: text/plain\r\n";
                resp += "Content-Length: " + std::to_string(notfound.size()) + "\r\n";
                resp += "Connection: close\r\n\r\n";
                resp += notfound;
            }

            // Send response and close
            ::send(fd, resp.data(), resp.size(), 0);
            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
        }
    }

    std::string addr_;
    int port_{};
    std::atomic<bool> running_{false};
    int listen_fd_{-1};
    std::thread th_;
};
