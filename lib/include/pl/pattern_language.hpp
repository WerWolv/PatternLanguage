#pragma once

#include <atomic>
#include <bit>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <set>
#include <thread>

#include <pl/api.hpp>

#include <pl/core/log_console.hpp>
#include <pl/core/token.hpp>
#include <pl/core/errors/error.hpp>
#include <pl/core/resolver.hpp>
#include <pl/core/resolvers.hpp>
#include <pl/core/parser_manager.hpp>

#include <pl/helpers/types.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/container/interval_tree.hpp>

namespace pl {

    namespace core {
        class Preprocessor;
        class Lexer;
        class Parser;
        class Validator;
        class Evaluator;

        namespace ast { class ASTNode; }
    }

    namespace ptrn {
        class Pattern;
        class IIterable;
    }

    /**
     * @brief This is the main entry point for the Pattern Language
     * @note The runtime can be reused for multiple executions, but if you want to execute multiple files at once, you should create a new runtime for each file
     * @note Things like the abort function and getter functions to check if the runtime is currently executing code are thread safe. However, the runtime is not thread safe in general
     */
    class PatternLanguage {
    public:

        /**
         * @brief Construct a new Pattern Language object
         * @param addLibStd Whether to add the standard library functions to the language
         */
        explicit PatternLanguage(bool addLibStd = true);
        ~PatternLanguage();

        PatternLanguage(const PatternLanguage&) = delete;
        PatternLanguage(PatternLanguage &&other) noexcept;

        struct Internals {
            std::unique_ptr<core::Preprocessor> preprocessor;
            std::unique_ptr<core::Lexer>        lexer;
            std::unique_ptr<core::Parser>       parser;
            std::unique_ptr<core::Validator>    validator;
            std::unique_ptr<core::Evaluator>    evaluator;
        };

        /**
        * @brief Lexes and preprocesses a pattern language code string and returns a token stream
        * @param code Code to preprocess
        * @param source Source of the code
        * @return token stream
        */
        [[nodiscard]] std::optional<std::vector<pl::core::Token>> preprocessString(const std::string &code, const std::string &source);


        /**
         * @brief Parses a pattern language code string and returns the generated AST
         *   To get parsing errors, check PatternLanguage#getCompileErrors() after calling this method
         * @param code Code to parse
         * @param source Source of the code
         * @return Generated AST
         */
        [[nodiscard]] std::optional<std::vector<std::shared_ptr<core::ast::ASTNode>>> parseString(const std::string &code, const std::string &source);

        /**
         * @brief Executes a pattern language code string
         * @param code Code to execute
         * @param envVars List of environment variables to set
         * @param inVariables List of input variables
         * @param checkResult Whether to check the result of the execution
         * @return True if the execution was successful, false otherwise. Call PatternLanguage#getCompileErrors() AND PatternLanguage#getEvalError() to get the compilation or runtime errors if false is returned
         */
        [[nodiscard]] bool executeString(const std::string &code, const std::string &source = api::Source::DefaultSource, const std::map<std::string, core::Token::Literal> &envVars = {}, const std::map<std::string, core::Token::Literal> &inVariables = {}, bool checkResult = true);

        /**
         * @brief Executes a pattern language file
         * @param path Path to the file to execute
         * @param envVars List of environment variables to set
         * @param inVariables List of input variables
         * @param checkResult Whether to check the result of the execution
         * @return True if the execution was successful, false otherwise
         */
        [[nodiscard]] bool executeFile(const std::filesystem::path &path, const std::map<std::string, core::Token::Literal> &envVars = {}, const std::map<std::string, core::Token::Literal> &inVariables = {}, bool checkResult = true);

        /**
         * @brief Executes code as if it was run inside of a function
         * @param code Code to execute
         * @return Pair of a boolean indicating whether the execution was successful and an optional literal containing the return value
         */
        [[nodiscard]] std::pair<bool, std::optional<core::Token::Literal>> executeFunction(const std::string &code);

        /**
         * @brief Adds a virtual source file under the path
         * @param code the code of the source
         * @param source the source of the code
         * @return the source that was added or that already existed
         */
        [[nodiscard]] api::Source* addVirtualSource(const std::string& code, const std::string& source, bool mainSource = false) const;

        /**
         * @brief Aborts the currently running execution asynchronously
        */
        void abort();

        /**
         * @brief Sets the data source for the pattern language
         * @param baseAddress Base address of the data source
         * @param size Size of the data source
         * @param readFunction Function to read data from the data source
         * @param writerFunction Optional function to write data to the data source
         */
        void setDataSource(u64 baseAddress, u64 size, std::function<void(u64, u8*, size_t)> readFunction, std::optional<std::function<void(u64, const u8*, size_t)>> writerFunction = std::nullopt);

        /**
         * @brief Sets the base address of the data source
         * @param baseAddress Base address of the data source
         */
        void setDataBaseAddress(u64 baseAddress);

        /**
         * @brief Sets the size of the data source
         * @param size Size of the data source
         */
        void setDataSize(u64 size);

        /**
         * @brief Sets the default endianess used in the pattern language
         * @param endian Endianess to use
         */
        void setDefaultEndian(std::endian endian);

        /**
         * @brief Sets the initial cursor position used at the start of  execution
         * @param address Initial cursor position
         */
        void setStartAddress(u64 address);
        u64 getStartAddress() const;


        /**
         * @brief Adds a new pragma preprocessor instruction
         * @param name Name of the pragma
         * @param callback Callback to execute when the pragma is encountered
         */
        void addPragma(const std::string &name, const api::PragmaHandler &callback);

        /**
         * @brief Removes a pragma preprocessor instruction
         * @param name Name of the pragma
         */
        void removePragma(const std::string &name);

        /**
         * @brief Adds a define to the preprocessor
         * @param name Name of the define
         * @param value Value of the define
         */
        void addDefine(const std::string &name, const std::string &value = "");

        /**
         * @brief Removes a define from the preprocessor
         * @param name Name of the define
         */
        void removeDefine(const std::string &name);

        /**
         * @brief Sets the include paths for where to look for include files
         * @param paths List of paths to look in
         */
        void setIncludePaths(const std::vector<std::fs::path>& paths);

        /**
         * @brief Sets the source resolver of the pattern language
         * @param resolver Resolver to use
         */
        void setResolver(const core::Resolver& resolver);

        /**
         * @brief Registers a callback to be called when a dangerous function is being executed
         * @note The callback should return true if the function should be executed, false otherwise
         * @note Returning false will cause the execution to abort
         * @note If the callback is not set, dangerous functions are disabled
         * @param callback Callback to call
         */
        void setDangerousFunctionCallHandler(std::function<bool()> callback);

        /**
         * @brief Sets the console log callback
         * @param callback Callback to call
         */
        void setLogCallback(const core::LogConsole::Callback &callback);

        /**
         * @brief Gets the potential error that occurred during the last evaluation of the compiled AST. This does NOT include compilation errors.
         * @return Error if one happened
         */
        [[nodiscard]] const std::optional<core::err::PatternLanguageError> &getEvalError() const;

        /**
         * @brief Gets the errors that occurred during the last compilation (e.g. parseString())
         * @return A vector of errors (can be empty if no errors occurred)
         */
        [[nodiscard]] const std::vector<core::err::CompileError>& getCompileErrors() const;

        /**
         * @brief Gets a map of all out variables and their values that have been defined in the last execution
         * @return Out variables
         */
        [[nodiscard]] std::map<std::string, core::Token::Literal> getOutVariables() const;

        /**
         * @brief Gets the number of created patterns during the last execution
         * @return Number of patterns
         */
        [[nodiscard]] u64 getCreatedPatternCount() const;

        /**
         * @brief Gets the maximum number of patterns allowed to be created
         * @return Maximum number of patterns
         */
        [[nodiscard]] u64 getMaximumPatternCount() const;

        /**
         * @brief Gets the memory of a custom section that was created
         * @param id ID of the section
         * @return Memory of the section
         */
        [[nodiscard]] const std::vector<u8>& getSection(u64 id) const;

        /**
         * @brief Gets all custom sections that were created
         * @return Custom sections
         */
        [[nodiscard]] const std::map<u64, api::Section>& getSections() const;

        /**
         * @brief Gets all patterns that were created in the given section
         * @param section Section Id
         * @return All patterns in the given section
         */
        [[nodiscard]] const std::vector<std::shared_ptr<ptrn::Pattern>> &getPatterns(u64 section = 0x00) const;

        /**
         * @brief Gets all patterns that overlap with the given address
         * @param address Address to check
         * @param section Section id
         * @return Patterns
         */
        [[nodiscard]] std::vector<ptrn::Pattern *> getPatternsAtAddress(u64 address, u64 section = 0x00) const;


        /**
         * @brief Get the colors of all patterns that overlap with the given address
         * @param address Address to check
         * @param section Section id
         * @return Patterns
         */
        [[nodiscard]] std::vector<u32> getColorsAtAddress(u64 address, u64 section = 0x00) const;

        /**
         * @brief Resets the runtime
         */
        void reset();

        /**
         * @brief Checks whether the runtime is currently running
         * @return True if the runtime is running, false otherwise
         */
        [[nodiscard]] bool isRunning() const {
            return this->m_running;
        }

        /**
         * @brief Gets the time the last execution took
         * @return Time the last execution took
         */
        [[nodiscard]] double getLastRunningTime() const {
            return this->m_runningTime;
        }

        /**
         * @brief Adds a new built-in function to the pattern language
         * @param ns Namespace of the function
         * @param name Name of the function
         * @param parameterCount Number of parameters the function takes
         * @param func Callback to execute when the function is called
         */
        void addFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func);

        /**
         * @brief Adds a new dangerous built-in function to the pattern language
         * @param ns Namespace of the function
         * @param name Name of the function
         * @param parameterCount Number of parameters the function takes
         * @param func Callback to execute when the function is called
         */
        void addDangerousFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func);

        /**
         * @brief Adds a new custom built-in type to the pattern language
         * @param ns Namespace of the type
         * @param name Name of the type
         * @param parameterCount Number of non-type template parameters the type takes
         * @param func Callback to execute when the type is instantiated
         */
        void addType(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::TypeCallback &func);

        /**
         * @brief Gets the internals of the pattern language
         * @warning Generally this should only be used by "IDEs" or other tools that need to access the internals of the pattern language
         * @return Internals
         */
        [[nodiscard]] const Internals& getInternals() const {
            return this->m_internals;
        }

        [[nodiscard]] const std::map<std::string, std::string>& getDefines() const {
            return this->m_defines;
        }

        [[nodiscard]] const std::vector<std::shared_ptr<core::ast::ASTNode>> getAST() const {
            return this->m_currAST;
        }

        [[nodiscard]] const std::map<std::string, api::PragmaHandler>& getPragmas() const {
            return this->m_pragmas;
        }

        /**
         * @brief Gets the source resolver of the pattern language
         * @return Mutable reference to the Resolver
         */
        [[nodiscard]] core::Resolver& getResolver() {
            return this->m_resolvers;
        }

        /**
         * @brief Gets the source resolver of the pattern language
         * @return Resolver
         */
        [[nodiscard]] const core::Resolver& getResolver() const {
            return this->m_resolvers;
        }

        /**
         * Adds a new cleanup callback that is called when the runtime is reset
         * @note This is useful for built-in functions that need to clean up their state
         * @param callback Callback to call
         */
        void addCleanupCallback(const std::function<void(PatternLanguage&)> &callback) {
            this->m_cleanupCallbacks.push_back(callback);
        }

        /**
         * @brief Checks whether the patterns are valid
         * @return True if the patterns are valid, false otherwise
         */
        [[nodiscard]] bool arePatternsValid() const {
            return this->m_patternsValid;
        }

        /**
         * @brief Gets the current run id
         * @return Run id
         */
        [[nodiscard]] u64 getRunId() const {
            return this->m_runId;
        }

        [[nodiscard]] const std::atomic<u64>& getLastReadAddress() const;
        [[nodiscard]] const std::atomic<u64>& getLastWriteAddress() const;
        [[nodiscard]] const std::atomic<u64>& getLastPatternPlaceAddress() const;

        PatternLanguage cloneRuntime() const;

        [[nodiscard]] bool isSubRuntime() const {
            return this->m_subRuntime;
        }

        [[nodiscard]] const std::set<pl::ptrn::Pattern*>& getPatternsWithAttribute(const std::string &attribute) const;

    private:
        void flattenPatterns();

    private:
        Internals m_internals;
        std::vector<core::err::CompileError> m_compileErrors;
        std::optional<core::err::PatternLanguageError> m_currError;
        std::map<std::string, std::string> m_defines;
        std::map<std::string, api::PragmaHandler> m_pragmas;
        bool m_subRuntime = false;

        core::Resolver m_resolvers;
        core::resolvers::FileResolver m_fileResolver;
        core::ParserManager m_parserManager;

        std::map<u64, std::vector<std::shared_ptr<ptrn::Pattern>>> m_patterns;
        std::atomic<bool> m_flattenedPatternsValid = false;
        std::map<u64, wolv::container::IntervalTree<ptrn::Pattern*, u64, 5>> m_flattenedPatterns;
        std::thread m_flattenThread;
        std::vector<std::function<void(PatternLanguage&)>> m_cleanupCallbacks;
        std::vector<std::shared_ptr<core::ast::ASTNode>> m_currAST;

        std::atomic<bool> m_running = false;
        std::atomic<bool> m_patternsValid = false;
        std::atomic<bool> m_aborted = false;
        std::atomic<u64>  m_runId = 0;

        std::optional<u64> m_startAddress;
        std::endian m_defaultEndian = std::endian::little;
        double m_runningTime = 0;

        u64 m_dataBaseAddress;
        u64 m_dataSize;
        std::function<void(u64, u8*, size_t)> m_dataReadFunction;
        std::optional<std::function<void(u64, const u8*, size_t)>> m_dataWriteFunction;

        std::function<bool()> m_dangerousFunctionCallCallback;
        core::LogConsole::Callback m_logCallback;

        struct Function {
            api::Namespace nameSpace;
            std::string name;
            api::FunctionParameterCount parameterCount;
            api::FunctionCallback callback;
            bool dangerous;
        };
        std::vector<Function> m_functions;
    };

}