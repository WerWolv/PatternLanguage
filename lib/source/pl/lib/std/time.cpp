#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <cmath>
#include <ctime>
#include <limits>
#include <fmt/format.h>
#include <fmt/chrono.h>

namespace pl::lib::libstd::time {

    static u128 packTMValue(const std::tm &tm, std::endian endian = std::endian::native) {
        const auto packField = [](int value, u8 bits) {
            return u128(value) & hlp::bitmask(bits);
        };
        const auto year = hlp::changeEndianess(u16(tm.tm_year), endian);
        const auto yearDay = hlp::changeEndianess(u16(tm.tm_yday), endian);
        u128 result =
            (packField(tm.tm_sec,   8) << 0)  |
            (packField(tm.tm_min,   8) << 8)  |
            (packField(tm.tm_hour,  8) << 16) |
            (packField(tm.tm_mday,  8) << 24) |
            (packField(tm.tm_mon,   8) << 32) |
            (u128(year)                    << 40) |
            (packField(tm.tm_wday,  8) << 56) |
            (u128(yearDay)                 << 64) |
            (packField(tm.tm_isdst, 8) << 80);

        return hlp::changeEndianess(result, 16, endian);
    }

    static tm unpackTMValue(u128 value, std::endian endian = std::endian::native) {
        value = hlp::changeEndianess(value, 16, endian);
        tm tm = { };
        tm.tm_sec   = int(value >> 0)  & 0xFF;
        tm.tm_min   = int(value >> 8)  & 0xFF;
        tm.tm_hour  = int(value >> 16) & 0xFF;
        tm.tm_mday  = int(value >> 24) & 0xFF;
        tm.tm_mon   = int(value >> 32) & 0xFF;
        tm.tm_year  = i16(hlp::changeEndianess(u16(value >> 40), endian));
        tm.tm_wday  = int(value >> 56) & 0xFF;
        tm.tm_yday  = hlp::changeEndianess(u16(value >> 64), endian);
        tm.tm_isdst = i8(u8(value >> 80));

        return tm;
    }

    static std::optional<std::time_t> getTimeT(const core::Token::Literal &literal) {
        if (literal.isFloatingPoint()) {
            const auto value = literal.toFloatingPoint();
            if (!std::isfinite(value))
                return std::nullopt;

            const auto extendedValue = static_cast<long double>(value);
            if (extendedValue < static_cast<long double>(std::numeric_limits<std::time_t>::min()) ||
                extendedValue > static_cast<long double>(std::numeric_limits<std::time_t>::max()))
                return std::nullopt;

            return static_cast<std::time_t>(value);
        }

        if (literal.isUnsigned()) {
            const auto value = literal.toUnsigned();
            if (value > u128(std::numeric_limits<std::time_t>::max()))
                return std::nullopt;

            return static_cast<std::time_t>(value);
        }

        const auto value = literal.toSigned();
        if constexpr (std::numeric_limits<std::time_t>::is_signed) {
            if (value < i128(std::numeric_limits<std::time_t>::min()) ||
                value > i128(std::numeric_limits<std::time_t>::max()))
                return std::nullopt;
        } else if (value < 0 || u128(value) > u128(std::numeric_limits<std::time_t>::max())) {
            return std::nullopt;
        }

        return static_cast<std::time_t>(value);
    }

    static core::Token::Literal fromTimeT(std::time_t time) {
        if constexpr (std::numeric_limits<std::time_t>::is_signed)
            return i128(time);
        else
            return u128(time);
    }

    static std::optional<std::tm> getLocalTime(std::time_t time) {
        std::tm result = { };

        #ifdef _MSC_VER
            if (localtime_s(&result, &time) != 0)
                return std::nullopt;
        #elif defined(OS_LINUX) || defined(OS_MACOS)
            if (localtime_r(&time, &result) == nullptr)
                return std::nullopt;
        #else
            const auto localTime = std::localtime(&time);
            if (localTime == nullptr)
                return std::nullopt;
            result = *localTime;
        #endif

        return result;
    }

    static std::optional<std::tm> getUtcTime(std::time_t time) {
        std::tm result = { };

        #ifdef _MSC_VER
            if (gmtime_s(&result, &time) != 0)
                return std::nullopt;
        #elif defined(OS_LINUX) || defined(OS_MACOS)
            if (gmtime_r(&time, &result) == nullptr)
                return std::nullopt;
        #else
            const auto utcTime = std::gmtime(&time);
            if (utcTime == nullptr)
                return std::nullopt;
            result = *utcTime;
        #endif

        return result;
    }

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdTime = { "builtin", "std", "time" };
        {
            /* epoch() */
            runtime.addFunction(nsStdTime, "epoch", FunctionParameterCount::exactly(0), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                wolv::util::unused(params);

                return { fromTimeT(std::time(nullptr)) };
            });

            /* to_local(time) */
            runtime.addFunction(nsStdTime, "to_local", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto time = getTimeT(params[0]);
                if (!time.has_value())
                    return u128(0);

                const auto localTime = getLocalTime(*time);
                if (!localTime.has_value())
                    return u128(0);

                return { packTMValue(*localTime, ctx->getDefaultEndian()) };
            });

            /* to_utc(time) */
            runtime.addFunction(nsStdTime, "to_utc", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                const auto time = getTimeT(params[0]);
                if (!time.has_value())
                    return u128(0);

                const auto utcTime = getUtcTime(*time);
                if (!utcTime.has_value())
                    return u128(0);

                return { packTMValue(*utcTime, ctx->getDefaultEndian()) };
            });

            /* to_epoch(structured_time) */
            runtime.addFunction(nsStdTime, "to_epoch", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                u128 structuredTime = params[0].toUnsigned();

                tm time = unpackTMValue(structuredTime, ctx->getDefaultEndian());

                return { fromTimeT(std::mktime(&time)) };
            });

            /* format(format_string, structured_time) */
            runtime.addFunction(nsStdTime, "format", FunctionParameterCount::exactly(2), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto formatString = params[0].toString(false);
                u128 structuredTime = params[1].toUnsigned();

                auto time = unpackTMValue(structuredTime, ctx->getDefaultEndian());

                if (time.tm_sec  < 0 || time.tm_sec  > 61 ||
                    time.tm_min  < 0 || time.tm_min  > 59 ||
                    time.tm_hour < 0 || time.tm_hour > 23 ||
                    time.tm_mday < 1 || time.tm_mday > 31 ||
                    time.tm_mon  < 0 || time.tm_mon  > 11 ||
                    time.tm_wday < 0 || time.tm_wday > 6  ||
                    time.tm_yday < 0 || time.tm_yday > 365 ||
                    time.tm_isdst < -1 || time.tm_isdst > 1)
                    return std::string("Invalid");

                try {
                    return { fmt::format(fmt::runtime(fmt::format("{{:{}}}", formatString)), time) };
                } catch (const fmt::format_error&) {
                    return std::string("Invalid");
                }
            });
        }
    }

}
