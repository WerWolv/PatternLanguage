#pragma once

#include <pl/pattern_language.hpp>

#include <string>
#include <vector>

namespace pl::cli {

    int executeCommandLineInterface(std::vector<std::string> args, pl::PatternLanguage &runtime);

}