#include <pl/pattern_language.hpp>
#include <wolv/io/file.hpp>

#include <iostream>
#include <pl/helpers/utils.hpp>
#include <wolv/utils/string.hpp>

int main(int argc, char **argv) {
    if(argc < 2) {
        std::cout << "Invalid number of arguments specified! " << argc << std::endl;
        return EXIT_FAILURE;
    }

    std::fs::path path;

    if(strncmp(argv[1], "-t", 2) == 0) {
        if(argc != 3 ) {
            std::cout << "Invalid number of arguments specified! " << argc << std::endl;
            return EXIT_FAILURE;
        }
        std::string base = argv[2]; // base path
        std::cout << "Number: " << std::endl;
        int number = 0;
        std::cin >> number;
        std::vector<std::fs::path> paths;
        for(auto &file : std::filesystem::directory_iterator(base)) {
            paths.push_back(file.path());
        }
        // sort by name
        std::ranges::sort(paths, [](const std::fs::path &a, const std::fs::path &b) {
            return a.filename().string() < b.filename().string();
        });
        path = paths[number];
        std::cout << "Executing: " << path << std::endl;
    } else {
        path = argv[1];
    }

    wolv::io::File patternFile(path, wolv::io::File::Mode::Read);

    pl::PatternLanguage runtime;

    auto result =
        runtime.parseString(patternFile.readString(), wolv::util::toUTF8String(path));

    return EXIT_SUCCESS;
}
