#pragma once


#define PL_TOKEN_CONCAT_IMPL(x, y)    x##y
#define PL_TOKEN_CONCAT(x, y)         PL_TOKEN_CONCAT_IMPL(x, y)
#define PL_ANONYMOUS_VARIABLE(prefix) PL_TOKEN_CONCAT(prefix, __COUNTER__)

namespace pl::scope_guard {

    #define PL_SCOPE_GUARD   ::pl::scope_guard::ScopeGuardOnExit() + [&]()
    #define PL_ON_SCOPE_EXIT [[maybe_unused]] auto PL_ANONYMOUS_VARIABLE(SCOPE_EXIT_) = PL_SCOPE_GUARD

    template<class F>
    class ScopeGuard {
    private:
        F m_func;
        bool m_active;

    public:
        explicit constexpr ScopeGuard(F func) : m_func(std::move(func)), m_active(true) { }
        ~ScopeGuard() {
            if (this->m_active) { this->m_func(); }
        }
        void release() { this->m_active = false; }

        ScopeGuard(ScopeGuard &&other) noexcept : m_func(std::move(other.m_func)), m_active(other.m_active) {
            other.cancel();
        }

        ScopeGuard &operator=(ScopeGuard &&) = delete;
    };

    enum class ScopeGuardOnExit { };

    template<typename F>
    constexpr ScopeGuard<F> operator+(ScopeGuardOnExit, F &&f) {
        return ScopeGuard<F>(std::forward<F>(f));
    }
}