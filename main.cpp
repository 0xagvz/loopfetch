#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include "argparse/argparse.hpp"
#include "core.hpp"

int run_loopfetch(const std::string& path, int width, int height, int fps, std::string& output_path) {
    if (output_path.empty()) {
        output_path = cache_dir_for(path, width, height, fps);
        std::cout << "Using cache dir: " << output_path << std::endl;
    }

    const std::string key = cache_key_for(path, width, height, fps);
    if (is_cached(output_path, key)) {
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
        .required();
    program.add_argument("--width")
        .help("output width (0 = auto; also ascii cols)")
        .default_value(0)
        .scan<'i', int>();
    program.add_argument("--height")
        .help("output height (0 = auto; also ascii rows)")
        .default_value(0)
        .scan<'i', int>();
    program.add_argument("-f", "--fps")
        .help("frames per second (0 = original)")
        .default_value(0)
        .scan<'i', int>();
    program.add_argument("-o", "--output")
        .help("output dir for frames (default: hashed dir under $XDG_CACHE_HOME/loopfetch)")
        .default_value(std::string(""));

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

    return run_loopfetch(path, width, height, fps, output_path);
}