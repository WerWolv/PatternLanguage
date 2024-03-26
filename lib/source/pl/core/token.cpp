#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>

#include <pl/patterns/pattern.hpp>

#include <pl/helpers/utils.hpp>
#include <pl/helpers/concepts.hpp>
#include <variant>

namespace pl::core {

    std::shared_ptr<ptrn::Pattern> Token::Literal::toPattern() const {
        return std::visit(wolv::util::overloaded {
                              [&](const std::shared_ptr<ptrn::Pattern> &result) -> std::shared_ptr<ptrn::Pattern> { return result; },
                              [&](const std::string &) -> std::shared_ptr<ptrn::Pattern> { err::E0004.throwError("Cannot cast value to type 'pattern'."); },
                              [](auto &&) ->  std::shared_ptr<ptrn::Pattern> { err::E0004.throwError("Cannot cast value to type 'pattern'."); }
                          }, *this);
    }

    u128 Token::Literal::toUnsigned() const {
        return std::visit(wolv::util::overloaded {
                              [&](const std::shared_ptr<ptrn::Pattern>&) -> u128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                              [&](const std::string &) -> u128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                              [](auto &&result) -> u128 { return result; }
                          }, *this);
    }

    i128 Token::Literal::toSigned() const {
        return std::visit(wolv::util::overloaded {
                              [](const std::shared_ptr<ptrn::Pattern>&) -> i128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                              [](const std::string &) -> i128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                              [](auto &&result) -> i128 { return result; }
                          }, *this);
    }

    double Token::Literal::toFloatingPoint() const {
        return std::visit(wolv::util::overloaded {
                              [](const std::shared_ptr<ptrn::Pattern>&) -> double { err::E0004.throwError("Cannot cast value to type 'floating point'."); },
                              [](const std::string &) -> double { err::E0004.throwError("Cannot cast value to type 'floating point'."); },
                              [](auto &&result) -> double { return result; }
                          }, *this);
    }

    char Token::Literal::toCharacter() const {
        return std::visit(wolv::util::overloaded {
                              [&](const std::shared_ptr<ptrn::Pattern>&) -> char { err::E0004.throwError("Cannot cast value to type 'char'."); },
                              [&](const std::string &) -> char { err::E0004.throwError("Cannot cast value to type 'char'."); },
                              [](auto &&result) -> char { return result; }
                          }, *this);
    }

    bool Token::Literal::toBoolean() const {
        return std::visit(wolv::util::overloaded {
                              [](const std::shared_ptr<ptrn::Pattern>&) -> bool { err::E0004.throwError("Cannot cast value to type 'bool'."); },
                              [](const std::string &) -> bool { err::E0004.throwError("Cannot cast value to type 'bool'."); },
                              [](auto &&result) -> bool { return result != 0; }
                          },  *this);
    }

    std::string Token::Literal::toString(bool cast) const {
        if (!cast && std::get_if<std::string>(this) == nullptr)
            err::E0004.throwError("Expected value of type 'string'.");

        return std::visit(wolv::util::overloaded {
                              [](const std::string &result) -> std::string { return result; },
                              [](const u128 &result) -> std::string { return hlp::to_string(result); },
                              [](const i128 &result) -> std::string { return hlp::to_string(result); },
                              [](const bool &result) -> std::string { return result ? "true" : "false"; },
                              [](const char &result) -> std::string { return { result }; },
                              [](const std::shared_ptr<ptrn::Pattern> &result) -> std::string { return result->toString(); },
                              [](auto &&result) -> std::string { return std::to_string(result); }
                          }, *this);
    }

    std::vector<u8> Token::Literal::toBytes() const {
        return std::visit(wolv::util::overloaded {
                [](const std::string &result) -> std::vector<u8> { return { result.begin(), result.end() }; },
                [](ptrn::Pattern *result) -> std::vector<u8> { return result->getBytes(); },
                [](auto &&result) -> std::vector<u8> {
                    return wolv::util::toContainer<std::vector<u8>>(wolv::util::toBytes(result));
                }
        }, *this);
    }

    bool Token::Literal::isUnsigned() const {
        return std::holds_alternative<u128>(*this);
    }

    bool Token::Literal::isSigned() const {
        return std::holds_alternative<i128>(*this);
    }

    bool Token::Literal::isFloatingPoint() const {
        return std::holds_alternative<double>(*this);
    }

    bool Token::Literal::isCharacter() const {
        return std::holds_alternative<char>(*this);
    }

    bool Token::Literal::isBoolean() const {
        return std::holds_alternative<bool>(*this);
    }

    bool Token::Literal::isString() const {
        return std::holds_alternative<std::string>(*this);
    }

    bool Token::Literal::isPattern() const {
        return std::holds_alternative<std::shared_ptr<ptrn::Pattern>>(*this);
    }

    std::strong_ordering Token::Literal::operator<=>(const Literal &other) const {
        return std::visit(wolv::util::overloaded {
            []<typename T>(T lhs, T rhs) -> std::strong_ordering {
                if (lhs == rhs) return std::strong_ordering::equal;
                if (lhs < rhs) return std::strong_ordering::less;
                return std::strong_ordering::greater;
            },
            [](pl::integral auto lhs, pl::integral auto rhs) -> std::strong_ordering {
                if constexpr (std::same_as<decltype(lhs), char> || std::same_as<decltype(rhs), char>)
                    return char(lhs) <=> char(rhs);
                else if constexpr (std::same_as<decltype(lhs), bool> || std::same_as<decltype(rhs), bool>)
                    return bool(lhs) <=> bool(rhs);
                else {
                    if (std::cmp_equal(lhs, rhs)) return std::strong_ordering::equal;
                    if (std::cmp_less(lhs, rhs)) return std::strong_ordering::less;
                    return std::strong_ordering::greater;
                }
            },
            [](pl::integral auto lhs, pl::floating_point auto rhs) -> std::strong_ordering {
                if (lhs == rhs) return std::strong_ordering::equal;
                if (lhs < rhs) return std::strong_ordering::less;
                return std::strong_ordering::greater;
            },
            [](pl::floating_point auto lhs, pl::integral auto rhs) -> std::strong_ordering {
                if (lhs == rhs) return std::strong_ordering::equal;
                if (lhs < rhs) return std::strong_ordering::less;
                return std::strong_ordering::greater;
            },
            [](auto, auto) -> std::strong_ordering {
                return std::strong_ordering::less;
            }
        }, *this, other);
    }

    Token::ValueType Token::Literal::getType() const {
        return std::visit(wolv::util::overloaded {
                [](char) { return Token::ValueType::Character; },
                [](bool) { return Token::ValueType::Boolean; },
                [](u128) { return Token::ValueType::Unsigned128Bit; },
                [](i128) { return Token::ValueType::Signed128Bit; },
                [](double) { return Token::ValueType::Double; },
                [](const std::string &) { return Token::ValueType::String; },
                [](const std::shared_ptr<ptrn::Pattern>&) { return Token::ValueType::CustomType; }
        }, *this);
    }

    [[nodiscard]] std::string Token::getFormattedType() const {
        switch (this->type) {
            using enum Token::Type;

            case Keyword: return "Keyword";
            case ValueType: return "Value Type";
            case Operator: return "Operator";
            case Integer: return "Integer";
            case String: return "String";
            case Identifier: return "Identifier";
            case Separator: return "Separator";
            case Comment: return "Comment";
            case DocComment: return "Doc Comment";
            case Directive: return "Directive";
        }

        return "Unknown";
    }


    [[nodiscard]] const char* Token::getTypeName(const core::Token::ValueType type) {
        switch (type) {
            case ValueType::Signed8Bit:
                return "s8";
            case ValueType::Signed16Bit:
                return "s16";
            case ValueType::Signed24Bit:
                return "s24";
            case ValueType::Signed32Bit:
                return "s32";
            case ValueType::Signed48Bit:
                return "s48";
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
            case ValueType::Unsigned48Bit:
                return "u48";
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
            case ValueType::Auto:
                return "auto";
            default:
                return "???";
        }
    }

    [[nodiscard]] std::string Token::getFormattedValue() const {
        return std::visit(wolv::util::overloaded {
                              [](const Keyword keyword) -> std::string {
                                  switch (keyword) {
                                      using enum Keyword;
                                      case Struct: return "struct";
                                      case Union: return "union";
                                      case Using: return "using";
                                      case Enum: return "enum";
                                      case Match: return "match";
                                      case Bitfield: return "bitfield";
                                      case Unsigned: return "unsigned";
                                      case Signed: return "signed";
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
                                      case Reference: return "ref";
                                      case Null: return "null";
                                      case Const: return "const";
                                      case Underscore: return "_";
                                      case Try: return "try";
                                      case Catch: return "catch";
                                      case Import: return "import";
                                      case As: return "as";
                                      case Is: return "is";
                                  }

                                  return "";
                              },
                              [](const Separator separator) -> std::string  {
                                  switch(separator) {
                                      using enum Separator;

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
                              [](const Operator op) -> std::string  {
                                  switch (op) {
                                      using enum Operator;

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
                                      case TypeNameOf: return "typenameof";
                                      case ScopeResolution: return "::";
                                  }

                                  return "";
                              },
                              [](const Directive directive) -> std::string  {
                                  switch (directive) {
                                      using enum Directive;

                                      case Include: return "#include";
                                      case Define: return "#define";
                                      case IfDef: return "#ifdef";
                                      case IfNDef: return "#ifndef";
                                      case EndIf: return "#endif";
                                      case Undef: return "#undef";
                                      case Pragma: return "#pragma";
                                      case Error: return "#error";
                                  }

                                  return "";
                              },
                              [](const Identifier &identifier) -> std::string  {
                                  return fmt::format("'{}'", identifier.get());
                              },
                              [](const Literal &literal) -> std::string  {
                                  return fmt::format("'{}'", literal.toString(true));
                              },
                              [](const ValueType valueType) -> std::string  {
                                  return getTypeName(valueType);
                              },
                              [](const Comment &comment) -> std::string  {
                                  return fmt::format("/* {} */", comment.comment);
                              },
                              [](const DocComment &docComment) -> std::string  {
                                  if (docComment.global)
                                      return fmt::format("/*! {} */", docComment.comment);
                                  else
                                      return fmt::format("/** {} */", docComment.comment);
                              }
                          }, this->value);
    }

    bool Token::operator==(const ValueTypes &other) const {
        if (this->type == Type::Integer || this->type == Type::Identifier || this->type == Type::String || this->type == Type::Comment || this->type == Type::DocComment || this->type == Type::Directive)
            return true;
        if (this->type == Type::ValueType) {
            const auto otherValueType = std::get_if<ValueType>(&other);
            const auto valueType      = std::get_if<ValueType>(&this->value);

            if (otherValueType == nullptr) return false;
            if (valueType == nullptr) return false;

            if (*otherValueType == *valueType)
                return true;
            if (*otherValueType == ValueType::Any)
                return *valueType != ValueType::CustomType && *valueType != ValueType::Padding;
            if (*otherValueType == ValueType::Unsigned)
                return isUnsigned(*valueType);
            if (*otherValueType == ValueType::Signed)
                return isSigned(*valueType);
            if (*otherValueType == ValueType::FloatingPoint)
                return isFloatingPoint(*valueType);
            if (*otherValueType == ValueType::Integer)
                return isUnsigned(*valueType) || isSigned(*valueType);
        } else
            return other == this->value;

        return false;
    }

    bool Token::operator!=(const ValueTypes &other) const {
        return !operator==(other);
    }

    std::map<std::string_view, Token>& Token::Operators() {
        static std::map<std::string_view, Token> s_operators;

        return s_operators;
    }

    std::map<std::string_view, Token>& Token::Keywords() {
        static std::map<std::string_view, Token> s_keywords;

        return s_keywords;
    }

    std::map<char, Token>& Token::Separators() {
        static std::map<char, Token> s_separators;

        return s_separators;
    }

    std::map<std::string_view, Token> &Token::Types() {
        static std::map<std::string_view, Token> s_types;

        return s_types;
    }

    std::map<std::string_view, Token>& Token::Directives() {
        static std::map<std::string_view, Token> s_directives;

        return s_directives;
    }

}