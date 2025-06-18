#pragma once

#include <pl/core/token.hpp>

namespace pl::core::tkn {

    static const inline std::map<std::string_view, Token::Literal> constants = {
            { "true", Token::Literal(true) },
            { "false", Token::Literal(false) },
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

        inline const auto If           = makeKeyword(Token::Keyword::If, "if");
        inline const auto Else         = makeKeyword(Token::Keyword::Else, "else");
        inline const auto While        = makeKeyword(Token::Keyword::While, "while");
        inline const auto For          = makeKeyword(Token::Keyword::For, "for");
        inline const auto Match        = makeKeyword(Token::Keyword::Match, "match");
        inline const auto Return       = makeKeyword(Token::Keyword::Return, "return");
        inline const auto Break        = makeKeyword(Token::Keyword::Break, "break");
        inline const auto Continue     = makeKeyword(Token::Keyword::Continue, "continue");
        inline const auto Struct       = makeKeyword(Token::Keyword::Struct, "struct");
        inline const auto Enum         = makeKeyword(Token::Keyword::Enum, "enum");
        inline const auto Union        = makeKeyword(Token::Keyword::Union, "union");
        inline const auto Function     = makeKeyword(Token::Keyword::Function, "fn");
        inline const auto Bitfield     = makeKeyword(Token::Keyword::Bitfield, "bitfield");
        inline const auto Unsigned     = makeKeyword(Token::Keyword::Unsigned, "unsigned");
        inline const auto Signed       = makeKeyword(Token::Keyword::Signed, "signed");
        inline const auto LittleEndian = makeKeyword(Token::Keyword::LittleEndian, "le");
        inline const auto BigEndian    = makeKeyword(Token::Keyword::BigEndian, "be");
        inline const auto Parent       = makeKeyword(Token::Keyword::Parent, "parent");
        inline const auto Namespace    = makeKeyword(Token::Keyword::Namespace, "namespace");
        inline const auto Using        = makeKeyword(Token::Keyword::Using, "using");
        inline const auto This         = makeKeyword(Token::Keyword::This, "this");
        inline const auto In           = makeKeyword(Token::Keyword::In, "in");
        inline const auto Out          = makeKeyword(Token::Keyword::Out, "out");
        inline const auto Reference    = makeKeyword(Token::Keyword::Reference, "ref");
        inline const auto Null         = makeKeyword(Token::Keyword::Null, "null");
        inline const auto Const        = makeKeyword(Token::Keyword::Const, "const");
        inline const auto Underscore   = makeKeyword(Token::Keyword::Underscore, "_");
        inline const auto Try          = makeKeyword(Token::Keyword::Try, "try");
        inline const auto Catch        = makeKeyword(Token::Keyword::Catch, "catch");
        inline const auto Import       = makeKeyword(Token::Keyword::Import, "import");
        inline const auto As           = makeKeyword(Token::Keyword::As, "as");
        inline const auto Is           = makeKeyword(Token::Keyword::Is, "is");
        inline const auto From         = makeKeyword(Token::Keyword::From, "from");

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

        inline Token makeDocComment(bool global,bool singleLine, const std::string &value) {
            return { Token::Type::DocComment, Token::DocComment { global, singleLine, value }, Location::Empty() };
        }

        inline Token makeComment(bool singleLine, const std::string &value) {
            return { Token::Type::Comment, Token::Comment { singleLine,  value }, Location::Empty() };
        }

        inline const auto Identifier = makeToken(core::Token::Type::Identifier, { });
        inline const auto Numeric    = makeToken(core::Token::Type::Integer, { });
        inline const auto String     = makeToken(core::Token::Type::String,  { });
        inline const auto DocComment = makeToken(core::Token::Type::DocComment, { });
        inline const auto Comment    = makeToken(core::Token::Type::Comment, { });

    }

    namespace Operator {

        constexpr u8 maxOperatorLength = 2;

        inline Token makeOperator(const Token::Operator& op, const std::string_view& name) {
            auto token = makeToken(core::Token::Type::Operator, op);
            Token::Operators()[name] = token;
            return token;
        }

        inline const auto Plus                     = makeOperator(Token::Operator::Plus, "+");
        inline const auto Minus                    = makeOperator(Token::Operator::Minus, "-");
        inline const auto Star                     = makeOperator(Token::Operator::Star, "*");
        inline const auto Slash                    = makeOperator(Token::Operator::Slash, "/");
        inline const auto Percent                  = makeOperator(Token::Operator::Percent, "%");
        // ls and rs are composed in the parser due to ambiguity with recursive templates
        inline const auto BitAnd                   = makeOperator(Token::Operator::BitAnd, "&");
        inline const auto BitOr                    = makeOperator(Token::Operator::BitOr, "|");
        inline const auto BitXor                   = makeOperator(Token::Operator::BitXor, "^");
        inline const auto BitNot                   = makeOperator(Token::Operator::BitNot, "~");
        inline const auto BoolEqual                = makeOperator(Token::Operator::BoolEqual, "==");
        inline const auto BoolNotEqual             = makeOperator(Token::Operator::BoolNotEqual, "!=");
        inline const auto BoolLessThan             = makeOperator(Token::Operator::BoolLessThan, "<");
        inline const auto BoolGreaterThan          = makeOperator(Token::Operator::BoolGreaterThan, ">");
        // ltoe and gtoe are also handled in the parser due to ambiguity with ls assignment
        inline const auto BoolAnd                  = makeOperator(Token::Operator::BoolAnd, "&&");
        inline const auto BoolOr                   = makeOperator(Token::Operator::BoolOr, "||");
        inline const auto BoolNot                  = makeOperator(Token::Operator::BoolNot, "!");
        inline const auto BoolXor                  = makeOperator(Token::Operator::BoolXor, "^^");
        inline const auto Dollar                   = makeOperator(Token::Operator::Dollar, "$");
        inline const auto Colon                    = makeOperator(Token::Operator::Colon, ":");
        inline const auto ScopeResolution          = makeOperator(Token::Operator::ScopeResolution, "::");
        inline const auto TernaryConditional       = makeOperator(Token::Operator::TernaryConditional, "?");
        inline const auto At                       = makeOperator(Token::Operator::At, "@");
        inline const auto Assign                   = makeOperator(Token::Operator::Assign, "=");

        inline const auto AddressOf                = makeOperator(Token::Operator::AddressOf, "addressof");
        inline const auto SizeOf                   = makeOperator(Token::Operator::SizeOf, "sizeof");
        inline const auto TypeNameOf               = makeOperator(Token::Operator::TypeNameOf, "typenameof");

    }

    namespace ValueType {

        inline Token makeValueType(const Token::ValueType& value, const std::string_view& name) {
            auto token = makeToken(core::Token::Type::ValueType, value);
            Token::Types()[name] = token;
            return token;
        }

        inline const auto Padding          = makeValueType(Token::ValueType::Padding, "padding");
        inline const auto Auto             = makeValueType(Token::ValueType::Auto, "auto");
        inline const auto Any              = makeValueType(Token::ValueType::Any, "any");

        inline const auto Unsigned8Bit     = makeValueType(Token::ValueType::Unsigned8Bit, "u8");
        inline const auto Unsigned16Bit    = makeValueType(Token::ValueType::Unsigned16Bit, "u16");
        inline const auto Unsigned24Bit    = makeValueType(Token::ValueType::Unsigned24Bit, "u24");
        inline const auto Unsigned32Bit    = makeValueType(Token::ValueType::Unsigned32Bit, "u32");
        inline const auto Unsigned48Bit    = makeValueType(Token::ValueType::Unsigned48Bit, "u48");
        inline const auto Unsigned64Bit    = makeValueType(Token::ValueType::Unsigned64Bit, "u64");
        inline const auto Unsigned96Bit    = makeValueType(Token::ValueType::Unsigned96Bit, "u96");
        inline const auto Unsigned128Bit   = makeValueType(Token::ValueType::Unsigned128Bit, "u128");

        inline const auto Signed8Bit       = makeValueType(Token::ValueType::Signed8Bit, "s8");
        inline const auto Signed16Bit      = makeValueType(Token::ValueType::Signed16Bit, "s16");
        inline const auto Signed24Bit      = makeValueType(Token::ValueType::Signed24Bit, "s24");
        inline const auto Signed32Bit      = makeValueType(Token::ValueType::Signed32Bit, "s32");
        inline const auto Signed48Bit      = makeValueType(Token::ValueType::Signed48Bit, "s48");
        inline const auto Signed64Bit      = makeValueType(Token::ValueType::Signed64Bit, "s64");
        inline const auto Signed96Bit      = makeValueType(Token::ValueType::Signed96Bit, "s96");
        inline const auto Signed128Bit     = makeValueType(Token::ValueType::Signed128Bit, "s128");

        inline const auto Float            = makeValueType(Token::ValueType::Float, "float");
        inline const auto Double           = makeValueType(Token::ValueType::Double, "double");

        inline const auto Boolean          = makeValueType(Token::ValueType::Boolean, "bool");

        inline const auto Character        = makeValueType(Token::ValueType::Character, "char");
        inline const auto Character16      = makeValueType(Token::ValueType::Character16, "char16");
        inline const auto String           = makeValueType(Token::ValueType::String, "str");

        // non named
        inline const auto Unsigned         = makeToken(Token::Type::ValueType, Token::ValueType::Unsigned);
        inline const auto Signed           = makeToken(Token::Type::ValueType, Token::ValueType::Signed);
        inline const auto FloatingPoint    = makeToken(Token::Type::ValueType, Token::ValueType::FloatingPoint);
        inline const auto Integer          = makeToken(Token::Type::ValueType, Token::ValueType::Integer);
        inline const auto CustomType       = makeToken(Token::Type::ValueType, Token::ValueType::CustomType);

    }

    namespace Separator {

        inline Token makeSeparator(const Token::Separator& value, char name) {
            auto token = makeToken(Token::Type::Separator, value);
            Token::Separators()[name] = token;
            return token;
        }

        inline const auto LeftParenthesis = makeSeparator(Token::Separator::LeftParenthesis, '(');
        inline const auto RightParenthesis = makeSeparator(Token::Separator::RightParenthesis, ')');
        inline const auto LeftBrace = makeSeparator(Token::Separator::LeftBrace, '{');
        inline const auto RightBrace = makeSeparator(Token::Separator::RightBrace, '}');
        inline const auto LeftBracket = makeSeparator(Token::Separator::LeftBracket, '[');
        inline const auto RightBracket = makeSeparator(Token::Separator::RightBracket, ']');
        inline const auto Comma = makeSeparator(Token::Separator::Comma, ',');
        inline const auto Dot = makeSeparator(Token::Separator::Dot, '.');
        inline const auto Semicolon = makeSeparator(Token::Separator::Semicolon, ';');

        inline const auto EndOfProgram = makeToken(Token::Type::Separator, Token::Separator::EndOfProgram);

    }

    namespace Directive {

        inline Token makeDirective(const Token::Directive& value, const std::string_view& name) {
            auto token = makeToken(core::Token::Type::Directive, value);
            Token::Directives()[name] = token;
            return token;
        }

        inline const auto Include = makeDirective(Token::Directive::Include, "#include");
        inline const auto Define = makeDirective(Token::Directive::Define, "#define");
        inline const auto Undef = makeDirective(Token::Directive::Undef, "#undef");
        inline const auto IfDef = makeDirective(Token::Directive::IfDef, "#ifdef");
        inline const auto IfNDef = makeDirective(Token::Directive::IfNDef, "#ifndef");
        inline const auto EndIf = makeDirective(Token::Directive::EndIf, "#endif");
        inline const auto Error = makeDirective(Token::Directive::Error, "#error");
        inline const auto Pragma = makeDirective(Token::Directive::Pragma, "#pragma");
    }

}