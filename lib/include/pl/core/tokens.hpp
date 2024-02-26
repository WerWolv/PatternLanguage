#pragma once

#include <pl/core/token.hpp>

namespace pl::core::tkn {

    static const inline std::map<std::string_view, Token::Literal> constants = {
            { "true", Token::Literal(1) },
            { "false", Token::Literal(0) },
            { "nan", Token::Literal(std::numeric_limits<double>::quiet_NaN()) },
            { "inf", Token::Literal(std::numeric_limits<double>::infinity()) },
    };

    inline Token makeToken(const core::Token::Type type, const core::Token::ValueTypes &value) {
        return { type, value, Location::Empty() };
    }

    namespace Keyword {

        inline Token makeKeyword(const Token::Keyword& keyword, const std::string_view& name) {
            auto token = makeToken(core::Token::Type::Keyword, keyword);
            Token::Keywords()[name] = token;
            return token;
        }

        const auto If           = makeKeyword(Token::Keyword::If, "if");
        const auto Else         = makeKeyword(Token::Keyword::Else, "else");
        const auto While        = makeKeyword(Token::Keyword::While, "while");
        const auto For          = makeKeyword(Token::Keyword::For, "for");
        const auto Match        = makeKeyword(Token::Keyword::Match, "match");
        const auto Return       = makeKeyword(Token::Keyword::Return, "return");
        const auto Break        = makeKeyword(Token::Keyword::Break, "break");
        const auto Continue     = makeKeyword(Token::Keyword::Continue, "continue");
        const auto Struct       = makeKeyword(Token::Keyword::Struct, "struct");
        const auto Enum         = makeKeyword(Token::Keyword::Enum, "enum");
        const auto Union        = makeKeyword(Token::Keyword::Union, "union");
        const auto Function     = makeKeyword(Token::Keyword::Function, "fn");
        const auto Bitfield     = makeKeyword(Token::Keyword::Bitfield, "bitfield");
        const auto Unsigned     = makeKeyword(Token::Keyword::Unsigned, "unsigned");
        const auto Signed       = makeKeyword(Token::Keyword::Signed, "signed");
        const auto LittleEndian = makeKeyword(Token::Keyword::LittleEndian, "le");
        const auto BigEndian    = makeKeyword(Token::Keyword::BigEndian, "be");
        const auto Parent       = makeKeyword(Token::Keyword::Parent, "parent");
        const auto Namespace    = makeKeyword(Token::Keyword::Namespace, "namespace");
        const auto Using        = makeKeyword(Token::Keyword::Using, "using");
        const auto This         = makeKeyword(Token::Keyword::This, "this");
        const auto In           = makeKeyword(Token::Keyword::In, "in");
        const auto Out          = makeKeyword(Token::Keyword::Out, "out");
        const auto Reference    = makeKeyword(Token::Keyword::Reference, "ref");
        const auto Null         = makeKeyword(Token::Keyword::Null, "null");
        const auto Const        = makeKeyword(Token::Keyword::Const, "const");
        const auto Underscore   = makeKeyword(Token::Keyword::Underscore, "_");
        const auto Try          = makeKeyword(Token::Keyword::Try, "try");
        const auto Catch        = makeKeyword(Token::Keyword::Catch, "catch");
        const auto Import       = makeKeyword(Token::Keyword::Import, "import");
        const auto As           = makeKeyword(Token::Keyword::As, "as");
        const auto Is           = makeKeyword(Token::Keyword::Is, "is");

    }

    namespace Literal {

        inline Token makeIdentifier(const std::string &name) {
            return makeToken(core::Token::Type::Identifier, Token::Identifier(name));
        }

        inline Token makeNumeric(const Token::Literal &value) {
            return makeToken(core::Token::Type::Integer, value);
        }

        inline Token makeString(const std::string &value) {
            return makeToken(core::Token::Type::String, Token::Literal(value));
        }

        inline Token makeDocComment(bool global, const std::string &value) {
            return { core::Token::Type::DocComment, Token::DocComment { global, value }, Location::Empty() };
        }

        inline Token makeComment(const std::string &value) {
            return { core::Token::Type::Comment, Token::Comment { value }, Location::Empty() };
        }

        const auto Identifier = makeToken(core::Token::Type::Identifier, { });
        const auto Numeric    = makeToken(core::Token::Type::Integer, { });
        const auto String     = makeToken(core::Token::Type::String,  { });

    }

    namespace Operator {

        constexpr u8 maxOperatorLength = 2;

        inline Token makeOperator(const Token::Operator& op, const std::string_view& name) {
            auto token = makeToken(core::Token::Type::Operator, op);
            Token::Operators()[name] = token;
            return token;
        }

        const auto Plus                     = makeOperator(Token::Operator::Plus, "+");
        const auto Minus                    = makeOperator(Token::Operator::Minus, "-");
        const auto Star                     = makeOperator(Token::Operator::Star, "*");
        const auto Slash                    = makeOperator(Token::Operator::Slash, "/");
        const auto Percent                  = makeOperator(Token::Operator::Percent, "%");
        // ls and rs are composed in the parser due to ambiguity with recursive templates
        const auto BitAnd                   = makeOperator(Token::Operator::BitAnd, "&");
        const auto BitOr                    = makeOperator(Token::Operator::BitOr, "|");
        const auto BitXor                   = makeOperator(Token::Operator::BitXor, "^");
        const auto BitNot                   = makeOperator(Token::Operator::BitNot, "~");
        const auto BoolEqual                = makeOperator(Token::Operator::BoolEqual, "==");
        const auto BoolNotEqual             = makeOperator(Token::Operator::BoolNotEqual, "!=");
        const auto BoolLessThan             = makeOperator(Token::Operator::BoolLessThan, "<");
        const auto BoolGreaterThan          = makeOperator(Token::Operator::BoolGreaterThan, ">");
        // ltoe and gtoe are also handled in the parser due to ambiguity with ls assignment
        const auto BoolAnd                  = makeOperator(Token::Operator::BoolAnd, "&&");
        const auto BoolOr                   = makeOperator(Token::Operator::BoolOr, "||");
        const auto BoolNot                  = makeOperator(Token::Operator::BoolNot, "!");
        const auto BoolXor                  = makeOperator(Token::Operator::BoolXor, "^^");
        const auto Dollar                   = makeOperator(Token::Operator::Dollar, "$");
        const auto Colon                    = makeOperator(Token::Operator::Colon, ":");
        const auto ScopeResolution          = makeOperator(Token::Operator::ScopeResolution, "::");
        const auto TernaryConditional       = makeOperator(Token::Operator::TernaryConditional, "?");
        const auto At                       = makeOperator(Token::Operator::At, "@");
        const auto Assign                   = makeOperator(Token::Operator::Assign, "=");

        const auto AddressOf                = makeOperator(Token::Operator::AddressOf, "addressof");
        const auto SizeOf                   = makeOperator(Token::Operator::SizeOf, "sizeof");
        const auto TypeNameOf               = makeOperator(Token::Operator::TypeNameOf, "typenameof");

    }

    namespace ValueType {

        inline Token makeValueType(const Token::ValueType& value, const std::string_view& name) {
            auto token = makeToken(core::Token::Type::ValueType, value);
            Token::Types()[name] = token;
            return token;
        }

        const auto Padding          = makeValueType(Token::ValueType::Padding, "padding");
        const auto Auto             = makeValueType(Token::ValueType::Auto, "auto");
        const auto Any              = makeValueType(Token::ValueType::Any, "any");

        const auto Unsigned8Bit     = makeValueType(Token::ValueType::Unsigned8Bit, "u8");
        const auto Unsigned16Bit    = makeValueType(Token::ValueType::Unsigned16Bit, "u16");
        const auto Unsigned24Bit    = makeValueType(Token::ValueType::Unsigned24Bit, "u24");
        const auto Unsigned32Bit    = makeValueType(Token::ValueType::Unsigned32Bit, "u32");
        const auto Unsigned48Bit    = makeValueType(Token::ValueType::Unsigned48Bit, "u48");
        const auto Unsigned64Bit    = makeValueType(Token::ValueType::Unsigned64Bit, "u64");
        const auto Unsigned96Bit    = makeValueType(Token::ValueType::Unsigned96Bit, "u96");
        const auto Unsigned128Bit   = makeValueType(Token::ValueType::Unsigned128Bit, "u128");

        const auto Signed8Bit       = makeValueType(Token::ValueType::Signed8Bit, "s8");
        const auto Signed16Bit      = makeValueType(Token::ValueType::Signed16Bit, "s16");
        const auto Signed24Bit      = makeValueType(Token::ValueType::Signed24Bit, "s24");
        const auto Signed32Bit      = makeValueType(Token::ValueType::Signed32Bit, "s32");
        const auto Signed48Bit      = makeValueType(Token::ValueType::Signed48Bit, "s48");
        const auto Signed64Bit      = makeValueType(Token::ValueType::Signed64Bit, "s64");
        const auto Signed96Bit      = makeValueType(Token::ValueType::Signed96Bit, "s96");
        const auto Signed128Bit     = makeValueType(Token::ValueType::Signed128Bit, "s128");

        const auto Float            = makeValueType(Token::ValueType::Float, "float");
        const auto Double           = makeValueType(Token::ValueType::Double, "double");

        const auto Boolean          = makeValueType(Token::ValueType::Boolean, "bool");

        const auto Character        = makeValueType(Token::ValueType::Character, "char");
        const auto Character16      = makeValueType(Token::ValueType::Character16, "char16");
        const auto String           = makeValueType(Token::ValueType::String, "str");

        // non named
        const auto Unsigned         = makeToken(Token::Type::ValueType, Token::ValueType::Unsigned);
        const auto Signed           = makeToken(Token::Type::ValueType, Token::ValueType::Signed);
        const auto FloatingPoint    = makeToken(Token::Type::ValueType, Token::ValueType::FloatingPoint);
        const auto Integer          = makeToken(Token::Type::ValueType, Token::ValueType::Integer);
        const auto CustomType       = makeToken(Token::Type::ValueType, Token::ValueType::CustomType);

    }

    namespace Separator {

        inline Token makeSeparator(const Token::Separator& value, char name) {
            auto token = makeToken(Token::Type::Separator, value);
            Token::Separators()[name] = token;
            return token;
        }

        const auto LeftParenthesis = makeSeparator(Token::Separator::LeftParenthesis, '(');
        const auto RightParenthesis = makeSeparator(Token::Separator::RightParenthesis, ')');
        const auto LeftBrace = makeSeparator(Token::Separator::LeftBrace, '{');
        const auto RightBrace = makeSeparator(Token::Separator::RightBrace, '}');
        const auto LeftBracket = makeSeparator(Token::Separator::LeftBracket, '[');
        const auto RightBracket = makeSeparator(Token::Separator::RightBracket, ']');
        const auto Comma = makeSeparator(Token::Separator::Comma, ',');
        const auto Dot = makeSeparator(Token::Separator::Dot, '.');
        const auto Semicolon = makeSeparator(Token::Separator::Semicolon, ';');

        const auto EndOfProgram = makeToken(Token::Type::Separator, Token::Separator::EndOfProgram);

    }

    namespace Directive {

        inline Token makeDirective(const Token::Directive& value, const std::string_view& name) {
            auto token = makeToken(core::Token::Type::Directive, value);
            Token::Directives()[name] = token;
            return token;
        }

        const auto Include = makeDirective(Token::Directive::Include, "#include");
        const auto Define = makeDirective(Token::Directive::Define, "#define");
        const auto Undef = makeDirective(Token::Directive::Undef, "#undef");
        const auto IfDef = makeDirective(Token::Directive::IfDef, "#ifdef");
        const auto IfNDef = makeDirective(Token::Directive::IfNDef, "#ifndef");
        const auto EndIf = makeDirective(Token::Directive::EndIf, "#endif");
        const auto Error = makeDirective(Token::Directive::Error, "#error");
        const auto Pragma = makeDirective(Token::Directive::Pragma, "#pragma");
    }

}