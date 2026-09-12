#pragma once
#include <string>

bool getvidres(const std::string& path, int& width, int& height);
int saveframes(const std::string& path, int width, int height, int fps, const std::string& output_dir);
int preprocessvid(const std::string& path, int width, int height, int fps, std::string& output_path);