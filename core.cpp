#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
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
    // fps == 0 means "original", so omit -r (ffmpeg rejects `-r 0`)
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

int preprocessvid(const std::string& path, int width, int height, int fps, std::string& output_path) {
    if (!does_file_exist(path)) {
        std::cerr << "Error: File does not exist: " << path << std::endl;
        return 1;
    }

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

    return saveframes(path, width, height, fps, output_path);
}

