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
        enum class Type : u64 {
            Keyword,
            ValueType,
            Operator,
            Integer,
            String,
            Identifier,
            Separator
        };

        enum class Keyword {
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
            Continue,
            Reference
        };

        enum class Operator {
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

        enum class ValueType {
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

        enum class Separator {
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

        [[nodiscard]] constexpr static inline Token::ValueType getType(const Token::Literal &literal) {
            return std::visit(hlp::overloaded {
                [](char) { return Token::ValueType::Character; },
                [](bool) { return Token::ValueType::Boolean; },
                [](u128) { return Token::ValueType::Unsigned128Bit; },
                [](i128) { return Token::ValueType::Signed128Bit; },
                [](double) { return Token::ValueType::Double; },
                [](const std::string &) { return Token::ValueType::String; },
                [](const ptrn::Pattern *) { return Token::ValueType::CustomType; }
            }, literal);
        }

        static ptrn::Pattern* literalToPattern(const core::Token::Literal &literal);
        static u128 literalToUnsigned(const core::Token::Literal &literal);
        static i128 literalToSigned(const core::Token::Literal &literal);
        static double literalToFloatingPoint(const core::Token::Literal &literal);
        static char literalToCharacter(const core::Token::Literal &literal);
        static bool literalToBoolean(const core::Token::Literal &literal);
        static std::string literalToString(const core::Token::Literal &literal, bool cast);

        [[nodiscard]] static const char* getTypeName(core::Token::ValueType type);

        [[nodiscard]] std::string getFormattedType() const;
        [[nodiscard]] std::string getFormattedValue() const;

        bool operator==(const ValueTypes &other) const;
        bool operator!=(const ValueTypes &other) const;

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
            constexpr auto Reference    = createToken(core::Token::Type::Keyword, Token::Keyword::Reference);

        }

        namespace Literal {

            constexpr auto IdentifierValue   = [](const std::string &name = { }) -> Token     { return createToken(core::Token::Type::Identifier, Token::Identifier(name)); };
            constexpr auto NumericValue      = [](const Token::Literal &value = { }) -> Token { return createToken(core::Token::Type::Integer, value); };
            constexpr auto StringValue       = [](const std::string &value = { }) -> Token    { return createToken(core::Token::Type::String, Token::Literal(value)); };

            constexpr auto Identifier = createToken(core::Token::Type::Identifier, { });
            constexpr auto Numeric = createToken(core::Token::Type::Integer, { });
            constexpr auto String = createToken(core::Token::Type::String, { });

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