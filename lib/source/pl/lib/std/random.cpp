#include <numeric>

#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>
#include <pl/lib/std/types.hpp>

#include <pl/helpers/buffered_reader.hpp>

#include <random>

namespace pl::lib::libstd::random {

    enum class RandomType {
        Uniform = 0,
        Normal = 1,
        Exponential = 2,
        Gamma = 3,
        Weibull = 4,
        ExtremeValue = 5,
        ChiSquared = 6,
        Cauchy = 7,
        FisherF = 8,
        StudentT = 9,
        LogNormal = 10,
        Bernoulli = 11,
        Binomial = 12,
        NegativeBinomial = 13,
        Geometric = 14,
        Poisson = 15
    };

    static std::random_device randomDevice;
    static std::mt19937_64 random(randomDevice());

    template<typename T>
    constexpr static auto generateNumber(auto && ... params) {
        auto dist = T(params...);
        return dist(random);
    }

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdRandom = { "builtin", "std", "random" };
        {
            const auto currentTime = std::chrono::system_clock::now();
            const auto epoch = currentTime.time_since_epoch();
            random.seed(epoch.count());

            /* set_seed(seed) */
            runtime.addFunction(nsStdRandom, "set_seed", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                random.seed(params[0].toUnsigned());
                return {};
            });

            /* random(type, param1, param2) */
            runtime.addFunction(nsStdRandom, "generate", FunctionParameterCount::exactly(3), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto type = RandomType(params[0].toUnsigned());

                switch (type) {
                    using enum RandomType;
                    case Uniform:
                        return generateNumber<std::uniform_int_distribution<i128>>(params[1].toUnsigned(), params[2].toUnsigned());
                    case Normal:
                        return generateNumber<std::normal_distribution<double>>(params[1].toFloatingPoint(), params[2].toFloatingPoint());
                    case Exponential:
                        return generateNumber<std::exponential_distribution<double>>(params[1].toFloatingPoint());
                    case Gamma:
                        return generateNumber<std::gamma_distribution<double>>(params[1].toFloatingPoint(), params[2].toFloatingPoint());
                    case Weibull:
                        return generateNumber<std::weibull_distribution<double>>(params[1].toFloatingPoint(), params[2].toFloatingPoint());
                    case ExtremeValue:
                        return generateNumber<std::extreme_value_distribution<double>>(params[1].toFloatingPoint(), params[2].toFloatingPoint());
                    case ChiSquared:
                        return generateNumber<std::chi_squared_distribution<double>>(params[1].toFloatingPoint());
                    case Cauchy:
                        return generateNumber<std::cauchy_distribution<double>>(params[1].toFloatingPoint(), params[2].toFloatingPoint());
                    case FisherF:
                        return generateNumber<std::fisher_f_distribution<double>>(params[1].toFloatingPoint(), params[2].toFloatingPoint());
                    case StudentT:
                        return generateNumber<std::student_t_distribution<double>>(params[1].toFloatingPoint());
                    case LogNormal:
                        return generateNumber<std::lognormal_distribution<double>>(params[1].toFloatingPoint(), params[2].toFloatingPoint());
                    case Bernoulli:
                        return generateNumber<std::bernoulli_distribution>(params[1].toFloatingPoint());
                    case Binomial:
                        return generateNumber<std::binomial_distribution<i128>>(params[1].toUnsigned(), params[2].toFloatingPoint());
                    case NegativeBinomial:
                        return generateNumber<std::negative_binomial_distribution<i128>>(params[1].toUnsigned(), params[2].toFloatingPoint());
                    case Geometric:
                        return generateNumber<std::geometric_distribution<i128>>(params[1].toFloatingPoint());
                    case Poisson:
                        return generateNumber<std::poisson_distribution<i128>>(params[1].toFloatingPoint());
                    default:
                        err::E0003.throwError("Invalid random type");
                }
            });

        }
    }

}