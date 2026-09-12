#pragma once
#include <string>
#include <vector>


inline const std::string FRAME_SEPARATOR = "/J/";
inline const std::string CACHE_FILENAME = "cache.ascii";
inline const std::string CACHE_KEY_FILENAME = ".loopfetch_key";

bool getvidres(const std::string& path, int& width, int& height);
std::string xdg_cache_base();
std::string hash_key(const std::string& s);
std::string cache_key_for(const std::string& video_path, int width, int height, int fps);
std::string cache_dir_for(const std::string& video_path, int width, int height, int fps);
bool is_cached(const std::string& output_dir, const std::string& key);
int saveframes(const std::string& path, int width, int height, int fps, const std::string& output_dir);
std::string frame2ascii(const std::string& frame_path, int ascii_width, int ascii_height);
int video2ascii(const std::string& frames_dir, int ascii_width, int ascii_height, const std::string& cache_path);
std::vector<std::string> read_ascii_cache(const std::string& cache_path);
int preprocessvid(const std::string& path, int width, int height, int fps, std::string& output_path);