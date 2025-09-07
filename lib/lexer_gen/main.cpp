/*
Build a "static" lexer in release builds.

Static in the sense that the state machine is built in a pre-build
step to optimize application start-up time. 
*/
#include <pl/core/lexer_sm.hpp>

#include <lexertl/state_machine.hpp>
#include <lexertl/generate_cpp.hpp>

#include <fstream>
#include <filesystem>

int main(int argc, char *argv[])
{
    if (argc!=2)
        return 1;

    try
    {
        std::filesystem::path argPath(argv[1]);
        std::filesystem::path genDir = argPath.parent_path();
        std::filesystem::create_directory(genDir);
    }
    catch (const std::filesystem::filesystem_error &fse)
    {
        return 1;
    }

    lexertl::state_machine sm;
    pl::core::newLexerBuild(sm);
    sm.minimise();

    std::ofstream ofs(argv[1]);
    lexertl::table_based_cpp::generate("lookup", sm, false, ofs);

    return 0;
}
