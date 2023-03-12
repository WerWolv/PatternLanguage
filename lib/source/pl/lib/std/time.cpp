#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <ctime>
#include <fmt/format.h>
#include <fmt/chrono.h>

namespace pl::lib::libstd::time {

    static u128 packTMValue(std::tm tm) {
        return
            (u128(tm.tm_sec)   << 0)  |
            (u128(tm.tm_min)   << 8)  |
            (u128(tm.tm_hour)  << 16) |
            (u128(tm.tm_mday)  << 24) |
            (u128(tm.tm_mon)   << 32) |
            (u128(tm.tm_year)  << 40) |
            (u128(tm.tm_wday)  << 56) |
            (u128(tm.tm_yday)  << 64) |
            (u128(tm.tm_isdst) << 80);
    }

    static tm unpackTMValue(u128 value) {
        tm tm = { };
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
                wolv::util::unused(params);

                return { std::time(nullptr) };
            });

            /* to_local(time) */
            runtime.addFunction(nsStdTime, "to_local", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                time_t time = params[0].toUnsigned();

                try {
                    auto localTime = fmt::localtime(time);

                    return { packTMValue(localTime) };
                } catch (const fmt::format_error&) {
                    return u128(0);
                }

            });

            /* to_utc(time) */
            runtime.addFunction(nsStdTime, "to_utc", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                time_t time = params[0].toUnsigned();

                try {
                    auto gmTime = fmt::gmtime(time);

                    return { packTMValue(gmTime) };
                } catch (const fmt::format_error&) {
                    return u128(0);
                }
            });

            /* to_epoch(structured_time) */
            runtime.addFunction(nsStdTime, "to_epoch", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                u128 structuredTime = params[0].toUnsigned();

                tm time = unpackTMValue(structuredTime);

                return { u128(std::mktime(&time)) };
            });

            /* format(format_string, structured_time) */
            runtime.addFunction(nsStdTime, "format", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto formatString = params[0].toString(false);
                u128 structuredTime = params[1].toUnsigned();

                auto time = unpackTMValue(structuredTime);

                if (time.tm_sec  < 0 || time.tm_sec  > 61 ||
                    time.tm_min  < 0 || time.tm_min  > 59 ||
                    time.tm_hour < 0 || time.tm_hour > 23 ||
                    time.tm_mday < 1 || time.tm_mday > 31 ||
                    time.tm_mon  < 0 || time.tm_mon  > 11 ||
                    time.tm_wday < 0 || time.tm_wday > 6  ||
                    time.tm_yday < 0 || time.tm_yday > 365)
                    return "Invalid";

                return { fmt::format(fmt::runtime(fmt::format("{{:{}}}", formatString)), time) };
            });
        }
    }

}