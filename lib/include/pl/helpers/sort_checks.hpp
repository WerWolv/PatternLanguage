#pragma once

#define ENABLE_STD_SORT_CHECKS

#ifdef ENABLE_STD_SORT_CHECKS
#include <cstddef>
#endif

#include <algorithm>

namespace  pl::hlp {

#ifdef ENABLE_STD_SORT_CHECKS

void sortPredicateError(const char *pMsg);
void transitivityError(const char *pMsg, std::size_t b_idx, std::size_t e_idx, std::size_t x_idx, std::size_t y_idx);

// Logical implication
inline bool imp(bool l, bool r) {
    return !(l && !r);
}

template <typename Predicate>
auto checked_pedicate(const Predicate pred) {
    // IDIOT: The lambda below can't know the location in the collection!
    return [=](const auto &l, const auto &r) {
        // Irreflexivity: !(x<x)
        if (pred(l,l))
            sortPredicateError("Irreflexivity: pred(l,l) is false");
        if (pred(r,r))
            sortPredicateError("Irreflexivity: pred(r,r) is false");

        // Asymmetry: if l<r not r<l
        if (!imp(pred(l,r), !pred(r,l)))
            sortPredicateError("Asymmetry: if pred(l,r) then !pred(r,l) is false");
        if (!imp(pred(r,l), !pred(l,r)))
            sortPredicateError("Asymmetry: if pred(r,l) then !pred(l,r) is false");

        return pred(l, r);
    };
}

namespace impl {

// Transitivity:
// For all x,y.z in [b, e)
//  if pred(x,y) and pred(y,z) are true then pred(x,z) is true
//
// All elements in [b, e) should have already been sorted so
// b[n]<b[n+1]. We'll assume this even though with a dogy predicate
// it may not be the case.
template <typename Iter, typename Predicate>
void transitivity(const Iter cb, const Iter b, const Iter e, const Predicate pred)
{
    for (Iter l=b; l<e-2; ++l) {
        for (Iter r=l+2; r<e; ++r) {
            if (!pred(*l, *r)) {
                transitivityError("Transitivity", b-cb, e-cb-1, l-cb, r-cb);
                // For all x (at index n) in [b, e-1): 
                //  pred(b[n], b[n+1]) == true
                // pred(*l, *r) returned false however. This is in violation
                // of a strict weak ordering.
            }
        }
    }
}

// For all x,y.z in [b, e)
//  if !pred(x,y) && !pred(y,x) && !pred(y,z) && !pred(z,y)
//  then
//  !pred(x,z) && !pred(z,x)
//
// Incomparability is perhaps better undersood as equality.
template <typename Iter, typename Predicate>
void transitivity_of_incomparability(const Iter cb, const Iter b, const Iter e, const Predicate pred)
{
    for (Iter l=b; l<e-2; ++l) {
        for (Iter r=l+2; r<e; ++r) {
            if (!(!pred(*l, *r) && !pred(*r, *l))) {
                transitivityError("Transitivity of incomparability", b-cb, e-cb-1, l-cb, r-cb);
                // For all x (at index n) in [b, e-1): 
                //  pred(b[n], b[n+1]) == false && pred(b[n+1], b[n]) == false
                // (!pred(*l, *r) && !pred(*r, *l)) returned false however. This is in violation
                // of a strict weak ordering.
            }
        }
    }
}

} // namespace impl

template <typename Iter, typename Predicate>
void post_sort_check(const Iter b, const Iter e, const Predicate pred) {
    if (b==e)
        return;

    for (Iter l=b; l<e-1;) {
        if (pred(*l, *(l+1))) {
            Iter r = l+1;
            for (; r<e-1 && pred(*r, *(r+1)); ++r) {}
            impl::transitivity(b, l, r+1, pred);
            l = r;
        } else if (!pred(*(l+1), *l)) {
            Iter r = l+1;
            for (; r<e-1 && !pred(*(r+1), *r) && !pred(*r, *(r+1)); ++r) {}
            impl::transitivity_of_incomparability(b, l, r+1, pred);
            l = r;
        }
        else {
            // !pred(*l, *(l+1)) && pred(*(l+1), *l)
            // So l>=r && l>r
            sortPredicateError("Not sorted"); // NOT sorted!
            ++l;
        }
    }
}

#endif // ENABLE_STD_SORT_CHECKS

template<typename RandomIt, typename Compare>
void checked_sort(RandomIt first, RandomIt last, Compare comp) {
#ifdef ENABLE_STD_SORT_CHECKS
    std::sort(first, last, checked_pedicate(comp));
    post_sort_check(first, last, comp);
#else
    std::sort(first, last, comp);
#endif
}

template<typename RandomIt, typename Compare>
void checked_stable_sort(RandomIt first, RandomIt last, Compare comp) {
#ifdef ENABLE_STD_SORT_CHECKS
    std::stable_sort(first, last, checked_pedicate(comp));
    post_sort_check(first, last, comp);
#else
    std::stable_sort(first, last, comp);
#endif
}

} // namespace pl::hlp
