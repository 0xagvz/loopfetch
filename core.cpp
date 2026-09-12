#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include "core.hpp"
#include <iostream>

namespace {
volatile std::sig_atomic_t g_interrupted = 0;
void on_sigint(int) { g_interrupted = 1; }
}

bool does_file_exist(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "r");
    if (file) {
        std::fclose(file);
        return true;
    }
    return false;
}

std::string xdg_cache_base() {
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    std::string base;
    if (xdg && *xdg) {
        base = xdg;
    } else {
        const char* home = std::getenv("HOME");
        base = (home && *home) ? std::string(home) + "/.cache" : "/tmp";
    }
    return base + "/loopfetch";
}

std::string hash_key(const std::string& s) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << h;
    return oss.str();
}

std::string cache_key_for(const std::string& video_path, int width, int height, int fps) {
    namespace fs = std::filesystem;
    std::error_code ec;
    std::string abs = fs::absolute(video_path, ec).string();
    if (ec) abs = video_path;
    ec.clear();
    uint64_t sz = fs::file_size(video_path, ec);
    if (ec) sz = 0;
    ec.clear();
    long long mtime = 0;
    auto wt = fs::last_write_time(video_path, ec);
    if (!ec) mtime = (long long)wt.time_since_epoch().count();
    return abs + "|" + std::to_string(sz) + "|" + std::to_string(mtime)
        + "|" + std::to_string(width) + "x" + std::to_string(height)
        + "@" + std::to_string(fps) + "fps";
}

std::string cache_dir_for(const std::string& video_path, int width, int height, int fps) {
    return xdg_cache_base() + "/" + hash_key(cache_key_for(video_path, width, height, fps));
}

bool is_cached(const std::string& output_dir, const std::string& key) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir(output_dir);
    uint64_t sz = fs::file_size(dir / CACHE_FILENAME, ec);
    if (ec || sz == 0) return false;
    std::ifstream kf(dir / CACHE_KEY_FILENAME);
    if (!kf) return false;
    std::string stored;
    std::getline(kf, stored);
    if (!stored.empty() && stored.back() == '\r') stored.pop_back();
    return stored == key;
}

bool getvidres(const std::string& path, int& width, int& height) {
    std::string cmd = "ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=s=x:p=0 \"" + path + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error: failed to run ffprobe" << std::endl;
        return false;
    }
    char buffer[128] = {0};
    std::string res;
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        res += buffer;
    }
    int rc = pclose(pipe);
    if (rc != 0) {
        std::cerr << "Error: ffprobe failed for: " << path << std::endl;
        return false;
    }
    std::cout << "Probed dimensions: " << res;
    int probed_w = 0, probed_h = 0;
    if (std::sscanf(res.c_str(), "%dx%d", &probed_w, &probed_h) != 2) {
        std::cerr << "Error: could not parse ffprobe output: " << res << std::endl;
        return false;
    }
    width = probed_w;
    height = probed_h;
    return true;
}

int saveframes(const std::string& path, int width, int height, int fps, const std::string& output_dir) {
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        std::cerr << "Error: could not create output dir '" << output_dir << "': " << ec.message() << std::endl;
        return 1;
    }

    std::string command = "ffmpeg -hide_banner -loglevel error -y -i \"" + path + "\"";
    if (width > 0 && height > 0) {
        command += " -vf scale=" + std::to_string(width) + ":" + std::to_string(height);
    }
    if (fps > 0) {
        command += " -r " + std::to_string(fps);
    }
    command += " \"" + output_dir + "/frame_%04d.png\"";
    std::cout << "Running: " << command << std::endl;
    int rc = std::system(command.c_str());
    if (rc != 0) {
        std::cerr << "Error: ffmpeg failed with code " << rc << std::endl;
        return 1;
    }
    return 0;
}

static bool exec_capture(const std::string& cmd, std::string& out) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    char buffer[4096] = {0};
    out.clear();
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        out += buffer;
    }
    int rc = pclose(pipe);
    return rc == 0;
}

std::string frame2ascii(const std::string& frame_path, int ascii_width, int ascii_height) {
    int w = ascii_width > 0 ? ascii_width : 80;
    int h = ascii_height > 0 ? ascii_height : 25;
    std::string cmd = "chafa --symbols ascii --fg-only --format symbols --size="
        + std::to_string(w) + "x" + std::to_string(h);
    cmd += " \"" + frame_path + "\" 2>/dev/null";
    std::string out;
    if (!exec_capture(cmd, out)) {
        std::cerr << "Error: chafa failed for frame: " << frame_path << std::endl;
        return "";
    }
    return out;
}

int video2ascii(const std::string& frames_dir, int ascii_width, int ascii_height, const std::string& cache_path) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::is_directory(frames_dir, ec)) {
        std::cerr << "Error: frames dir does not exist: " << frames_dir << std::endl;
        return 1;
    }

    std::vector<fs::path> frames;
    for (const auto& entry : fs::directory_iterator(frames_dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
            frames.push_back(entry.path());
        }
    }
    if (ec) {
        std::cerr << "Error: could not list frames dir '" << frames_dir << "': " << ec.message() << std::endl;
        return 1;
    }
    if (frames.empty()) {
        std::cerr << "Error: no frames found in: " << frames_dir << std::endl;
        return 1;
    }
    std::sort(frames.begin(), frames.end());

    std::ofstream out(cache_path, std::ios::trunc);
    if (!out) {
        std::cerr << "Error: could not open cache file for writing: " << cache_path << std::endl;
        return 1;
    }

    size_t n = 0;
    for (const auto& f : frames) {
        std::string ascii = frame2ascii(f.string(), ascii_width, ascii_height);
        if (ascii.empty()) {
            std::cerr << "Warning: empty ascii for frame: " << f << std::endl;
        }
        out << ascii;
        if (!ascii.empty() && ascii.back() != '\n') {
            out << '\n';
        }
        out << FRAME_SEPARATOR << '\n';
        ++n;
    }
    out.close();
    std::cout << "Wrote " << n << " ascii frames to cache: " << cache_path << std::endl;
    return 0;
}

std::vector<std::string> read_ascii_cache(const std::string& cache_path) {
    std::vector<std::string> frames;
    std::ifstream in(cache_path);
    if (!in) {
        std::cerr << "Error: could not open cache file: " << cache_path << std::endl;
        return frames;
    }
    std::string line, current;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == FRAME_SEPARATOR) {
            frames.push_back(current);
            current.clear();
        } else {
            current += line;
            current += '\n';
        }
    }
    if (!current.empty()) {
        frames.push_back(current);
    }
    return frames;
}

static std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> lines;
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> get_fetch_output(bool use_neofetch, const std::string& config) {
    std::string cmd = use_neofetch ? "neofetch --off" : "fastfetch --logo none --pipe false";
    if (!config.empty()) {
        cmd += " --config \"" + config + "\"";
    }
    cmd += " 2>/dev/null";
    std::string out;
    if (!exec_capture(cmd, out)) {
        std::cerr << "Error: fetch command failed: " << cmd << std::endl;
        return {};
    }
    return split_lines(out);
}

std::string make_template_from_fetch_lines(const std::vector<std::string>& fetch_lines) {
    std::string t;
    for (size_t i = 0; i < fetch_lines.size(); ++i) {
        if (i) t += '\n';
        t += fetch_lines[i];
    }
    return t;
}

size_t visible_len(const std::string& s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
            i += 2;
            while (i < s.size() && !((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z'))) {
                ++i;
            }
            if (i < s.size()) ++i;
        } else {
            ++n;
            ++i;
        }
    }
    return n;
}

std::string render_frame(const std::vector<std::string>& ascii_lines, const std::vector<std::string>& fetch_lines, int top, int left_pad, int gap) {
    size_t left_w = 0;
    for (const auto& l : ascii_lines) {
        left_w = std::max(left_w, visible_len(l));
    }
    const std::string pad_left(left_pad < 0 ? 0 : (size_t)left_pad, ' ');
    const std::string pad_gap(gap < 0 ? 0 : (size_t)gap, ' ');
    size_t rows = std::max(ascii_lines.size(), fetch_lines.size());
    std::string out;
    for (int i = 0; i < (top < 0 ? 0 : top); ++i) {
        out += '\n';
    }
    for (size_t r = 0; r < rows; ++r) {
        out += pad_left;
        std::string left = r < ascii_lines.size() ? ascii_lines[r] : "";
        out += left;
        size_t vlen = visible_len(left);
        if (vlen < left_w) {
            out += std::string(left_w - vlen, ' ');
        }
        out += pad_gap;
        if (r < fetch_lines.size()) {
            out += fetch_lines[r];
        }
        out += '\n';
    }
    return out;
}

static std::string describe_key(const char* buf, ssize_t n) {
    if (n <= 0) return "";
    unsigned char c = static_cast<unsigned char>(buf[0]);
    if (c == 27) return "ESC";
    if (c >= 32 && c < 127) return std::string(1, static_cast<char>(c));
    char tmp[8];
    std::snprintf(tmp, sizeof tmp, "0x%02X", c);
    return tmp;
}

static bool push_back_to_tty(const std::string& bytes) {
    if (bytes.empty() || !isatty(STDIN_FILENO)) return false;
    for (unsigned char c : bytes) {
        if (ioctl(STDIN_FILENO, TIOCSTI, &c) != 0) return false;
    }
    return true;
}

int play_ascii_frames(const std::vector<std::string>& frames, const std::vector<std::string>& fetch_lines, int fps, int loops, int top, int left_pad, int gap) {
    if (frames.empty()) {
        std::cerr << "Error: no frames to play" << std::endl;
        return 1;
    }
    long usec = (fps > 0) ? 1000000L / fps : 100000L; 
    if (usec < 20000) usec = 20000;
    g_interrupted = 0;

    const bool in_tty = isatty(STDIN_FILENO);
    const bool out_tty = isatty(STDOUT_FILENO);

    struct termios saved{};
    bool raw = false;
    void (*old_handler)(int) = SIG_DFL;
    if (in_tty && tcgetattr(STDIN_FILENO, &saved) == 0) {
        struct termios r = saved;
        r.c_lflag &= ~(ICANON | ECHO);
        r.c_cc[VMIN] = 0;
        r.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &r) == 0) {
            raw = true;
            old_handler = std::signal(SIGINT, on_sigint);
            char tmp[64];
            while (read(STDIN_FILENO, tmp, sizeof tmp) > 0) {}
        }
    }

    auto restore_terminal = [&]() {
        if (raw) {
            tcsetattr(STDIN_FILENO, TCSANOW, &saved);
            std::signal(SIGINT, old_handler);
            raw = false;
        }
        if (out_tty) std::cout << "\033[?25h" << std::flush; 
    };

    const std::string clear = out_tty ? "\033[2J\033[H" : "";
    if (out_tty) std::cout << "\033[2J\033[3J\033[H" << std::flush;

    std::string last_key;
    std::string pressed;
    bool quit = false;
    int loop = 0;
    while ((loops == 0 || loop < loops) && !quit && !g_interrupted) {
        for (const auto& f : frames) {
            std::string screen = render_frame(split_lines(f), fetch_lines, top, left_pad, gap);
            std::cout << clear << screen << std::flush;
            long remaining = usec;
            while (remaining > 0 && !quit && !g_interrupted) {
                if (!raw) {
                    std::this_thread::sleep_for(std::chrono::microseconds(remaining));
                    remaining = 0;
                } else {
                    long step = std::min(remaining, 50000L);
                    fd_set rfds;
                    FD_ZERO(&rfds);
                    FD_SET(STDIN_FILENO, &rfds);
                    struct timeval tv{static_cast<time_t>(step / 1000000),
                                      static_cast<suseconds_t>(step % 1000000)};
                    int r = select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
                    if (r < 0) {
                        if (errno == EINTR) continue;
                        remaining = 0;
                    } else if (r == 0) {
                        remaining -= step;
                    } else {
                        char buf[16];
                        ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
                        if (n > 0) {
                            pressed.assign(buf, (size_t)n);
                            last_key = describe_key(buf, n);
                            quit = true;
                        }
                    }
                }
            }
            if (quit || g_interrupted) break;
        }
        ++loop;
    }

    restore_terminal();
    if (g_interrupted) {
        std::cout << "\n[interrupted]" << std::endl;
        return 130;
    }
    if (quit) {
        if (!push_back_to_tty(pressed)) {
            std::cout << "[key: " + last_key + "]" << std::endl;
        }
    }
    return 0;
}

int preprocessvid(const std::string& path, int width, int height, int fps, std::string& output_path) {
    if (!does_file_exist(path)) {
        std::cerr << "Error: File does not exist: " << path << std::endl;
        return 1;
    }

    const int ascii_w = width;
    const int ascii_h = height;

    if (!width || !height) {
        int probed_w = 0, probed_h = 0;
        if (!getvidres(path, probed_w, probed_h)) {
            return 1;
        }
        if (!width) width = probed_w;
        if (!height) height = probed_h;
    }

    if (output_path.empty() || output_path == path) {
        output_path = "./frames";
    }

    int rc = saveframes(path, width, height, fps, output_path);
    if (rc != 0) {
        return rc;
    }

    const std::string cache_path =
        (std::filesystem::path(output_path) / CACHE_FILENAME).string();
    rc = video2ascii(output_path, ascii_w, ascii_h, cache_path);
    if (rc != 0) {
        return rc;
    }
    std::cout << "Cache ascii: " << cache_path << " (separador '" << FRAME_SEPARATOR << "')" << std::endl;
    std::ofstream kf(std::filesystem::path(output_path) / CACHE_KEY_FILENAME, std::ios::trunc);
    if (!kf) {
        std::cerr << "Warning: could not write cache key file" << std::endl;
    } else {
        kf << cache_key_for(path, ascii_w, ascii_h, fps) << '\n';
    }
    return 0;
}

