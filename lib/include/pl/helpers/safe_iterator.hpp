#pragma once

#include <stdexcept>

#include <utility>

namespace pl::hlp {

    template <typename T>
    concept IteratorLike = requires(T t) {
        typename T::value_type;
        typename T::reference;
        typename T::pointer;
        typename T::difference_type;
    };

    template <IteratorLike Iter>
    class SafeIterator {
        using Type = typename Iter::value_type;
        using Reference = typename Iter::reference;
        using Pointer = typename Iter::pointer;
        using DifferenceType = typename Iter::difference_type;

    public:
        SafeIterator() = default;
        explicit SafeIterator(Iter start, Iter end) : m_start(start), m_end(end) { }
        SafeIterator(const SafeIterator& other) : m_start(other.m_start), m_end(other.m_end) { }
        SafeIterator(SafeIterator&& other) noexcept : m_start(std::move(other.m_start)), m_end(std::move(other.m_end)) { }

        SafeIterator& operator=(const SafeIterator& other) {
            m_start = other.m_start;
            m_end = other.m_end;
            return *this;
        }



        // operators
        Reference operator*() const {
            checkValid();
            return *m_start;
        }

        Pointer operator->() const {
            checkValid();
            return &(*m_start);
        }

        SafeIterator& operator++() {
            checkRange(1);
            ++m_start;
            return *this;
        }

        SafeIterator& operator--() {
            checkRange(1);
            --m_start;
            return *this;
        }

        Reference operator[](DifferenceType index) const {
            checkRange(index + 1);
            return m_start[index];
        }

        SafeIterator& operator+=(DifferenceType index) {
            checkRange(index);
            m_start += index;
            return *this;
        }

        SafeIterator& operator-=(DifferenceType index) {
            checkRange(index);
            m_start -= index;
            return *this;
        }

        SafeIterator operator+(DifferenceType index) const {
            checkRange(index);
            return SafeIterator(m_start + index, m_end);
        }

        SafeIterator operator-(DifferenceType index) const {
            checkRange(index);
            return SafeIterator(m_start - index, m_end);
        }

        bool operator==(const SafeIterator& other) const {
            return m_start == other.m_start;
        }

        bool operator>(const SafeIterator& other) const {
            return m_start > other.m_start;
        }

        // helpers
        Reference front() const {
            checkValid();
            return *m_start;
        }

        Reference back() const {
            checkValid();
            return *(m_end - 1);
        }

    private:
        Iter m_start, m_end;

        void checkRange(DifferenceType range) const {
            if(m_end - m_start < range) {
                throw std::out_of_range("iterator out of range");
            }
        }

        void checkValid() const {
            if(m_end - m_start == 0) {
                throw std::out_of_range("iterator out of range");
            }
        }
    };

}