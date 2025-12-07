#pragma once

#include <string>
#include <vector>

#include <pl/patterns/pattern.hpp>

#define TEST(name) (pl::test::TestPattern *)new pl::test::TestPattern##name(&s_evaluator)

namespace pl::test {

    using namespace pl::ptrn;

    enum class Mode
    {
        Succeeding,
        Failing
    };

    class TestPattern {
    public:
        explicit TestPattern(core::Evaluator *evaluator, const std::string &name, Mode mode = Mode::Succeeding) : m_evaluator(evaluator), m_mode(mode) {
            TestPattern::s_tests.insert({ name, this });
        }

        PatternLanguage *m_runtime;

        virtual void setup() {};

        virtual ~TestPattern() = default;

        template<typename T>
        std::shared_ptr<T> create(const std::string &typeName, const std::string &varName, auto... args) {
            auto pattern = std::make_shared<T>(m_evaluator, args...);
            pattern->setTypeName(typeName);
            pattern->setVariableName(varName);

            return pattern;
        }

        [[nodiscard]] virtual std::string getSourceCode() const = 0;

        [[nodiscard]] virtual const std::vector<std::shared_ptr<ptrn::Pattern>> &getPatterns() const final { return this->m_patterns; }
        virtual void addPattern(std::shared_ptr<ptrn::Pattern> &&pattern) final {
            this->m_patterns.push_back(std::move(pattern));
        }

        [[nodiscard]] Mode getMode() {
            return this->m_mode;
        }

        [[nodiscard]] static auto &getTests() {
            return TestPattern::s_tests;
        }

        [[nodiscard]] virtual bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const {
            wolv::util::unused(patterns);

            return true;
        }

        [[nodiscard]] virtual size_t repeatTimes() const {
            return 1;
        }

    private:
        std::vector<std::shared_ptr<ptrn::Pattern>> m_patterns;
        core::Evaluator *m_evaluator;
        Mode m_mode;

        static inline std::map<std::string, TestPattern *> s_tests;
    };

}