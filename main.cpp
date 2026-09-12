#include <iostream>
#include <string>
#include "argparse/argparse.hpp"
#include "core.hpp"

int run_loopfetch(const std::string& path, int width, int height, int fps, std::string& output_path) {
    if (output_path.empty()) {
        // Save in cache folder with hashed frame (to-do function)
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
        .help("output dir for frames; ascii cache saved as <dir>/cache.ascii with '/J/' separators")
        .default_value(std::string("./frames"));

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