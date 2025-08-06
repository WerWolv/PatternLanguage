#include <pl/core/new_lexer_sm.hpp>

#include <lexertl/state_machine.hpp>
#include <lexertl/generate_cpp.hpp>

#include <fstream>

int main()
{
    lexertl::state_machine sm;

    pl::core::new_lexer_compile(sm);
    sm.minimise();

    std::ofstream ofs("lexer.hpp");
    lexertl::table_based_cpp::generate_cpp("lookup", sm, false, ofs);

    return 0;
}
