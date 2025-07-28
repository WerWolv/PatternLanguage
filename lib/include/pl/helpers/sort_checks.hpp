#pragma once

#include <algorithm>

void sortPredicateError(size_t l_idx, size_t r_idx, const char *pMsg);

template <typename RandomIt>
void genSortPredicateError(RandomIt collBegin, RandomIt l, RandomIt r, const char *pMsg)
{
    sortPredicateError(l-collBegin, r-collBegin, pMsg);
}

// Logical implication
inline bool imp(bool l, bool r) {
    return !(l && !r);
}

template <typename Predicate, typename RandomIt>
auto checked_pedicate(RandomIt b, Predicate pred) {
    return [=](const auto &l, const auto &r) {
        // Irreflexivity: !(x<x)
        if (pred(l,l))
            genSortPredicateError(b, l, r, "Irreflexivity: pred(l,l) is false");
        if (pred(r,r))
            genSortPredicateError(b, l, r, "Irreflexivity: pred(r,r) is false");

        // Asymmetry: if l<r not r<l
        if (!imp(pred(l,r), !pred(r,l)))
            genSortPredicateError(b, l, r, "Asymmetry: if pred(l,r) then !pred(r,l) is false");
        if (!imp(pred(r,l), !pred(l,r)))
            genSortPredicateError(b, l, r, "Asymmetry: if pred(r,l) then !pred(l,r) is false");

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
void transitivity(const Iter b, const Iter e, const Predicate pred)
{
    for (Iter l=b; l<e-2; ++l) {
        for (Iter r=l+2; r<e; ++r) {
            if (!pred(*l, *r)) {
                genSortPredicateError(b, l, r, "Transitivity");
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
void transitivity_of_incomparability(const Iter b, const Iter e, const Predicate pred)
{
    for (Iter l=b; l<e-2; ++l) {
        for (Iter r=l+2; r<e; ++r) {
            if (!(!pred(*l, *r) && !pred(*r, *l))) {
                genSortPredicateError(b, l, r, "Transitivity of incomparability");
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
            impl::transitivity(l, r+1, pred);
            l = r;
        } else if (!pred(*(l+1), *l)) {
            Iter r = l+1;
            for (; r<e-1 && !pred(*(r+1), *r) && !pred(*r, *(r+1)); ++r) {}
            impl::transitivity_of_incomparability(l, r+1, pred);
            l = r;
        }
        else {
            // !pred(*l, *(l+1)) && pred(*(l+1), *l)
            // So l>=r && l>r
            genSortPredicateError(b, l, l+1, "Not sorted"); // NOT sorted!
            ++l;
        }
    }
}

template<typename RandomIt, typename Compare>
void checked_sort(RandomIt b, RandomIt e, Compare comp) {
    std::sort(b, e, checked_pedicate(b, comp));
    post_sort_check(b, e, comp);
}

template<typename RandomIt, typename Compare>
void checked_stable_sort(RandomIt b, RandomIt e, Compare comp) {
    std::stable_sort(b, e, checked_pedicate(b, comp));
    post_sort_check(b, e, comp);
}
