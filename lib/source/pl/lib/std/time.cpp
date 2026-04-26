#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>
#include <wolv/utils/date_time_format.hpp>

#include <ctime>
#include <fmt/format.h>
#include <fmt/chrono.h>

namespace pl::lib::libstd::time {

    const std::string s_invalid         = "Invalid";
    const std::string s_canNotFormat    = "Can not format";

    static u128 packTMValue(const std::tm &tm, pl::PatternLanguage &runtime) {
        auto endian = runtime.getInternals().evaluator->getDefaultEndian();
        std::tm tmCopy = tm;
        tmCopy.tm_year = hlp::changeEndianess(tm.tm_year, 2, endian);
        tmCopy.tm_yday = hlp::changeEndianess(tm.tm_yday, 2, endian);
        u128 result =
            (u128(tm.tm_sec)   << 0)  |
            (u128(tm.tm_min)   << 8)  |
            (u128(tm.tm_hour)  << 16) |
            (u128(tm.tm_mday)  << 24) |
            (u128(tm.tm_mon)   << 32) |
            (u128(tmCopy.tm_year)  << 40) |
            (u128(tm.tm_wday)  << 56) |
            (u128(tmCopy.tm_yday)  << 64) |
            (u128(tm.tm_isdst) << 80);

        return hlp::changeEndianess(result, 16, endian);
    }

    static tm unpackTMValue(u128 value, pl::PatternLanguage &runtime) {
        auto endian = runtime.getInternals().evaluator->getDefaultEndian();
        value = hlp::changeEndianess(value, 16, endian);
        tm tm = { };
        tm.tm_sec   = (int)(value >> 0)  & 0xFF;
        tm.tm_min   = (int)(value >> 8)  & 0xFF;
        tm.tm_hour  = (int)(value >> 16) & 0xFF;
        tm.tm_mday  = (int)(value >> 24) & 0xFF;
        tm.tm_mon   = (int)(value >> 32) & 0xFF;
        tm.tm_year  = (int)(value >> 40) & 0xFFFF;
        tm.tm_year = hlp::changeEndianess(tm.tm_year, 2, endian);
        tm.tm_wday  = (int)(value >> 56) & 0xFF;
        tm.tm_yday  = (int)(value >> 64) & 0xFFFF;
        tm.tm_yday = hlp::changeEndianess(tm.tm_yday, 2, endian);
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

                return { u128(std::time(nullptr)) };
            });

            /* to_local(time) */
            runtime.addFunction(nsStdTime, "to_local", FunctionParameterCount::exactly(1), [&runtime](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto time = time_t(params[0].toUnsigned());
                auto localTime = std::localtime(&time);

                if (localTime == nullptr) return u128(0);

                return { packTMValue(*localTime, runtime) };
            });

            /* to_utc(time) */
            runtime.addFunction(nsStdTime, "to_utc", FunctionParameterCount::exactly(1), [&runtime](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto time = time_t(params[0].toUnsigned());
                auto gmTime = std::gmtime(&time);

                if (gmTime == nullptr) return u128(0);

                return { packTMValue(*gmTime, runtime) };
            });

            /* to_epoch(structured_time) */
            runtime.addFunction(nsStdTime, "to_epoch", FunctionParameterCount::exactly(1), [&runtime](Evaluator *, auto params) -> std::optional<Token::Literal> {
                u128 structuredTime = params[0].toUnsigned();

                tm time = unpackTMValue(structuredTime, runtime);

                return { u128(std::mktime(&time)) };
            });

            /* format(format_string, structured_time) */
            runtime.addFunction(nsStdTime, "format", FunctionParameterCount::exactly(2), [&runtime](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto formatString = params[0].toString(false);
                u128 structuredTime = params[1].toUnsigned();

                auto time = unpackTMValue(structuredTime, runtime);

                if (time.tm_sec  < 0 || time.tm_sec  > 61 ||
                    time.tm_min  < 0 || time.tm_min  > 59 ||
                    time.tm_hour < 0 || time.tm_hour > 23 ||
                    time.tm_mday < 1 || time.tm_mday > 31 ||
                    time.tm_mon  < 0 || time.tm_mon  > 11 ||
                    time.tm_wday < 0 || time.tm_wday > 6  ||
                    time.tm_yday < 0 || time.tm_yday > 365)
                    return std::string("Invalid");

                return { fmt::format(fmt::runtime(fmt::format("{{:{}}}", formatString)), time) };
            });

            /* format_tt(time_t) */
            runtime.addFunction(nsStdTime, "format_tt", FunctionParameterCount::exactly(1), [&runtime](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto tt = params[0].toUnsigned();
                const wolv::util::Locale &lc = runtime.getLocale();

                using wolv::util::DTOpts;
                auto optval = wolv::util::formatTT(lc, tt, DTOpts::TT64 | DTOpts::DandT);
                if (!optval) {
                    return s_canNotFormat;
                }

                return optval;
            });

            /* format_dos_date(time_t) */
            runtime.addFunction(nsStdTime, "format_dos_date", FunctionParameterCount::exactly(1), [&runtime](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto p = params[0].toUnsigned();

                struct DOSDate {
                    u16 day   : 5;
                    u16 month : 4;
                    u16 year  : 7;
                };

                DOSDate dd;
                std::memcpy(&dd, &p, sizeof(dd));
                if ( (dd.day<1 || dd.day>31) || (dd.month<1 || dd.month>12) ) {
                    return s_invalid;
                }

                std::tm tm{};
                tm.tm_year = dd.year + 80;
                tm.tm_mon  = dd.month - 1;
                tm.tm_mday = dd.day;

#if defined(OS_WINDOWS)
                time_t tt = _mkgmtime(&tm);
#else
                time_t tt = timegm(&tm);
#endif
                if (tt == -1) {
                    return s_canNotFormat;
                }

                const wolv::util::Locale &lc = runtime.getLocale();

                using wolv::util::DTOpts;
                auto optval = wolv::util::formatTT(lc, tt, DTOpts::TT64 | DTOpts::D);
                if (!optval) {
                    return s_canNotFormat;
                }
                
                return *optval;
            });

            /* format_dos_time(time_t) */
            runtime.addFunction(nsStdTime, "format_dos_time", FunctionParameterCount::exactly(1), [&runtime](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto p = params[0].toUnsigned();

                struct DOSTime {
                    u16 seconds : 5;
                    u16 minutes : 6;
                    u16 hours   : 5;
                };

                DOSTime dt;
                std::memcpy(&dt, &p, sizeof(dt));

                if ( (dt.hours<0 || dt.hours>23)     ||
                     (dt.minutes<0 || dt.minutes>59) ||
                     (dt.seconds<0 || dt.seconds>29)  )
                {
                    return s_invalid;
                }

                time_t tt = dt.hours*60*60 + dt.minutes*60 + dt.seconds*2;

                const wolv::util::Locale &lc = runtime.getLocale();

                using wolv::util::DTOpts;
                auto optval = wolv::util::formatTT(lc, tt, DTOpts::TT64 | DTOpts::T);
                if (!optval) {
                    return s_canNotFormat;
                }
                
                return *optval;
            });
        }
    }

}