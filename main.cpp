#include <iostream>
#include <string>
#include "argparse/argparse.hpp"
#include "core.hpp"

int run_loopfetch(const std::string& path, int width, int height, int fps) {
    return preprocessvid(path, width, height, fps, path);
}

int main(int argc, char* argv[]) {
    // Arguments: 
    // -p --path /path/to/vid
    // -w --width 
    // -h --height
    // -f --fps
    // --version
    argparse::ArgumentParser program("AniFetchNative", "0.1.0");

    program.add_argument("-p", "--path")
        .help("/path/to/vid")
        .required();
    program.add_argument("-w", "--width")
        .help("output width")
        .scan<'i', int>();
    program.add_argument("-h", "--height")
        .help("output height")
        .scan<'i', int>();
    program.add_argument("-f", "--fps")
        .help("frames per second")
        .scan<'i', int>();

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    run_loopfetch(
        program.get<std::string>("--path"),
        program.get<int>("--width"),
        program.get<int>("--height"),
        program.get<int>("--fps")
    );

    return 0;
}