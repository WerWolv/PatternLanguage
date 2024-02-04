#pragma once
#include <concepts>
#include <memory>
#include <stdexcept>

namespace pl::hlp {

    template<typename T>
    concept SmartPointer = requires(T t) {
            { t.get() } -> std::convertible_to<typename T::element_type*>;
    };

    template<SmartPointer T>
    class SafePointer : public T {
    public:
        using T::T;

        SafePointer(T &&t) : T(std::move(t)) { }

        using Contained = typename T::element_type;

        Contained* operator->() const {
            return this->get();
        }

        Contained* operator->() {
            return this->get();
        }

        Contained& operator*() const {
            return *this->get();
        }

        Contained& operator*() {
            return *this->get();
        }

        Contained* get() const {
            if (this->T::get() == nullptr)
                throw std::runtime_error("Pointer is null!");

            return this->T::get();
        }

        operator T&() {
            if (this->T::get() == nullptr)
                throw std::runtime_error("Pointer is null!");

            return *this;
        }

        operator T&() const {
            if (this->T::get() == nullptr)
                throw std::runtime_error("Pointer is null!");

            return *this;
        }

        operator T&&() && {
            if (this->T::get() == nullptr)
                throw std::runtime_error("Pointer is null!");

            return *this;
        }

    };

    template<typename T>
    using safe_unique_ptr = SafePointer<std::unique_ptr<T>>;

    template<typename T>
    using safe_shared_ptr = SafePointer<std::shared_ptr<T>>;
}