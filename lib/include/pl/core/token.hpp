#pragma once


#include <utility>
#include <string>
#include <variant>

#include <fmt/format.h>

#include <pl/core/log_console.hpp>

#include <pl/helpers/types.hpp>
#include <pl/helpers/utils.hpp>
#include <pl/helpers/concepts.hpp>

namespace pl::ptrn {
    class Pattern;
}

namespace pl::core {

    namespace ast { class ASTNode; }

    class Token {
    public:
        enum class Type : u64
        {
            Keyword,
            ValueType,
            Operator,
            Integer,
            String,
            Identifier,
            Separator
        };

        enum class Keyword
        {
            Struct,
            Union,
            Using,
            Enum,
            Bitfield,
            LittleEndian,
            BigEndian,
            If,
            Else,
            Parent,
            This,
            While,
            For,
            Function,
            Return,
            Namespace,
            In,
            Out,
            Break,
            Continue
        };

        enum class Operator
        {
            At,
            Assign,
            Colon,
            Plus,
            Minus,
            Star,
            Slash,
            Percent,
            LeftShift,
            RightShift,
            BitOr,
            BitAnd,
            BitXor,
            BitNot,
            BoolEqual,
            BoolNotEqual,
            BoolGreaterThan,
            BoolLessThan,
            BoolGreaterThanOrEqual,
            BoolLessThanOrEqual,
            BoolAnd,
            BoolOr,
            BoolXor,
            BoolNot,
            TernaryConditional,
            Dollar,
            AddressOf,
            SizeOf,
            ScopeResolution
        };

        enum class ValueType
        {
            Unsigned8Bit   = 0x10,
            Signed8Bit     = 0x11,
            Unsigned16Bit  = 0x20,
            Signed16Bit    = 0x21,
            Unsigned24Bit  = 0x30,
            Signed24Bit    = 0x31,
            Unsigned32Bit  = 0x40,
            Signed32Bit    = 0x41,
            Unsigned48Bit  = 0x60,
            Signed48Bit    = 0x61,
            Unsigned64Bit  = 0x80,
            Signed64Bit    = 0x81,
            Unsigned96Bit  = 0xC0,
            Signed96Bit    = 0xC1,
            Unsigned128Bit = 0x100,
            Signed128Bit   = 0x101,
            Character      = 0x13,
            Character16    = 0x23,
            Boolean        = 0x14,
            Float          = 0x42,
            Double         = 0x82,
            String         = 0x15,
            Auto           = 0x16,
            CustomType     = 0x00,
            Padding        = 0x1F,

            Unsigned      = 0xFF00,
            Signed        = 0xFF01,
            FloatingPoint = 0xFF02,
            Integer       = 0xFF03,
            Any           = 0xFFFF
        };

        enum class Separator
        {
            LeftParenthesis,
            RightParenthesis,
            LeftBrace,
            RightBrace,
            LeftBracket,
            RightBracket,
            Comma,
            Dot,
            Semicolon,
            EndOfProgram
        };

        struct Identifier {
            explicit Identifier(std::string identifier) : m_identifier(std::move(identifier)) { }

            [[nodiscard]] const std::string &get() const { return this->m_identifier; }

            bool operator==(const Identifier &) const  = default;

        private:
            std::string m_identifier;
        };

        using Literal    = std::variant<char, bool, u128, i128, double, std::string, ptrn::Pattern *>;
        using ValueTypes = std::variant<Keyword, Identifier, Operator, Literal, ValueType, Separator>;

        constexpr Token(Type type, auto value, u32 line, u32 column) : type(type), value(std::move(value)), line(line), column(column) {}

        [[nodiscard]] constexpr static inline bool isInteger(const ValueType &type) {
            return isUnsigned(type) || isSigned(type);
        }

        [[nodiscard]] constexpr static inline bool isUnsigned(const ValueType &type) {
            return (static_cast<u32>(type) & 0x0F) == 0x00;
        }

        [[nodiscard]] constexpr static inline bool isSigned(const ValueType &type) {
            return (static_cast<u32>(type) & 0x0F) == 0x01;
        }

        [[nodiscard]] constexpr static inline bool isFloatingPoint(const ValueType &type) {
            return (static_cast<u32>(type) & 0x0F) == 0x02;
        }

        [[nodiscard]] constexpr static inline u32 getTypeSize(const ValueType &type) {
            return static_cast<u32>(type) >> 4;
        }

        static ptrn::Pattern* literalToPattern(const core::Token::Literal &literal) {
            return std::visit(hlp::overloaded {
                    [&](ptrn::Pattern *result) -> ptrn::Pattern* { return result; },
                    [&](const std::string &) -> ptrn::Pattern* { err::E0004.throwError("Cannot cast value to type 'pattern'."); },
                    [](auto &&) ->  ptrn::Pattern* { err::E0004.throwError("Cannot cast value to type 'pattern'."); }
            }, literal);
        }

        static u128 literalToUnsigned(const core::Token::Literal &literal) {
            return std::visit(hlp::overloaded {
                    [&](ptrn::Pattern *) -> u128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                    [&](const std::string &) -> u128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                [](auto &&result) -> u128 { return result; }
            }, literal);
        }

        static i128 literalToSigned(const core::Token::Literal &literal) {
            return std::visit(hlp::overloaded {
                [](const std::string &) -> i128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                [](ptrn::Pattern *) -> i128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                [](auto &&result) -> i128 { return result; }
            }, literal);
        }

        static double literalToFloatingPoint(const core::Token::Literal &literal) {
            return std::visit(hlp::overloaded {
                [](const std::string &) -> double { err::E0004.throwError("Cannot cast value to type 'floating point'."); },
                [](ptrn::Pattern *) -> double { err::E0004.throwError("Cannot cast value to type 'floating point'."); },
                [](auto &&result) -> double { return result; }
            }, literal);
        }

        static bool literalToBoolean(const core::Token::Literal &literal) {
            return std::visit(hlp::overloaded {
                [](const std::string &) -> bool { err::E0004.throwError("Cannot cast value to type 'bool'."); },
                [](ptrn::Pattern *) -> bool { err::E0004.throwError("Cannot cast value to type 'bool'."); },
                [](auto &&result) -> bool { return result != 0; }
            },  literal);
        }

        static std::string literalToString(const core::Token::Literal &literal, bool cast) {
            if (!cast && std::get_if<std::string>(&literal) == nullptr)
                err::E0004.throwError("Expected value of type 'string'.");

            return std::visit(hlp::overloaded {
                [](std::string result) -> std::string { return result; },
                [](u128 result) -> std::string { return hlp::to_string(result); },
                [](i128 result) -> std::string { return hlp::to_string(result); },
                [](bool result) -> std::string { return result ? "true" : "false"; },
                [](char result) -> std::string { return { 1, result }; },
                [](ptrn::Pattern *) -> std::string { err::E0004.throwError("Cannot cast value to type 'str'."); },
                [](auto &&result) -> std::string { return std::to_string(result); }
            }, literal);
        }

        [[nodiscard]] constexpr static auto getTypeName(const core::Token::ValueType type) {
            switch (type) {
                case ValueType::Signed8Bit:
                    return "s8";
                case ValueType::Signed16Bit:
                    return "s16";
                case ValueType::Signed24Bit:
                    return "s24";
                case ValueType::Signed32Bit:
                    return "s32";
                case ValueType::Signed64Bit:
                    return "s64";
                case ValueType::Signed96Bit:
                    return "s96";
                case ValueType::Signed128Bit:
                    return "s128";
                case ValueType::Unsigned8Bit:
                    return "u8";
                case ValueType::Unsigned16Bit:
                    return "u16";
                case ValueType::Unsigned24Bit:
                    return "u24";
                case ValueType::Unsigned32Bit:
                    return "u32";
                case ValueType::Unsigned64Bit:
                    return "u64";
                case ValueType::Unsigned96Bit:
                    return "u96";
                case ValueType::Unsigned128Bit:
                    return "u128";
                case ValueType::Float:
                    return "float";
                case ValueType::Double:
                    return "double";
                case ValueType::Character:
                    return "char";
                case ValueType::Character16:
                    return "char16";
                case ValueType::Padding:
                    return "padding";
                case ValueType::String:
                    return "str";
                case ValueType::Boolean:
                    return "bool";
                default:
                    return "< ??? >";
            }
        }

        [[nodiscard]] std::string getFormattedType() const {
            switch (this->type) {
                using enum Token::Type;

                case Keyword: return "Keyword";
                case ValueType: return "Value Type";
                case Operator: return "Operator";
                case Integer: return "Integer";
                case String: return "String";
                case Identifier: return "Identifier";
                case Separator: return "Separator";
            }

            return "Unknown";
        }

        [[nodiscard]] std::string getFormattedValue() const {
            return std::visit(hlp::overloaded {
                    [](Token::Keyword keyword) -> std::string {
                        switch (keyword) {
                            using enum Token::Keyword;
                            case Struct: return "struct";
                            case Union: return "union";
                            case Using: return "using";
                            case Enum: return "enum";
                            case Bitfield: return "bitfield";
                            case LittleEndian: return "le";
                            case BigEndian: return "be";
                            case If: return "if";
                            case Else: return "else";
                            case Parent: return "parent";
                            case This: return "this";
                            case While: return "while";
                            case For: return "for";
                            case Function: return "fn";
                            case Return: return "return";
                            case Namespace: return "namespace";
                            case In: return "in";
                            case Out: return "out";
                            case Break: return "break";
                            case Continue: return "continue";
                        }

                        return "";
                    },
                    [](Token::Separator separator) -> std::string  {
                        switch(separator) {
                            using enum Token::Separator;

                            case LeftParenthesis: return "(";
                            case RightParenthesis: return ")";
                            case LeftBrace: return "{";
                            case RightBrace: return "}";
                            case LeftBracket: return "[";
                            case RightBracket: return "]";
                            case Comma: return ",";
                            case Dot: return ".";
                            case Semicolon: return ";";
                            case EndOfProgram: return "<EOF>";
                        }

                        return "";
                    },
                    [](Token::Operator op) -> std::string  {
                        switch (op) {
                            using enum Token::Operator;

                            case At: return "@";
                            case Assign: return "=";
                            case Colon: return ":";
                            case Plus: return "+";
                            case Minus: return "-";
                            case Star: return "*";
                            case Slash: return "/";
                            case Percent: return "%";
                            case LeftShift: return "<<";
                            case RightShift: return ">>";
                            case BitOr: return "|";
                            case BitAnd: return "&";
                            case BitXor: return "^";
                            case BitNot: return "~";
                            case BoolEqual: return "==";
                            case BoolNotEqual: return "!=";
                            case BoolGreaterThan: return ">";
                            case BoolLessThan: return "<";
                            case BoolGreaterThanOrEqual: return ">=";
                            case BoolLessThanOrEqual: return "<=";
                            case BoolAnd: return "&&";
                            case BoolOr: return "||";
                            case BoolXor: return "^^";
                            case BoolNot: return "!";
                            case TernaryConditional: return "?";
                            case Dollar: return "$";
                            case AddressOf: return "addressof";
                            case SizeOf: return "sizeof";
                            case ScopeResolution: return "::";
                        }

                        return "";
                    },
                    [](const Token::Identifier &identifier) -> std::string  {
                        return fmt::format("'{}'", identifier.get());
                    },
                    [](const Token::Literal &literal) -> std::string  {
                        return fmt::format("'{}'", Token::literalToString(literal, true));
                    },
                    [](Token::ValueType valueType) -> std::string  {
                        return getTypeName(valueType);
                    }
            }, this->value);
        }

        bool operator==(const ValueTypes &other) const {
            if (this->type == Type::Integer || this->type == Type::Identifier || this->type == Type::String)
                return true;
            else if (this->type == Type::ValueType) {
                auto otherValueType = std::get_if<ValueType>(&other);
                auto valueType      = std::get_if<ValueType>(&this->value);

                if (otherValueType == nullptr) return false;
                if (valueType == nullptr) return false;

                if (*otherValueType == *valueType)
                    return true;
                else if (*otherValueType == ValueType::Any)
                    return *valueType != ValueType::CustomType && *valueType != ValueType::Padding;
                else if (*otherValueType == ValueType::Unsigned)
                    return isUnsigned(*valueType);
                else if (*otherValueType == ValueType::Signed)
                    return isSigned(*valueType);
                else if (*otherValueType == ValueType::FloatingPoint)
                    return isFloatingPoint(*valueType);
                else if (*otherValueType == ValueType::Integer)
                    return isUnsigned(*valueType) || isSigned(*valueType);
            } else
                return other == this->value;

            return false;
        }

        bool operator!=(const ValueTypes &other) const {
            return !operator==(other);
        }

        Type type;
        ValueTypes value;
        u32 line = 1, column = 1;
    };

    namespace tkn {

        constexpr inline Token createToken(const core::Token::Type type, const core::Token::ValueTypes &value) {
            return { type, value, 1, 1 };
        }

        namespace Keyword {

            constexpr auto If           = createToken(core::Token::Type::Keyword, Token::Keyword::If);
            constexpr auto Else         = createToken(core::Token::Type::Keyword, Token::Keyword::Else);
            constexpr auto While        = createToken(core::Token::Type::Keyword, Token::Keyword::While);
            constexpr auto For          = createToken(core::Token::Type::Keyword, Token::Keyword::For);
            constexpr auto Return       = createToken(core::Token::Type::Keyword, Token::Keyword::Return);
            constexpr auto Break        = createToken(core::Token::Type::Keyword, Token::Keyword::Break);
            constexpr auto Continue     = createToken(core::Token::Type::Keyword, Token::Keyword::Continue);
            constexpr auto Struct       = createToken(core::Token::Type::Keyword, Token::Keyword::Struct);
            constexpr auto Enum         = createToken(core::Token::Type::Keyword, Token::Keyword::Enum);
            constexpr auto Union        = createToken(core::Token::Type::Keyword, Token::Keyword::Union);
            constexpr auto Function     = createToken(core::Token::Type::Keyword, Token::Keyword::Function);
            constexpr auto Bitfield     = createToken(core::Token::Type::Keyword, Token::Keyword::Bitfield);
            constexpr auto LittleEndian = createToken(core::Token::Type::Keyword, Token::Keyword::LittleEndian);
            constexpr auto BigEndian    = createToken(core::Token::Type::Keyword, Token::Keyword::BigEndian);
            constexpr auto Parent       = createToken(core::Token::Type::Keyword, Token::Keyword::Parent);
            constexpr auto Namespace    = createToken(core::Token::Type::Keyword, Token::Keyword::Namespace);
            constexpr auto Using        = createToken(core::Token::Type::Keyword, Token::Keyword::Using);
            constexpr auto This         = createToken(core::Token::Type::Keyword, Token::Keyword::This);
            constexpr auto In           = createToken(core::Token::Type::Keyword, Token::Keyword::In);
            constexpr auto Out          = createToken(core::Token::Type::Keyword, Token::Keyword::Out);

        }

        namespace Literal {

            constexpr auto Identifier   = [](const std::string &name = { }) -> Token     { return createToken(core::Token::Type::Identifier, Token::Identifier(name)); };
            constexpr auto Numeric      = [](const Token::Literal &value = { }) -> Token { return createToken(core::Token::Type::Integer, value); };
            constexpr auto String       = [](const std::string &value = { }) -> Token    { return createToken(core::Token::Type::String, Token::Literal(value)); };

        }

        namespace Operator {

            constexpr auto Plus                     = createToken(core::Token::Type::Operator, Token::Operator::Plus);
            constexpr auto Minus                    = createToken(core::Token::Type::Operator, Token::Operator::Minus);
            constexpr auto Star                     = createToken(core::Token::Type::Operator, Token::Operator::Star);
            constexpr auto Slash                    = createToken(core::Token::Type::Operator, Token::Operator::Slash);
            constexpr auto Percent                  = createToken(core::Token::Type::Operator, Token::Operator::Percent);
            constexpr auto LeftShift                = createToken(core::Token::Type::Operator, Token::Operator::LeftShift);
            constexpr auto RightShift               = createToken(core::Token::Type::Operator, Token::Operator::RightShift);
            constexpr auto BitAnd                   = createToken(core::Token::Type::Operator, Token::Operator::BitAnd);
            constexpr auto BitOr                    = createToken(core::Token::Type::Operator, Token::Operator::BitOr);
            constexpr auto BitXor                   = createToken(core::Token::Type::Operator, Token::Operator::BitXor);
            constexpr auto BitNot                   = createToken(core::Token::Type::Operator, Token::Operator::BitNot);
            constexpr auto BoolEqual                = createToken(core::Token::Type::Operator, Token::Operator::BoolEqual);
            constexpr auto BoolNotEqual             = createToken(core::Token::Type::Operator, Token::Operator::BoolNotEqual);
            constexpr auto BoolLessThan             = createToken(core::Token::Type::Operator, Token::Operator::BoolLessThan);
            constexpr auto BoolGreaterThan          = createToken(core::Token::Type::Operator, Token::Operator::BoolGreaterThan);
            constexpr auto BoolLessThanOrEqual      = createToken(core::Token::Type::Operator, Token::Operator::BoolLessThanOrEqual);
            constexpr auto BoolGreaterThanOrEqual   = createToken(core::Token::Type::Operator, Token::Operator::BoolGreaterThanOrEqual);
            constexpr auto BoolAnd                  = createToken(core::Token::Type::Operator, Token::Operator::BoolAnd);
            constexpr auto BoolOr                   = createToken(core::Token::Type::Operator, Token::Operator::BoolOr);
            constexpr auto BoolNot                  = createToken(core::Token::Type::Operator, Token::Operator::BoolNot);
            constexpr auto BoolXor                  = createToken(core::Token::Type::Operator, Token::Operator::BoolXor);
            constexpr auto Dollar                   = createToken(core::Token::Type::Operator, Token::Operator::Dollar);
            constexpr auto Colon                    = createToken(core::Token::Type::Operator, Token::Operator::Colon);
            constexpr auto ScopeResolution          = createToken(core::Token::Type::Operator, Token::Operator::ScopeResolution);
            constexpr auto TernaryConditional       = createToken(core::Token::Type::Operator, Token::Operator::TernaryConditional);
            constexpr auto AddressOf                = createToken(core::Token::Type::Operator, Token::Operator::AddressOf);
            constexpr auto SizeOf                   = createToken(core::Token::Type::Operator, Token::Operator::SizeOf);
            constexpr auto At                       = createToken(core::Token::Type::Operator, Token::Operator::At);
            constexpr auto Assign                   = createToken(core::Token::Type::Operator, Token::Operator::Assign);

        }

        namespace ValueType {

            constexpr auto CustomType       = createToken(core::Token::Type::ValueType, Token::ValueType::CustomType);
            constexpr auto Padding          = createToken(core::Token::Type::ValueType, Token::ValueType::Padding);
            constexpr auto Unsigned         = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned);
            constexpr auto Signed           = createToken(core::Token::Type::ValueType, Token::ValueType::Signed);
            constexpr auto FloatingPoint    = createToken(core::Token::Type::ValueType, Token::ValueType::FloatingPoint);
            constexpr auto Auto             = createToken(core::Token::Type::ValueType, Token::ValueType::Auto);
            constexpr auto Any              = createToken(core::Token::Type::ValueType, Token::ValueType::Any);

            constexpr auto Unsigned8Bit     = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned8Bit);
            constexpr auto Unsigned16Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned16Bit);
            constexpr auto Unsigned24Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned24Bit);
            constexpr auto Unsigned32Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned32Bit);
            constexpr auto Unsigned48Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned48Bit);
            constexpr auto Unsigned64Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned64Bit);
            constexpr auto Unsigned96Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned96Bit);
            constexpr auto Unsigned128Bit   = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned128Bit);

            constexpr auto Signed8Bit       = createToken(core::Token::Type::ValueType, Token::ValueType::Signed8Bit);
            constexpr auto Signed16Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed16Bit);
            constexpr auto Signed24Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed24Bit);
            constexpr auto Signed32Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed32Bit);
            constexpr auto Signed48Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed48Bit);
            constexpr auto Signed64Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed64Bit);
            constexpr auto Signed96Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed96Bit);
            constexpr auto Signed128Bit     = createToken(core::Token::Type::ValueType, Token::ValueType::Signed128Bit);

            constexpr auto Float            = createToken(core::Token::Type::ValueType, Token::ValueType::Float);
            constexpr auto Double           = createToken(core::Token::Type::ValueType, Token::ValueType::Double);

            constexpr auto Boolean          = createToken(core::Token::Type::ValueType, Token::ValueType::Boolean);

            constexpr auto Character        = createToken(core::Token::Type::ValueType, Token::ValueType::Character);
            constexpr auto Character16      = createToken(core::Token::Type::ValueType, Token::ValueType::Character16);
            constexpr auto String           = createToken(core::Token::Type::ValueType, Token::ValueType::String);

        }

        namespace Separator {

            constexpr auto Comma            = createToken(core::Token::Type::Separator, Token::Separator::Comma);
            constexpr auto LeftParenthesis  = createToken(core::Token::Type::Separator, Token::Separator::LeftParenthesis);
            constexpr auto RightParenthesis = createToken(core::Token::Type::Separator, Token::Separator::RightParenthesis);
            constexpr auto LeftBracket      = createToken(core::Token::Type::Separator, Token::Separator::LeftBracket);
            constexpr auto RightBracket     = createToken(core::Token::Type::Separator, Token::Separator::RightBracket);
            constexpr auto LeftBrace        = createToken(core::Token::Type::Separator, Token::Separator::LeftBrace);
            constexpr auto RightBrace       = createToken(core::Token::Type::Separator, Token::Separator::RightBrace);
            constexpr auto Dot              = createToken(core::Token::Type::Separator, Token::Separator::Dot);
            constexpr auto Semicolon        = createToken(core::Token::Type::Separator, Token::Separator::Semicolon);
            constexpr auto EndOfProgram     = createToken(core::Token::Type::Separator, Token::Separator::EndOfProgram);

        }

    }

}