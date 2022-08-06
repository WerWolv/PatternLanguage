#include <pl.hpp>

const static std::map<pl::core::LogConsole::Level, std::string> LogLevels = {
        { pl::core::LogConsole::Level::Debug,     "Debug"     },
        { pl::core::LogConsole::Level::Info,      "Info"      },
        { pl::core::LogConsole::Level::Warning,   "Warning"   },
        { pl::core::LogConsole::Level::Error,     "Error"     }
};

int main() {
    // Create an instance of the pattern language runtime
    pl::PatternLanguage patternLanguage;

    // Create some data to analyze
    constexpr static auto Data = []{
        std::array<pl::u8, 0x100> data = { };
        for (size_t i = 0; i < data.size(); i++)
            data[i] = i;

        return data;
    }();

    // Tell the runtime where and how to read data
    patternLanguage.setDataSource([](pl::u64 address, pl::u8 *buffer, size_t size){
        std::memcpy(buffer, &Data[address], size);
    }, 0x00, Data.size());

    // Tell the runtime how to handle dangerous functions being called
    patternLanguage.setDangerousFunctionCallHandler([]{
        // Print a message
        printf("Dangerous function called!\n");

        // Allow all dangerous functions to be run
        return true;
    });

    // Create a normal builtin function called `test::normal_function` that takes a single parameter
    patternLanguage.addFunction({ "test" }, "normal_function", pl::api::FunctionParameterCount::exactly(1), [](pl::core::Evaluator *ctx, const std::vector<pl::core::Token::Literal> &params) -> std::optional<pl::core::Token::Literal>{
        fmt::print("normal_function {}\n", std::get<pl::i128>(params[0]));
        return std::nullopt;
    });

    // Create a dangerous function called `test::dangerous_function` that takes no parameters
    patternLanguage.addDangerousFunction({ "test" }, "dangerous_function", pl::api::FunctionParameterCount::none(), [](pl::core::Evaluator *, const std::vector<pl::core::Token::Literal> &) -> std::optional<pl::core::Token::Literal>{
        printf("dangerous_function\n");
        return 1337;
    });

    // Evaluate some pattern language source code
    bool result = patternLanguage.executeString(R"(
        fn main() {
            s32 x = test::dangerous_function();
            test::normal_function(x);
        };
    )");

    // Check if execution completed successfully
    if (!result) {
        // If not, print the error string
        auto error = patternLanguage.getError();
        printf("Error: %d:%d %s\n", error->line, error->column, error->message.c_str());

        // Print all log messages
        for (auto [level, message] : patternLanguage.getConsoleLog()) {
            printf("[%s] %s\n", LogLevels.at(level).c_str(), message.c_str());
        }
    }

}