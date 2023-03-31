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

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdRandom = { "builtin", "std", "random" };
        {

            static std::random_device randomDevice;
            static std::mt19937_64 random(randomDevice());

            random.seed(std::chrono::system_clock::now().time_since_epoch().count());

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
                        return std::uniform_int_distribution<i128>(params[1].toUnsigned(), params[2].toUnsigned())(random);
                    case Normal:
                        return std::normal_distribution<double>(params[1].toFloatingPoint(), params[2].toFloatingPoint())(random);
                    case Exponential:
                        return std::exponential_distribution<double>(params[1].toFloatingPoint())(random);
                    case Gamma:
                        return std::gamma_distribution<double>(params[1].toFloatingPoint(), params[2].toFloatingPoint())(random);
                    case Weibull:
                        return std::weibull_distribution<double>(params[1].toFloatingPoint(), params[2].toFloatingPoint())(random);
                    case ExtremeValue:
                        return std::extreme_value_distribution<double>(params[1].toFloatingPoint(), params[2].toFloatingPoint())(random);
                    case ChiSquared:
                        return std::chi_squared_distribution<double>(params[1].toFloatingPoint())(random);
                    case Cauchy:
                        return std::cauchy_distribution<double>(params[1].toFloatingPoint(), params[2].toFloatingPoint())(random);
                    case FisherF:
                        return std::fisher_f_distribution<double>(params[1].toFloatingPoint(), params[2].toFloatingPoint())(random);
                    case StudentT:
                        return std::student_t_distribution<double>(params[1].toFloatingPoint())(random);
                    case LogNormal:
                        return std::lognormal_distribution<double>(params[1].toFloatingPoint(), params[2].toFloatingPoint())(random);
                    case Bernoulli:
                        return std::bernoulli_distribution(params[1].toFloatingPoint())(random);
                    case Binomial:
                        return std::binomial_distribution<i128>(params[1].toUnsigned(), params[2].toFloatingPoint())(random);
                    case NegativeBinomial:
                        return std::negative_binomial_distribution<i128>(params[1].toUnsigned(), params[2].toFloatingPoint())(random);
                    case Geometric:
                        return std::geometric_distribution<i128>(params[1].toFloatingPoint())(random);
                    case Poisson:
                        return std::poisson_distribution<i128>(params[1].toFloatingPoint())(random);
                    default:
                        err::E0003.throwError("Invalid random type");
                }
            });

        }
    }

}