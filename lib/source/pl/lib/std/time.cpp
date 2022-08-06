#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <ctime>
#include <fmt/format.h>
#include <fmt/chrono.h>

namespace pl::lib::libstd::time {

    static u128 packTMValue(tm *tm) {
        return
            (u128(tm->tm_sec)   << 0)  |
            (u128(tm->tm_min)   << 8)  |
            (u128(tm->tm_hour)  << 16) |
            (u128(tm->tm_mday)  << 24) |
            (u128(tm->tm_mon)   << 32) |
            (u128(tm->tm_year)  << 40) |
            (u128(tm->tm_wday)  << 56) |
            (u128(tm->tm_yday)  << 64) |
            (u128(tm->tm_isdst) << 80);
    }

    static tm unpackTMValue(u128 value) {
        tm tm = { 0 };
        tm.tm_sec   = (int)(value >> 0)  & 0xFF;
        tm.tm_min   = (int)(value >> 8)  & 0xFF;
        tm.tm_hour  = (int)(value >> 16) & 0xFF;
        tm.tm_mday  = (int)(value >> 24) & 0xFF;
        tm.tm_mon   = (int)(value >> 32) & 0xFF;
        tm.tm_year  = (int)(value >> 40) & 0xFFFF;
        tm.tm_wday  = (int)(value >> 56) & 0xFF;
        tm.tm_yday  = (int)(value >> 64) & 0xFFFF;
        tm.tm_isdst = (int)(value >> 80) & 0xFF;

        return tm;
    }

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdTime = { "builtin", "std", "time" };
        {
            /* epoch() */
            runtime.addFunction(nsStdTime, "epoch", FunctionParameterCount::exactly(0), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return { std::time(nullptr) };
            });

            /* to_local(time) */
            runtime.addFunction(nsStdTime, "to_local", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                time_t time = Token::literalToUnsigned(params[0]);
                auto localTime = std::localtime(&time);

                return {packTMValue(localTime) };
            });

            /* to_utc(time) */
            runtime.addFunction(nsStdTime, "to_utc", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                time_t time = Token::literalToUnsigned(params[0]);
                auto gmTime = std::gmtime(&time);

                return {packTMValue(gmTime) };
            });

            /* to_epoch(structured_time) */
            runtime.addFunction(nsStdTime, "to_epoch", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                u128 structuredTime = Token::literalToUnsigned(params[0]);

                tm time = unpackTMValue(structuredTime);

                return { u128(std::mktime(&time)) };
            });

            /* format(format_string, structured_time) */
            runtime.addFunction(nsStdTime, "format", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto formatString = Token::literalToString(params[0], false);
                u128 structuredTime = Token::literalToUnsigned(params[1]);

                tm time = unpackTMValue(structuredTime);
                return { fmt::format(fmt::runtime(fmt::format("{{:{}}}", formatString)), unpackTMValue(structuredTime)) };
            });
        }
    }

}