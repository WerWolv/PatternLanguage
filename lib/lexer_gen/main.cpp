#include <pl/core/new_lexer_sm.hpp>

#include <lexertl/state_machine.hpp>
#include <lexertl/generate_cpp.hpp>

#include <fstream>
#include <iostream>

int main(int argc, char *argv[])
{
    if (argc!=2)
        return 1;

    lexertl::state_machine sm;

    pl::core::newLexerBuild(sm);
    sm.minimise();

    std::ofstream ofs(argv[1]);
    lexertl::table_based_cpp::generate("lookup", sm, false, ofs);

    return 0;
}
