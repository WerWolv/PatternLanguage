#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <limits>

#include <pl/helpers/types.hpp>

namespace pl::ptrn {
    class Pattern;
}

namespace pl::core {

    namespace ast { class ASTNode; }

    struct Location {
        u32 line;
        u32 column;
    };

    class Token {
    public:
        static std::map<std::string_view, Token>& operators();
        static std::map<char,             Token>& separators();
        static std::map<std::string_view, Token>& keywords();
        static std::map<std::string_view, Token>& types();

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
            Catch,
            Import,
            As,
            Is
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

        constexpr Token(Type type, auto value, Location location) : type(type), value(std::move(value)), location(location) {}

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
        Location location;

    };

}