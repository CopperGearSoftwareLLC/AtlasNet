// CmdRunner.hpp (single-header implementation)
// Streams child output optionally, and (when streaming on a TTY) clears EVERYTHING it printed when done.
// No "Running:" / "Done:" markers are printed.

#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>

struct CmdResult {
    int exit_code = -1;
    std::string stdout_text;
    std::string stderr_text;
    bool used_sudo = false;
    bool sudo_prompted = false; // true if we had to run interactively (sudo asked on tty)
};

static inline bool looks_like_permission_error(int exit_code, const std::string& err) {
    if (exit_code == 126) return true;
    if (exit_code == 13)  return true;
    if (err.find("Permission denied") != std::string::npos) return true;
    if (err.find("permission denied") != std::string::npos) return true;
    if (err.find("Operation not permitted") != std::string::npos) return true;
    if (err.find("operation not permitted") != std::string::npos) return true;
    return false;
}

static inline void write_all_fd(int fd, const char* data, size_t len) {
    while (len) {
        ssize_t n = ::write(fd, data, len);
        if (n > 0) { data += n; len -= (size_t)n; }
        else if (n < 0 && errno == EINTR) continue;
        else break;
    }
}

static inline bool stdout_is_tty() {
    return ::isatty(STDOUT_FILENO) == 1;
}

// Cursor save/restore + clear-to-end for ephemeral output (TTY only).
// Uses both styles for better compatibility.
static inline void term_save_cursor() {
    if (!stdout_is_tty()) return;
    write_all_fd(STDOUT_FILENO, "\0337", 2);  // DECSC
    write_all_fd(STDOUT_FILENO, "\033[s", 3); // ANSI (some terminals)
}
static inline void term_restore_cursor() {
    if (!stdout_is_tty()) return;
    write_all_fd(STDOUT_FILENO, "\0338", 2);  // DECRC
    write_all_fd(STDOUT_FILENO, "\033[u", 3); // ANSI
}
static inline void term_clear_to_end() {
    if (!stdout_is_tty()) return;
    write_all_fd(STDOUT_FILENO, "\033[J", 3); // clear from cursor to end of screen
}
static inline void term_restore_and_clear_to_end() {
    if (!stdout_is_tty()) return;
    term_restore_cursor();
    term_clear_to_end();
}

static inline void set_nonblocking(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) return;
    (void)fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

// Stream child stdout/stderr live to STDOUT (optional), while also capturing them.
static inline CmdResult run_capture_streaming(const std::vector<std::string>& argv, bool stream_live_to_stdout) {
    int out_pipe[2]{-1,-1};
    int err_pipe[2]{-1,-1};
    if (pipe(out_pipe) != 0) throw std::runtime_error("pipe(stdout) failed");
    if (pipe(err_pipe) != 0) throw std::runtime_error("pipe(stderr) failed");

    pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork failed");

    if (pid == 0) {
        // child
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);

        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (auto& s : argv) cargv.push_back(const_cast<char*>(s.c_str()));
        cargv.push_back(nullptr);

        execvp(cargv[0], cargv.data());
        _exit(127);
    }

    // parent
    close(out_pipe[1]);
    close(err_pipe[1]);

    set_nonblocking(out_pipe[0]);
    set_nonblocking(err_pipe[0]);

    CmdResult r;
    bool out_open = true;
    bool err_open = true;

    char buf[4096];

    while (out_open || err_open) {
        pollfd fds[2];
        int nfds = 0;
        int idx_out = -1, idx_err = -1;

        if (out_open) {
            idx_out = nfds;
            fds[nfds++] = pollfd{ out_pipe[0], short(POLLIN | POLLHUP | POLLERR), 0 };
        }
        if (err_open) {
            idx_err = nfds;
            fds[nfds++] = pollfd{ err_pipe[0], short(POLLIN | POLLHUP | POLLERR), 0 };
        }

        int pr = poll(fds, nfds, -1);
        if (pr < 0) {
            if (errno == EINTR) continue;
            break;
        }

        auto drain_fd = [&](int fd, std::string& sink, bool& open_flag) {
            for (;;) {
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n > 0) {
                    sink.append(buf, buf + n);
                    if (stream_live_to_stdout) write_all_fd(STDOUT_FILENO, buf, (size_t)n);
                    continue;
                }
                if (n == 0) { close(fd); open_flag = false; return; }
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) return;
                close(fd); open_flag = false; return;
            }
        };

        if (idx_out != -1 && (fds[idx_out].revents & (POLLIN | POLLHUP | POLLERR))) {
            drain_fd(out_pipe[0], r.stdout_text, out_open);
        }
        if (idx_err != -1 && (fds[idx_err].revents & (POLLIN | POLLHUP | POLLERR))) {
            drain_fd(err_pipe[0], r.stderr_text, err_open);
        }
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (WIFEXITED(status)) r.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) r.exit_code = 128 + WTERMSIG(status);
    else r.exit_code = -1;

    return r;
}

// Old behavior: capture only.
static inline CmdResult run_capture(const std::vector<std::string>& argv) {
    return run_capture_streaming(argv, /*stream_live_to_stdout=*/false);
}

static inline int run_inherit_tty(const std::vector<std::string>& argv) {
    // Inherit stdin/stdout/stderr so sudo can prompt on the terminal if needed.
    pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork failed");

    if (pid == 0) {
        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (auto& s : argv) cargv.push_back(const_cast<char*>(s.c_str()));
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

// Runs `bash -lc <cmd>`.
// If it fails due to permissions, retries with sudo.
// If sudo creds are cached, it stays non-interactive; otherwise sudo prompts on tty.
//
// If stream_live_to_stdout==true:
//   - streams stdout+stderr live to stdout
//   - and, if stdout is a TTY, clears everything the command printed when it finishes (including sudo prompt output)
//   - prints no extra markers.
static inline CmdResult run_bash_with_sudo_fallback(const std::string& cmd, bool stream_live_to_stdout) {
    const bool ephemeral = stream_live_to_stdout && stdout_is_tty();

    if (ephemeral) {
        // Mark where "ephemeral output" begins. Everything printed after this can be wiped.
        term_save_cursor();
    }

    auto clear_ephemeral_area = [&]() {
        if (!ephemeral) return;
        term_restore_and_clear_to_end();
        // Re-mark for a potential next attempt (sudo retry)
        term_save_cursor();
    };

    auto run_attempt = [&](const std::vector<std::string>& argv) -> CmdResult {
        return run_capture_streaming(argv, /*stream_live_to_stdout=*/stream_live_to_stdout);
    };

    // 1) Try without sudo
    CmdResult first = run_attempt({ "bash", "-lc", cmd });
    if (first.exit_code == 0) {
        if (ephemeral) term_restore_and_clear_to_end();
        return first;
    }

    if (!looks_like_permission_error(first.exit_code, first.stderr_text)) {
        if (ephemeral) term_restore_and_clear_to_end();
        return first;
    }

    // 2) Check sudo without prompting (cached creds)
    clear_ephemeral_area();
    CmdResult sudo_check = run_capture({ "sudo", "-n", "true" });
    if (sudo_check.exit_code == 0) {
        CmdResult r = run_attempt({ "sudo", "-n", "--", "bash", "-lc", cmd });
        r.used_sudo = true;
        r.sudo_prompted = false;
        if (ephemeral) term_restore_and_clear_to_end();
        return r;
    }

    // 3) Not cached -> interactive so sudo can prompt on tty
    clear_ephemeral_area();
    CmdResult r;
    r.used_sudo = true;
    r.sudo_prompted = true;
    r.exit_code = run_inherit_tty({ "sudo", "--", "bash", "-lc", cmd });

    if (ephemeral) term_restore_and_clear_to_end();
    return r;
}
