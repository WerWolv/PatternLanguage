#pragma once
#include <concepts>
#include <memory>
#include <stdexcept>

#include <throwing/unique_ptr.hpp>
#include <throwing/shared_ptr.hpp>

namespace pl::hlp {

    template<template<typename ...> typename SmartPointer, typename T>
    class SafePointer;

    template<typename T>
    class SafePointer<std::unique_ptr, T> : public throwing::unique_ptr<T> {
    public:
        using throwing::unique_ptr<T>::unique_ptr;

        template<typename U>
        operator std::unique_ptr<U>() && {
            checkPointer();
            return std::move(this->get_std_unique_ptr());
        }

        template<typename U>
        operator const std::unique_ptr<U>&() const {
            checkPointer();
            return this->get_std_unique_ptr();
        }

        template<typename U>
        operator std::shared_ptr<U>() && {
            checkPointer();
            return std::move(this->get_std_unique_ptr());
        }

        template<typename U>
        operator SafePointer<std::shared_ptr, U>() && {
            return std::move(this->get_std_unique_ptr());
        }

        const auto& unwrap() const {
            checkPointer();
            return this->get_std_unique_ptr();
        }

        auto& unwrap() {
            checkPointer();
            return this->get_std_unique_ptr();
        }

        auto& unwrapUnchecked() {
            return this->get_std_unique_ptr();
        }

    private:
        void checkPointer() const {
            if (this->get() == nullptr)
                throw throwing::null_ptr_exception<T>();
        }
    };

    template<typename T>
    class SafePointer<std::shared_ptr, T> : public throwing::shared_ptr<T> {
    public:
        using throwing::shared_ptr<T>::shared_ptr;

        template<typename U>
        operator const std::shared_ptr<U>&() const {
            checkPointer();
            return this->get_std_shared_ptr();
        }

        template<typename U>
        operator std::shared_ptr<U>() && {
            checkPointer();
            return std::move(this->get_std_shared_ptr());
        }

        const auto& unwrap() const {
            checkPointer();
            return this->get_std_shared_ptr();
        }

        const auto& unwrapUnchecked() const {
            return this->get_std_shared_ptr();
        }

        auto&& moveUnchecked() {
            return this->get_std_shared_ptr();
        }

    private:
        void checkPointer() const {
            if (this->get() == nullptr)
                throw throwing::null_ptr_exception<T>();
        }
    };

    template<typename T>
    using safe_unique_ptr = SafePointer<std::unique_ptr, T>;

    template<typename T>
    using safe_shared_ptr = SafePointer<std::shared_ptr, T>;
}