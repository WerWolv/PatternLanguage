#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <pl/helpers/types.hpp>

namespace pl::ptrn {
    class Pattern;
}

namespace pl::core {

    namespace ast { class ASTNode; }

    class Token {
    public:
        Token() = default;

        enum class Type : u64 {
            Keyword,
            ValueType,
            Operator,
            Integer,
            String,
            Identifier,
            Separator,
            DocComment
        };

        enum class Keyword {
            Struct,
            Union,
            Using,
            Enum,
            Bitfield,
            Unsigned,
            Signed,
            LittleEndian,
            BigEndian,
            If,
            Else,
            Parent,
            This,
            While,
            Match,
            For,
            Function,
            Return,
            Namespace,
            In,
            Out,
            Break,
            Continue,
            Reference,
            Null,
            Const,
            Underscore,
            Try,
            Catch
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
            TypeNameOf,
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

        struct DocComment {
            bool global;
            std::string comment;

            constexpr bool operator==(const DocComment &) const = default;
        };

        struct Literal : public std::variant<char, bool, u128, i128, double, std::string, std::shared_ptr<ptrn::Pattern>> {
            using variant::variant;

            [[nodiscard]] std::shared_ptr<ptrn::Pattern> toPattern() const;
            [[nodiscard]] u128 toUnsigned() const;
            [[nodiscard]] i128 toSigned() const;
            [[nodiscard]] double toFloatingPoint() const;
            [[nodiscard]] char toCharacter() const;
            [[nodiscard]] bool toBoolean() const;
            [[nodiscard]] std::string toString(bool cast = false) const;
            [[nodiscard]] std::vector<u8> toBytes() const;

            [[nodiscard]] bool isPattern() const;
            [[nodiscard]] bool isUnsigned() const;
            [[nodiscard]] bool isSigned() const;
            [[nodiscard]] bool isFloatingPoint() const;
            [[nodiscard]] bool isCharacter() const;
            [[nodiscard]] bool isBoolean() const;
            [[nodiscard]] bool isString() const;

            [[nodiscard]] ValueType getType() const;
        };

        using ValueTypes = std::variant<Keyword, Identifier, Operator, Literal, ValueType, Separator, DocComment>;

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

        inline Token createToken(const core::Token::Type type, const core::Token::ValueTypes &value) {
            return { type, value, 1, 1 };
        }

        namespace Keyword {

            const auto If           = createToken(core::Token::Type::Keyword, Token::Keyword::If);
            const auto Else         = createToken(core::Token::Type::Keyword, Token::Keyword::Else);
            const auto While        = createToken(core::Token::Type::Keyword, Token::Keyword::While);
            const auto For          = createToken(core::Token::Type::Keyword, Token::Keyword::For);
            const auto Match        = createToken(core::Token::Type::Keyword, Token::Keyword::Match);
            const auto Return       = createToken(core::Token::Type::Keyword, Token::Keyword::Return);
            const auto Break        = createToken(core::Token::Type::Keyword, Token::Keyword::Break);
            const auto Continue     = createToken(core::Token::Type::Keyword, Token::Keyword::Continue);
            const auto Struct       = createToken(core::Token::Type::Keyword, Token::Keyword::Struct);
            const auto Enum         = createToken(core::Token::Type::Keyword, Token::Keyword::Enum);
            const auto Union        = createToken(core::Token::Type::Keyword, Token::Keyword::Union);
            const auto Function     = createToken(core::Token::Type::Keyword, Token::Keyword::Function);
            const auto Bitfield     = createToken(core::Token::Type::Keyword, Token::Keyword::Bitfield);
            const auto Unsigned     = createToken(core::Token::Type::Keyword, Token::Keyword::Unsigned);
            const auto Signed       = createToken(core::Token::Type::Keyword, Token::Keyword::Signed);
            const auto LittleEndian = createToken(core::Token::Type::Keyword, Token::Keyword::LittleEndian);
            const auto BigEndian    = createToken(core::Token::Type::Keyword, Token::Keyword::BigEndian);
            const auto Parent       = createToken(core::Token::Type::Keyword, Token::Keyword::Parent);
            const auto Namespace    = createToken(core::Token::Type::Keyword, Token::Keyword::Namespace);
            const auto Using        = createToken(core::Token::Type::Keyword, Token::Keyword::Using);
            const auto This         = createToken(core::Token::Type::Keyword, Token::Keyword::This);
            const auto In           = createToken(core::Token::Type::Keyword, Token::Keyword::In);
            const auto Out          = createToken(core::Token::Type::Keyword, Token::Keyword::Out);
            const auto Reference    = createToken(core::Token::Type::Keyword, Token::Keyword::Reference);
            const auto Null         = createToken(core::Token::Type::Keyword, Token::Keyword::Null);
            const auto Const        = createToken(core::Token::Type::Keyword, Token::Keyword::Const);
            const auto Underscore   = createToken(core::Token::Type::Keyword, Token::Keyword::Underscore);
            const auto Try          = createToken(core::Token::Type::Keyword, Token::Keyword::Try);
            const auto Catch        = createToken(core::Token::Type::Keyword, Token::Keyword::Catch);

        }

        namespace Literal {

            const auto IdentifierValue   = [](const std::string &name = { }) -> Token           { return createToken(core::Token::Type::Identifier, Token::Identifier(name)); };
            const auto NumericValue      = [](const Token::Literal &value = { }) -> Token       { return createToken(core::Token::Type::Integer, value); };
            const auto StringValue       = [](const std::string &value = { }) -> Token          { return createToken(core::Token::Type::String, Token::Literal(value)); };
            const auto DocComment        = [](bool global, const std::string &value) -> Token   { return { core::Token::Type::DocComment, Token::DocComment { global, value }, 1, 1 }; };

            const auto Identifier = createToken(core::Token::Type::Identifier, { });
            const auto Numeric = createToken(core::Token::Type::Integer, { });
            const auto String = createToken(core::Token::Type::String, { });

        }

        namespace Operator {

            const auto Plus                     = createToken(core::Token::Type::Operator, Token::Operator::Plus);
            const auto Minus                    = createToken(core::Token::Type::Operator, Token::Operator::Minus);
            const auto Star                     = createToken(core::Token::Type::Operator, Token::Operator::Star);
            const auto Slash                    = createToken(core::Token::Type::Operator, Token::Operator::Slash);
            const auto Percent                  = createToken(core::Token::Type::Operator, Token::Operator::Percent);
            const auto LeftShift                = createToken(core::Token::Type::Operator, Token::Operator::LeftShift);
            const auto RightShift               = createToken(core::Token::Type::Operator, Token::Operator::RightShift);
            const auto BitAnd                   = createToken(core::Token::Type::Operator, Token::Operator::BitAnd);
            const auto BitOr                    = createToken(core::Token::Type::Operator, Token::Operator::BitOr);
            const auto BitXor                   = createToken(core::Token::Type::Operator, Token::Operator::BitXor);
            const auto BitNot                   = createToken(core::Token::Type::Operator, Token::Operator::BitNot);
            const auto BoolEqual                = createToken(core::Token::Type::Operator, Token::Operator::BoolEqual);
            const auto BoolNotEqual             = createToken(core::Token::Type::Operator, Token::Operator::BoolNotEqual);
            const auto BoolLessThan             = createToken(core::Token::Type::Operator, Token::Operator::BoolLessThan);
            const auto BoolGreaterThan          = createToken(core::Token::Type::Operator, Token::Operator::BoolGreaterThan);
            const auto BoolLessThanOrEqual      = createToken(core::Token::Type::Operator, Token::Operator::BoolLessThanOrEqual);
            const auto BoolGreaterThanOrEqual   = createToken(core::Token::Type::Operator, Token::Operator::BoolGreaterThanOrEqual);
            const auto BoolAnd                  = createToken(core::Token::Type::Operator, Token::Operator::BoolAnd);
            const auto BoolOr                   = createToken(core::Token::Type::Operator, Token::Operator::BoolOr);
            const auto BoolNot                  = createToken(core::Token::Type::Operator, Token::Operator::BoolNot);
            const auto BoolXor                  = createToken(core::Token::Type::Operator, Token::Operator::BoolXor);
            const auto Dollar                   = createToken(core::Token::Type::Operator, Token::Operator::Dollar);
            const auto Colon                    = createToken(core::Token::Type::Operator, Token::Operator::Colon);
            const auto ScopeResolution          = createToken(core::Token::Type::Operator, Token::Operator::ScopeResolution);
            const auto TernaryConditional       = createToken(core::Token::Type::Operator, Token::Operator::TernaryConditional);
            const auto AddressOf                = createToken(core::Token::Type::Operator, Token::Operator::AddressOf);
            const auto SizeOf                   = createToken(core::Token::Type::Operator, Token::Operator::SizeOf);
            const auto TypeNameOf               = createToken(core::Token::Type::Operator, Token::Operator::TypeNameOf);
            const auto At                       = createToken(core::Token::Type::Operator, Token::Operator::At);
            const auto Assign                   = createToken(core::Token::Type::Operator, Token::Operator::Assign);

        }

        namespace ValueType {

            const auto CustomType       = createToken(core::Token::Type::ValueType, Token::ValueType::CustomType);
            const auto Padding          = createToken(core::Token::Type::ValueType, Token::ValueType::Padding);
            const auto Unsigned         = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned);
            const auto Signed           = createToken(core::Token::Type::ValueType, Token::ValueType::Signed);
            const auto FloatingPoint    = createToken(core::Token::Type::ValueType, Token::ValueType::FloatingPoint);
            const auto Auto             = createToken(core::Token::Type::ValueType, Token::ValueType::Auto);
            const auto Any              = createToken(core::Token::Type::ValueType, Token::ValueType::Any);

            const auto Unsigned8Bit     = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned8Bit);
            const auto Unsigned16Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned16Bit);
            const auto Unsigned24Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned24Bit);
            const auto Unsigned32Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned32Bit);
            const auto Unsigned48Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned48Bit);
            const auto Unsigned64Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned64Bit);
            const auto Unsigned96Bit    = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned96Bit);
            const auto Unsigned128Bit   = createToken(core::Token::Type::ValueType, Token::ValueType::Unsigned128Bit);

            const auto Signed8Bit       = createToken(core::Token::Type::ValueType, Token::ValueType::Signed8Bit);
            const auto Signed16Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed16Bit);
            const auto Signed24Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed24Bit);
            const auto Signed32Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed32Bit);
            const auto Signed48Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed48Bit);
            const auto Signed64Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed64Bit);
            const auto Signed96Bit      = createToken(core::Token::Type::ValueType, Token::ValueType::Signed96Bit);
            const auto Signed128Bit     = createToken(core::Token::Type::ValueType, Token::ValueType::Signed128Bit);

            const auto Float            = createToken(core::Token::Type::ValueType, Token::ValueType::Float);
            const auto Double           = createToken(core::Token::Type::ValueType, Token::ValueType::Double);

            const auto Boolean          = createToken(core::Token::Type::ValueType, Token::ValueType::Boolean);

            const auto Character        = createToken(core::Token::Type::ValueType, Token::ValueType::Character);
            const auto Character16      = createToken(core::Token::Type::ValueType, Token::ValueType::Character16);
            const auto String           = createToken(core::Token::Type::ValueType, Token::ValueType::String);

        }

        namespace Separator {

            const auto Comma            = createToken(core::Token::Type::Separator, Token::Separator::Comma);
            const auto LeftParenthesis  = createToken(core::Token::Type::Separator, Token::Separator::LeftParenthesis);
            const auto RightParenthesis = createToken(core::Token::Type::Separator, Token::Separator::RightParenthesis);
            const auto LeftBracket      = createToken(core::Token::Type::Separator, Token::Separator::LeftBracket);
            const auto RightBracket     = createToken(core::Token::Type::Separator, Token::Separator::RightBracket);
            const auto LeftBrace        = createToken(core::Token::Type::Separator, Token::Separator::LeftBrace);
            const auto RightBrace       = createToken(core::Token::Type::Separator, Token::Separator::RightBrace);
            const auto Dot              = createToken(core::Token::Type::Separator, Token::Separator::Dot);
            const auto Semicolon        = createToken(core::Token::Type::Separator, Token::Separator::Semicolon);
            const auto EndOfProgram     = createToken(core::Token::Type::Separator, Token::Separator::EndOfProgram);

        }

    }

}