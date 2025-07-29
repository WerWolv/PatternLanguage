#include <pl/helpers/sort_checks.hpp>
#include <iostream>

using std::cout;
using std::endl;

void sortPredicateError(const char *pMsg) {
    cout << pMsg << endl;
}

void transitivityError(const char *pMsg, size_t b_idx, size_t e_idx, size_t x_idx, size_t y_idx) {
    cout << pMsg << endl
         << "   Run: " << "b_idx=" << b_idx << ", e_idx=" << e_idx << endl
         << "   Error: " << "x_idx=" << x_idx << ", y_idx=" << y_idx << endl;
}
