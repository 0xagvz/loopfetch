#include <cstdio>
#include <cstdlib>
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

int preprocessvid(const std::string& path, int width, int height, int fps, std::string& output_path) {

    if (!does_file_exist(path)) {
        std::cerr << "Error: File does not exist: " << path << std::endl;
        return 1;
    }

    if (!width || !height) {
        std::string res = "ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=s=x:p=0" + path;
    }
    
    std::string command = "ffmpeg -i " + path + " -vf scale=" + std::to_string(width) + ":" + std::to_string(height) + " -r " + std::to_string(fps) + " output.mp4";
    return std::system(command.c_str());
}

