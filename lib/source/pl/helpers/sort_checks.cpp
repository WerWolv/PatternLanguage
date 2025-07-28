#include <pl/helpers/sort_checks.hpp>
#include <iostream>

void sortPredicateError(size_t l_idx, size_t r_idx, const char *pMsg) {
    (void)l_idx; (void)r_idx;
    std::cout << pMsg << std::endl;
}
