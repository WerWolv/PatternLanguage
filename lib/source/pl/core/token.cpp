#include <pl/core/token.hpp>

#include <pl/patterns/pattern.hpp>

namespace pl::core {

    ptrn::Pattern* Token::Literal::toPattern() const {
        return std::visit(hlp::overloaded {
                              [&](ptrn::Pattern *result) -> ptrn::Pattern* { return result; },
                              [&](const std::string &) -> ptrn::Pattern* { err::E0004.throwError("Cannot cast value to type 'pattern'."); },
                              [](auto &&) ->  ptrn::Pattern* { err::E0004.throwError("Cannot cast value to type 'pattern'."); }
                          }, *this);
    }

    u128 Token::Literal::toUnsigned() const {
        return std::visit(hlp::overloaded {
                              [&](ptrn::Pattern *) -> u128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                              [&](const std::string &) -> u128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                              [](auto &&result) -> u128 { return result; }
                          }, *this);
    }

    i128 Token::Literal::toSigned() const {
        return std::visit(hlp::overloaded {
                              [](const std::string &) -> i128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                              [](ptrn::Pattern *) -> i128 { err::E0004.throwError("Cannot cast value to type 'integer'."); },
                              [](auto &&result) -> i128 { return result; }
                          }, *this);
    }

    double Token::Literal::toFloatingPoint() const {
        return std::visit(hlp::overloaded {
                              [](const std::string &) -> double { err::E0004.throwError("Cannot cast value to type 'floating point'."); },
                              [](ptrn::Pattern *) -> double { err::E0004.throwError("Cannot cast value to type 'floating point'."); },
                              [](auto &&result) -> double { return result; }
                          }, *this);
    }

    char Token::Literal::toCharacter() const {
        return std::visit(hlp::overloaded {
                              [&](ptrn::Pattern *) -> char { err::E0004.throwError("Cannot cast value to type 'char'."); },
                              [&](const std::string &) -> char { err::E0004.throwError("Cannot cast value to type 'char'."); },
                              [](auto &&result) -> char { return result; }
                          }, *this);
    }

    bool Token::Literal::toBoolean() const {
        return std::visit(hlp::overloaded {
                              [](const std::string &) -> bool { err::E0004.throwError("Cannot cast value to type 'bool'."); },
                              [](ptrn::Pattern *) -> bool { err::E0004.throwError("Cannot cast value to type 'bool'."); },
                              [](auto &&result) -> bool { return result != 0; }
                          },  *this);
    }

    std::string Token::Literal::toString(bool cast) const {
        if (!cast && std::get_if<std::string>(this) == nullptr)
            err::E0004.throwError("Expected value of type 'string'.");

        return std::visit(hlp::overloaded {
                              [](const std::string &result) -> std::string { return result; },
                              [](const u128 &result) -> std::string { return hlp::to_string(result); },
                              [](const i128 &result) -> std::string { return hlp::to_string(result); },
                              [](const bool &result) -> std::string { return result ? "true" : "false"; },
                              [](const char &result) -> std::string { return { result }; },
                              [](ptrn::Pattern *result) -> std::string { return result->toString(); },
                              [](auto &&result) -> std::string { return std::to_string(result); }
                          }, *this);
    }

    std::vector<u8> Token::Literal::toBytes() const {
        return std::visit(hlp::overloaded {
                [](const std::string &result) -> std::vector<u8> { return { result.begin(), result.end() }; },
                [](ptrn::Pattern *result) -> std::vector<u8> { return result->getBytes(); },
                [](auto &&result) -> std::vector<u8> { return hlp::toBytes(result); }
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
        return std::holds_alternative<ptrn::Pattern *>(*this);
    }

    Token::ValueType Token::Literal::getType() const {
        return std::visit(hlp::overloaded {
                [](char) { return Token::ValueType::Character; },
                [](bool) { return Token::ValueType::Boolean; },
                [](u128) { return Token::ValueType::Unsigned128Bit; },
                [](i128) { return Token::ValueType::Signed128Bit; },
                [](double) { return Token::ValueType::Double; },
                [](const std::string &) { return Token::ValueType::String; },
                [](const ptrn::Pattern *) { return Token::ValueType::CustomType; }
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
            case DocComment: return "Doc Comment";
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
            default:
                return "???";
        }
    }

    [[nodiscard]] std::string Token::getFormattedValue() const {
        return std::visit(hlp::overloaded {
                              [](Token::Keyword keyword) -> std::string {
                                  switch (keyword) {
                                      using enum Token::Keyword;
                                      case Struct: return "struct";
                                      case Union: return "union";
                                      case Using: return "using";
                                      case Enum: return "enum";
                                      case Match: return "match";
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
                                      case Reference: return "ref";
                                      case Null: return "null";
                                      case Const: return "const";
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
                                      case Underscore: return "_";
                                  }

                                  return "";
                              },
                              [](const Token::Identifier &identifier) -> std::string  {
                                  return fmt::format("'{}'", identifier.get());
                              },
                              [](const Token::Literal &literal) -> std::string  {
                                  return fmt::format("'{}'", literal.toString(true));
                              },
                              [](Token::ValueType valueType) -> std::string  {
                                  return getTypeName(valueType);
                              },
                              [](const Token::DocComment &docComment) -> std::string {
                                  if (docComment.global)
                                      return fmt::format("/*! {} */", docComment.comment);
                                  else
                                      return fmt::format("/** {} */", docComment.comment);
                              }
                          }, this->value);
    }

    bool Token::operator==(const ValueTypes &other) const {
        if (this->type == Type::Integer || this->type == Type::Identifier || this->type == Type::String || this->type == Type::DocComment)
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

    bool Token::operator!=(const ValueTypes &other) const {
        return !operator==(other);
    }

}