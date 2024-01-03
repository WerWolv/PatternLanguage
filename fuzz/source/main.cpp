#include <pl/pattern_language.hpp>
#include <wolv/io/file.hpp>

#include <iostream>
#include <pl/helpers/utils.hpp>
#include <wolv/utils/string.hpp>

int main(int argc, char **argv) {
    if(argc != 2) {
        std::cout << "Invalid number of arguments specified! " << argc << std::endl;
        return EXIT_FAILURE;
    }

    wolv::io::File patternFile(argv[1], wolv::io::File::Mode::Read);

    pl::PatternLanguage runtime;

    auto result =
        runtime.parseString(patternFile.readString(), wolv::util::toUTF8String(patternFile.getPath()));
}
