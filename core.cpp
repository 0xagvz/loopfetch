#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "core.hpp"
#include <iostream>

bool does_file_exist(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "r");
    if (file) {
        std::fclose(file);
        return true;
    }
    return false;
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

    std::string command = "ffmpeg -y -i \"" + path + "\"";
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
        (std::filesystem::path(output_path) / "cache.ascii").string();
    rc = video2ascii(output_path, ascii_w, ascii_h, cache_path);
    if (rc != 0) {
        return rc;
    }
    std::cout << "Cache ascii: " << cache_path << " (separador '" << FRAME_SEPARATOR << "')" << std::endl;
    return 0;
}

