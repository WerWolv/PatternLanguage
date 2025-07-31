#include <pl/helpers/sort_checks.hpp>

#ifdef ENABLE_STD_SORT_CHECKS
#include <cstddef>
#include <iostream>

using std::size_t;
using std::cout;
using std::endl;

namespace pl::hlp {

void sortPredicateError(const char *pMsg) {
    cout << pMsg << endl;
}

void transitivityError(const char *pMsg, size_t b_idx, size_t e_idx, size_t x_idx, size_t y_idx) {
    cout << pMsg << endl
         << "   Run: " << "b_idx=" << b_idx << ", e_idx=" << e_idx << endl
         << "   Error: " << "x_idx=" << x_idx << ", y_idx=" << y_idx << endl;
}

} // namespace pl::hlp

#endif // ENABLE_STD_SORT_CHECKS
