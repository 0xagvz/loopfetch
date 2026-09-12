#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include "argparse/argparse.hpp"
#include "core.hpp"

int run_loopfetch(const std::string& path, int width, int height, int fps, std::string& output_path) {
    if (output_path.empty()) {
        output_path = cache_dir_for(path, width, height, fps);
    }

    const std::string key = cache_key_for(path, width, height, fps);
    const std::string miss = cache_miss_reason(output_path, key);
    if (miss.empty()) {
        const std::string cache_path =
            (std::filesystem::path(output_path) / CACHE_FILENAME).string();
        std::vector<std::string> frames = read_ascii_cache(cache_path);
        if (!frames.empty()) {
            std::cout << "Cache hit: " << cache_path
                      << " (" << frames.size() << " frames, skipping render)"
                      << std::endl;
            return 0;
        }
        std::cerr << "Warning: cache unreadable, regenerating..." << std::endl;
    } else {
        std::cout << "Caching ascii..." << std::endl;
    }

    return preprocessvid(path, width, height, fps, output_path);
}

int main(int argc, char* argv[]) {
    // Arguments:
    // -p --path /path/to/vid (required)
    // --width  (0 = auto)
    // --height    (0 = auto, sin -h porque -h es --help)
    // -f --fps    (0 = original)
    // -v --version
    // -o --output /path/to/output (default: hashed dir under $XDG_CACHE_HOME/loopfetch)

    argparse::ArgumentParser program("loopfetch", "0.1.0");

    program.add_argument("-p", "--path")
        .help("/path/to/vid")
        .default_value(std::string(""));
    program.add_argument("--width")
        .help("output width (0 = auto; if alone, height follows aspect)")
        .default_value(0)
        .scan<'i', int>();
    program.add_argument("--height")
        .help("output height (0 = auto; if alone, width follows aspect)")
        .default_value(0)
        .scan<'i', int>();
    program.add_argument("-f", "--fps")
        .help("frames per second (0 = original)")
        .default_value(0)
        .scan<'i', int>();
    program.add_argument("-o", "--output")
        .help("output dir for frames (default: hashed dir under $XDG_CACHE_HOME/loopfetch)")
        .default_value(std::string(""));
    program.add_argument("-n", "--neofetch")
        .help("use neofetch instead of fastfetch")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("--fetch-config")
        .help("config file for fastfetch/neofetch")
        .default_value(std::string(""));
    program.add_argument("--top")
        .help("empty rows on top of the layout")
        .default_value(1)
        .scan<'i', int>();
    program.add_argument("--left")
        .help("spaces on the left of the layout")
        .default_value(4)
        .scan<'i', int>();
    program.add_argument("--gap")
        .help("spaces between ascii and fetch text")
        .default_value(2)
        .scan<'i', int>();
    program.add_argument("--loops")
        .help("playback loops (0 = infinite)")
        .default_value(0)
        .scan<'i', int>();
    program.add_argument("--clean")
        .help("delete cached frames and exit")
        .default_value(false)
        .implicit_value(true);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    std::string path = program.get<std::string>("--path");
    int width = program.get<int>("--width");
    int height = program.get<int>("--height");
    int fps = program.get<int>("--fps");
    std::string output_path = program.get<std::string>("--output");
    bool use_neofetch = program.get<bool>("--neofetch");
    std::string fetch_config = program.get<std::string>("--fetch-config");
    int top = program.get<int>("--top");
    int left_pad = program.get<int>("--left");
    int gap = program.get<int>("--gap");
    int loops = program.get<int>("--loops");
    bool do_clean = program.get<bool>("--clean");

    if (do_clean) {
        return clean_cache();
    }

    int rc = run_loopfetch(path, width, height, fps, output_path);
    if (rc != 0) return rc;

    const std::string cache_path =
        (std::filesystem::path(output_path) / CACHE_FILENAME).string();
    std::vector<std::string> frames = read_ascii_cache(cache_path);
    if (frames.empty()) {
        std::cerr << "Error: no frames in cache: " << cache_path << std::endl;
        return 1;
    }
    std::vector<std::string> fetch_lines = get_fetch_output(use_neofetch, fetch_config);
    if (fetch_lines.empty()) {
        std::cerr << "Warning: fetch output empty, playing ascii only" << std::endl;
    }
    return play_ascii_frames(frames, fetch_lines, fps, loops, top, left_pad, gap);
}