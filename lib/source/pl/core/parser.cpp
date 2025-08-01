#include <pl/core/parser.hpp>
#include <pl/core/tokens.hpp>

#include <pl/core/ast/ast_node_array_variable_decl.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_bitfield.hpp>
#include <pl/core/ast/ast_node_bitfield_array_variable_decl.hpp>
#include <pl/core/ast/ast_node_bitfield_field.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>
#include <pl/core/ast/ast_node_cast.hpp>
#include <pl/core/ast/ast_node_compound_statement.hpp>
#include <pl/core/ast/ast_node_conditional_statement.hpp>
#include <pl/core/ast/ast_node_control_flow_statement.hpp>
#include <pl/core/ast/ast_node_enum.hpp>
#include <pl/core/ast/ast_node_function_call.hpp>
#include <pl/core/ast/ast_node_function_definition.hpp>
#include <pl/core/ast/ast_node_imported_type.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_lvalue_assignment.hpp>
#include <pl/core/ast/ast_node_mathematical_expression.hpp>
#include <pl/core/ast/ast_node_multi_variable_decl.hpp>
#include <pl/core/ast/ast_node_match_statement.hpp>
#include <pl/core/ast/ast_node_pointer_variable_decl.hpp>
#include <pl/core/ast/ast_node_rvalue.hpp>
#include <pl/core/ast/ast_node_rvalue_assignment.hpp>
#include <pl/core/ast/ast_node_scope_resolution.hpp>
#include <pl/core/ast/ast_node_struct.hpp>
#include <pl/core/ast/ast_node_ternary_expression.hpp>
#include <pl/core/ast/ast_node_try_catch_statement.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_type_operator.hpp>
#include <pl/core/ast/ast_node_union.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_while_statement.hpp>

#include <stdexcept>
#include <wolv/utils/string.hpp>

#include <optional>

/**
 * Match only needs to be used when compound statements are involved.
 * This is because the parser will automatically reset if a match fails.
 */
#define MATCHES(x) (begin() && resetIfFailed(x))

// Definition syntax:
// [A]          : Either A or no token
// [A|B]        : Either A, B or no token
// <A|B>        : Either A or B
// <A...>       : One or more of A
// A B C        : Sequence of tokens A then B then C
// (parseXXXX)  : Parsing handled by other function
namespace pl::core {

    template<template<typename ...> typename SmartPointer, typename T>
    static std::vector<SmartPointer<T>> unwrapSafePointerVector(std::vector<hlp::SafePointer<SmartPointer, T>> &&vec) {
        std::vector<SmartPointer<T>> result;
        result.reserve(vec.size());

        for (auto &ptr : vec) {
            result.emplace_back(std::move(ptr.unwrap()));
        }

        return result;
    }

    /* Mathematical expressions */
    // ([(parseMathematicalExpression)|<(parseMathematicalExpression),...>(parseMathematicalExpression)]
    std::vector<hlp::safe_unique_ptr<ast::ASTNode>> Parser::parseParameters() {
        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> params;

        while (!sequence(tkn::Separator::RightParenthesis)) {
            auto param = parseMathematicalExpression();
            if (param == nullptr)
                continue;

            params.push_back(std::move(param));

            if (sequence(tkn::Separator::Comma, tkn::Separator::RightParenthesis)) {
                error("Expected ')' at end of parameter list, got {}.", getFormattedToken(0));
                break;
            }
            if (sequence(tkn::Separator::RightParenthesis))
                break;
            if (!sequence(tkn::Separator::Comma)) {
                error("Expected ',' in-between parameters, got {}.", getFormattedToken(0));
                break;
            }
        }

        return params;
    }

    // Identifier(<parseParameters>)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseFunctionCall() {
        std::string functionName = parseNamespaceResolution();
        auto *functionIdentifier = std::get_if<Token::Identifier>(&m_curr[-1].value);
        if (functionIdentifier != nullptr)
            functionIdentifier->setType(Token::Identifier::IdentifierType::Function);

        if (!sequence(tkn::Separator::LeftParenthesis)) {
            error("Expected '(' after function name, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto params = parseParameters();

        return create<ast::ASTNodeFunctionCall>(functionName, unwrapSafePointerVector(std::move(params)));
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseStringLiteral() {
        return create<ast::ASTNodeLiteral>(getValue<Token::Literal>(-1));
    }

    std::string Parser::parseNamespaceResolution() {
        std::string name;

        while (true) {
            name += getValue<Token::Identifier>(-1).get();
            auto namespaceIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
            if(m_autoNamespace == name) {
                // replace auto namespace with alias namespace
                name = m_aliasNamespaceString;
            }

            if (sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier)) {
                if (namespaceIdentifier != nullptr)
                    namespaceIdentifier->setType(Token::Identifier::IdentifierType::ScopeResolutionUnknown);
                name += "::";
                continue;
            }

            break;
        }

        return name;
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseScopeResolution() {
        std::string typeName;

        while (true) {
            typeName += getValue<Token::Identifier>(-1).get();

            if (sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier)) {
                if (peek(tkn::Operator::ScopeResolution, 0) && peek(tkn::Literal::Identifier, 1)) {
                    auto namespaceIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
                    if (namespaceIdentifier != nullptr)
                        namespaceIdentifier->setType(Token::Identifier::IdentifierType::ScopeResolutionUnknown);
                    typeName += "::";
                    continue;
                }
                if (this->m_types.contains(typeName))
                    return create<ast::ASTNodeScopeResolution>(this->m_types[typeName].unwrap(), getValue<Token::Identifier>(-1).get());
                for (auto &potentialName : getNamespacePrefixedNames(typeName)) {
                    if (this->m_types.contains(potentialName)) {
                        return create<ast::ASTNodeScopeResolution>(this->m_types[potentialName].unwrap(), getValue<Token::Identifier>(-1).get());
                    }
                }

                error("No namespace with this name found.");
                return nullptr;
            }

            break;
        }

        errorDesc("Invalid scope resolution.", "Expected statement in the form of 'NamespaceA::NamespaceB::TypeName'.");
        return nullptr;
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseRValue() {
        ast::ASTNodeRValue::Path path;
        return this->parseRValue(path);
    }

    // <Identifier[.]...>
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseRValue(ast::ASTNodeRValue::Path &path) {
        if (peek(tkn::Literal::Identifier, -1)) {
            path.emplace_back(getValue<Token::Identifier>(-1).get());

            auto identifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
            if (identifier != nullptr) {
                if (m_currTemplateType.empty())
                    identifier->setType(Token::Identifier::IdentifierType::FunctionUnknown);
                else
                    identifier->setType(Token::Identifier::IdentifierType::MemberUnknown);
            }

        } else if (peek(tkn::Keyword::Parent, -1))
            path.emplace_back("parent");
        else if (peek(tkn::Keyword::This, -1))
            path.emplace_back("this");
        else if (peek(tkn::Operator::Dollar, -1))
            path.emplace_back("$");
        else if (peek(tkn::Keyword::Null, -1))
            path.emplace_back("null");

        if (MATCHES(sequence(tkn::Separator::LeftBracket) && !peek(tkn::Separator::LeftBracket))) {
            path.emplace_back(std::move(parseMathematicalExpression().unwrap()));
            if (!sequence(tkn::Separator::RightBracket)) {
                error("Expected ']' at end of array indexing, got {}.", getFormattedToken(0));
                return nullptr;
            }
        }

        if (sequence(tkn::Separator::Dot)) {
            if (oneOf(tkn::Literal::Identifier, tkn::Keyword::Parent))
                return this->parseRValue(path);

            errorDesc("Invalid member access, expected variable identifier or parent keyword.", {});
            return nullptr;
        }
        return create<ast::ASTNodeRValue>(std::move(path));
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseRValueAssignment() {
        auto lhs = parseRValue();

        if (!sequence(tkn::Operator::Assign)) {
            errorHere("Expected value after '=' in variable assignment, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto rhs = parseMathematicalExpression();
        if (rhs == nullptr)
            return nullptr;

        return create<ast::ASTNodeRValueAssignment>(std::move(lhs), std::move(rhs));
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseUserDefinedLiteral(hlp::safe_unique_ptr<ast::ASTNode> &&literal) {
        std::vector<std::unique_ptr<ast::ASTNode>> params;
        params.emplace_back(std::move(literal.unwrap()));

        auto udlFunctionName = parseNamespaceResolution();
        if (!udlFunctionName.starts_with('_')) {
            errorDesc("Invalid use of user defined literal. User defined literals need to start with a underscore character (_).", {});
            return nullptr;
        }

        if (sequence(tkn::Separator::LeftParenthesis)) {
            auto extraParams = parseParameters();
            for (auto &param : extraParams) {
                params.emplace_back(std::move(param.unwrap()));
            }
        }

        return create<ast::ASTNodeFunctionCall>(udlFunctionName, std::move(params));
    }

    // <Integer|((parseMathematicalExpression))>
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseFactor() {
        if (sequence(tkn::Literal::Numeric)) {
            auto valueNode = create<ast::ASTNodeLiteral>(getValue<Token::Literal>(-1));
            if (sequence(tkn::Literal::Identifier)) {
                return parseUserDefinedLiteral(std::move(valueNode));
            } else {
                return valueNode;
            }
        }
        if (oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::BoolNot, tkn::Operator::BitNot))
            return this->parseMathematicalExpression();
        if (sequence(tkn::Separator::LeftParenthesis)) {
            auto node = this->parseMathematicalExpression();
            if (!sequence(tkn::Separator::RightParenthesis)) {
                error("Mismatched '(' in mathematical expression.");
                return nullptr;
            }

            return node;
        }
        if (sequence(tkn::Literal::Identifier)) {
            const auto originalPos = this->m_curr;
            parseNamespaceResolution();

            const bool isFunction = peek(tkn::Separator::LeftParenthesis);
            this->m_curr    = originalPos;

            if (isFunction) {
                return this->parseFunctionCall();
            }
            if (peek(tkn::Operator::ScopeResolution, 0)) {
                return this->parseScopeResolution();
            }
            return this->parseRValue();
        }
        if (oneOf(tkn::Keyword::Parent, tkn::Keyword::This, tkn::Operator::Dollar, tkn::Keyword::Null)) {
            return this->parseRValue();
        }
        if (MATCHES(oneOf(tkn::Operator::AddressOf, tkn::Operator::SizeOf, tkn::Operator::TypeNameOf) && sequence(tkn::Separator::LeftParenthesis))) {
            auto op = getValue<Token::Operator>(-2);

            hlp::safe_unique_ptr<ast::ASTNode> result;

            if (oneOf(tkn::Literal::Identifier)) {
                const auto startToken = this->m_curr;
                if (op == Token::Operator::SizeOf || op == Token::Operator::TypeNameOf) {
                    if (auto type = getCustomType(parseNamespaceResolution()); type != nullptr) {
                        parseCustomTypeParameters(type);
                        result = create<ast::ASTNodeTypeOperator>(op, std::move(type));
                    }
                }

                if (result == nullptr) {
                    this->m_curr = startToken;
                    auto rvalue = this->parseRValue();
                    if (rvalue == nullptr)
                        return nullptr;

                    result = create<ast::ASTNodeTypeOperator>(op, std::move(rvalue));
                }
            } else if (oneOf(tkn::Keyword::Parent, tkn::Keyword::This)) {
                auto rvalue = this->parseRValue();
                if (rvalue == nullptr)
                    return nullptr;

                result = create<ast::ASTNodeTypeOperator>(op, std::move(rvalue));
            } else if (op == Token::Operator::SizeOf && sequence(tkn::ValueType::Any)) {
                const auto type = getValue<Token::ValueType>(-1);

                result = create<ast::ASTNodeLiteral>(u128(Token::getTypeSize(type)));
            } else if (op == Token::Operator::TypeNameOf && sequence(tkn::ValueType::Any)) {
                const auto type = getValue<Token::ValueType>(-1);

                result = create<ast::ASTNodeLiteral>(std::string(Token::getTypeName(type)));
            } else if (sequence(tkn::Operator::Dollar)) {
                result = create<ast::ASTNodeTypeOperator>(op);
            } else {
                if (op == Token::Operator::SizeOf)
                    error("Expected rvalue, type or '$' operator.");
                if (op == Token::Operator::AddressOf)
                    error("Expected rvalue or '$' operator.");
                if (op == Token::Operator::TypeNameOf)
                    error("Expected rvalue or type.");
                return nullptr;
            }

            if (!sequence(tkn::Separator::RightParenthesis)) {
                error("Mismatched '(' of type operator expression.");
                return nullptr;
            }

            return result;
        }

        error("Expected value, got {}.", getFormattedToken(0));
        next();
        return nullptr;
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseReinterpretExpression() {
        auto value = parseFactor();

        if (sequence(tkn::Keyword::As)) {
            auto type = parseType();

            return create<ast::ASTNodeCast>(std::move(value), std::move(type), true);
        } else {
            return value;
        }
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseCastExpression() {
        if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any)) {
            auto type        = parseType();
            if (type == nullptr)
                return nullptr;

            auto builtinType = dynamic_cast<ast::ASTNodeBuiltinType *>(type->getType().get());

            if (builtinType == nullptr) {
                error("Cannot use non-built-in type in cast expression.");
                return nullptr;
            }

            if (!peek(tkn::Separator::LeftParenthesis)) {
                error("Expected '(' after type cast, got {}.", getFormattedToken(0));
                return nullptr;
            }

            auto node = parseFactor();
            if (node == nullptr)
                return nullptr;

            return create<ast::ASTNodeCast>(std::move(node), std::move(type), false);
        }

        return parseReinterpretExpression();
    }

    // <+|-|!|~> (parseFactor)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseUnaryExpression() {
        if (oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::BoolNot, tkn::Operator::BitNot)) {
            auto op = getValue<Token::Operator>(-1);
            auto node = this->parseUnaryExpression();
            if (node == nullptr)
                return nullptr;

            return create<ast::ASTNodeMathematicalExpression>(create<ast::ASTNodeLiteral>(i128(0)), std::move(node), op);
        }

        if (sequence(tkn::Literal::String)) {
            auto literal = this->parseStringLiteral();
            if (sequence(tkn::Literal::Identifier)) {
                return parseUserDefinedLiteral(std::move(literal));
            } else {
                return literal;
            }
        }

        return this->parseCastExpression();
    }

    // (parseUnaryExpression) <*|/|%> (parseUnaryExpression)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseMultiplicativeExpression() {
        auto node = this->parseUnaryExpression();
        if (node == nullptr)
            return nullptr;

        while (oneOf(tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent)) {
            auto op = getValue<Token::Operator>(-1);
            auto other = this->parseUnaryExpression();
            if (other == nullptr)
                return nullptr;

            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), op);
        }

        return node;
    }

    // (parseMultiplicativeExpression) <+|-> (parseMultiplicativeExpression)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseAdditiveExpression() {
        auto node = this->parseMultiplicativeExpression();
        if (node == nullptr)
            return nullptr;

        while (variant(tkn::Operator::Plus, tkn::Operator::Minus)) {
            auto op = getValue<Token::Operator>(-1);
            auto other = this->parseMultiplicativeExpression();
            if (other == nullptr)
                return nullptr;

            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), op);
        }

        return node;
    }

    // (parseAdditiveExpression) < >>|<< > (parseAdditiveExpression)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseShiftExpression(const bool inTemplate) {
        auto node = this->parseAdditiveExpression();
        if (node == nullptr)
            return nullptr;

        if (inTemplate && peek(tkn::Operator::BoolGreaterThan))
            return node;

        while (true) {
            if (sequence(tkn::Operator::BoolGreaterThan, tkn::Operator::BoolGreaterThan)) {
                auto other = this->parseAdditiveExpression();
                if(other == nullptr)
                    return nullptr;

                node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::RightShift);
            } else if (sequence(tkn::Operator::BoolLessThan, tkn::Operator::BoolLessThan)) {
                auto other = this->parseAdditiveExpression();
                if(other == nullptr)
                    return nullptr;

                node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::LeftShift);
            } else {
                break;
            }
        }

        return node;
    }

    // (parseShiftExpression) & (parseShiftExpression)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseBinaryAndExpression(const bool inTemplate) {
        auto node = this->parseShiftExpression(inTemplate);
        if (node == nullptr)
            return nullptr;

        while (sequence(tkn::Operator::BitAnd)) {
            auto other = this->parseShiftExpression(inTemplate);
            if (other == nullptr)
                return nullptr;

            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::BitAnd);
        }

        return node;
    }

    // (parseBinaryAndExpression) ^ (parseBinaryAndExpression)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseBinaryXorExpression(const bool inTemplate) {
        auto node = this->parseBinaryAndExpression(inTemplate);
        if (node == nullptr)
            return nullptr;

        while (sequence(tkn::Operator::BitXor)) {
            auto other = this->parseBinaryAndExpression(inTemplate);
            if (other == nullptr)
                return nullptr;

            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::BitXor);
        }

        return node;
    }

    // (parseBinaryXorExpression) | (parseBinaryXorExpression)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseBinaryOrExpression(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseBinaryXorExpression(inTemplate);
        if (node == nullptr)
            return nullptr;

        if (inMatchRange && peek(tkn::Operator::BitOr))
            return node;
        while (sequence(tkn::Operator::BitOr)) {
            auto other = this->parseBinaryXorExpression(inTemplate);
            if (other == nullptr)
                return nullptr;

            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::BitOr);
        }

        return node;
    }

    // (parseBinaryOrExpression) < >=|<=|>|< > (parseBinaryOrExpression)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseRelationExpression(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseBinaryOrExpression(inTemplate, inMatchRange);
        if (node == nullptr)
            return nullptr;

        if (inTemplate && peek(tkn::Operator::BoolGreaterThan))
            return node;

        while (true) {
            if (sequence(tkn::Operator::BoolGreaterThan, tkn::Operator::Assign)) {
                auto other = this->parseBinaryOrExpression(inTemplate, inMatchRange);
                if (other == nullptr)
                    return nullptr;

                node = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::BoolGreaterThanOrEqual);
            } else if (sequence(tkn::Operator::BoolLessThan, tkn::Operator::Assign)) {
                auto other = this->parseBinaryOrExpression(inTemplate, inMatchRange);
                if (other == nullptr)
                    return nullptr;

                node = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::BoolLessThanOrEqual);
            }
            else if (sequence(tkn::Operator::BoolGreaterThan)) {
                auto other = this->parseBinaryOrExpression(inTemplate, inMatchRange);
                if (other == nullptr)
                    return nullptr;

                node = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::BoolGreaterThan);
            }
            else if (sequence(tkn::Operator::BoolLessThan)) {
                auto other = this->parseBinaryOrExpression(inTemplate, inMatchRange);
                if (other == nullptr)
                    return nullptr;

                node = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::BoolLessThan);
            }
            else
                break;
        }

        return node;
    }

    // (parseRelationExpression) <==|!=> (parseRelationExpression)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseEqualityExpression(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseRelationExpression(inTemplate, inMatchRange);
        if (node == nullptr)
            return nullptr;

        while (MATCHES(sequence(tkn::Operator::BoolEqual) || sequence(tkn::Operator::BoolNotEqual))) {
            auto op = getValue<Token::Operator>(-1);
            auto other = this->parseRelationExpression(inTemplate, inMatchRange);
            if (other == nullptr)
                return nullptr;

            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), op);
        }

        return node;
    }

    // (parseEqualityExpression) && (parseEqualityExpression)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseBooleanAnd(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseEqualityExpression(inTemplate, inMatchRange);
        if (node == nullptr)
            return nullptr;

        while (sequence(tkn::Operator::BoolAnd)) {
            auto other = this->parseEqualityExpression(inTemplate, inMatchRange);
            if (other == nullptr)
                return nullptr;

            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::BoolAnd);
        }

        return node;
    }

    // (parseBooleanAnd) ^^ (parseBooleanAnd)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseBooleanXor(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseBooleanAnd(inTemplate, inMatchRange);
        if (node == nullptr)
            return nullptr;

        while (sequence(tkn::Operator::BoolXor)) {
            auto other = this->parseBooleanAnd(inTemplate, inMatchRange);
            if (other == nullptr)
                return nullptr;

            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::BoolXor);
        }

        return node;
    }

    // (parseBooleanXor) || (parseBooleanXor)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseBooleanOr(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseBooleanXor(inTemplate, inMatchRange);
        if (node == nullptr)
            return nullptr;

        while (sequence(tkn::Operator::BoolOr)) {
            auto other = this->parseBooleanXor(inTemplate, inMatchRange);
            if (other == nullptr)
                return nullptr;

            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), std::move(other), Token::Operator::BoolOr);
        }

        return node;
    }

    // (parseBooleanOr) ? (parseBooleanOr) : (parseBooleanOr)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseTernaryConditional(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseBooleanOr(inTemplate, inMatchRange);
        if (node == nullptr)
            return nullptr;

        while (sequence(tkn::Operator::TernaryConditional)) {
            auto second = this->parseBooleanOr(inTemplate, inMatchRange);

            if (!sequence(tkn::Operator::Colon)) {
                error("Expected ':' after ternary condition, got {}", getFormattedToken(0));
                return nullptr;
            }

            auto third = this->parseBooleanOr(inTemplate, inMatchRange);
            if (second == nullptr || third == nullptr)
                return nullptr;

            node = create<ast::ASTNodeTernaryExpression>(std::move(node), std::move(second), std::move(third), Token::Operator::TernaryConditional);
        }

        return node;
    }

    // (parseTernaryConditional)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseMathematicalExpression(const bool inTemplate, const bool inMatchRange) {
        return this->parseTernaryConditional(inTemplate, inMatchRange);
    }

    // [[ <Identifier[( (parseStringLiteral) )], ...> ]]
    void Parser::parseAttribute(ast::Attributable *currNode) {
        if (currNode == nullptr) {
            errorDesc("Cannot use attribute here.", "Attributes can only be applied after type or variable definitions.");
            return;
        }

        do {
            if (!sequence(tkn::Literal::Identifier)) {
                error("Expected attribute instruction name, got {}", getFormattedToken(0));
                return;
            }

            auto attributeIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
            if (attributeIdentifier != nullptr)
                attributeIdentifier->setType(Token::Identifier::IdentifierType::Attribute);

            auto attribute = parseNamespaceResolution();

            if (sequence(tkn::Separator::LeftParenthesis)) {
                std::vector<hlp::safe_unique_ptr<ast::ASTNode>> args;
                do {
                    auto expression = parseMathematicalExpression();
                    if (expression == nullptr)
                        continue;

                    args.push_back(std::move(expression));
                } while (sequence(tkn::Separator::Comma));

                if (!sequence(tkn::Separator::RightParenthesis)) {
                    error("Expected ')', got {}", getFormattedToken(0));
                    return;
                }

                currNode->addAttribute(create<ast::ASTNodeAttribute>(attribute, unwrapSafePointerVector(std::move(args)), m_aliasNamespaceString, m_autoNamespace));
            } else
                currNode->addAttribute(create<ast::ASTNodeAttribute>(attribute));
        } while (sequence(tkn::Separator::Comma));

        if (!sequence(tkn::Separator::RightBracket, tkn::Separator::RightBracket))
            error("Expected ']]' after attribute, got {}.", getFormattedToken(0));
    }

    /* Functions */

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseFunctionDefinition() {
        const auto &functionName = getValue<Token::Identifier>(-1).get();

        auto functionIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
        if (functionIdentifier != nullptr)
            functionIdentifier->setType(Token::Identifier::IdentifierType::Function);

        std::vector<std::pair<std::string, hlp::safe_unique_ptr<ast::ASTNode>>> params;
        std::optional<std::string> parameterPack;

        if (!sequence(tkn::Separator::LeftParenthesis)) {
            error("Expected '(' after function declaration, got {}.", getFormattedToken(0));
        }

        // Parse parameter list
        const bool hasParams        = !peek(tkn::Separator::RightParenthesis);
        u32 unnamedParamCount = 0;
        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> defaultParameters;

        while (hasParams) {
            if (sequence(tkn::ValueType::Auto, tkn::Separator::Dot, tkn::Separator::Dot, tkn::Separator::Dot, tkn::Literal::Identifier)) {
                parameterPack = getValue<Token::Identifier>(-1).get();

                if (sequence(tkn::Separator::Comma))
                    error("Parameter pack can only appear at the end of the parameter list.");

                break;
            }

            auto type = parseType();
            if (type == nullptr)
                return nullptr;

            if (sequence(tkn::Literal::Identifier)) {
                params.emplace_back(getValue<Token::Identifier>(-1).get(), std::move(type));
                auto argumentIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
                if (argumentIdentifier != nullptr)
                    argumentIdentifier->setType(Token::Identifier::IdentifierType::FunctionParameter);
            } else {
                params.emplace_back(std::to_string(unnamedParamCount), std::move(type));
                unnamedParamCount++;
            }

            if (sequence(tkn::Operator::Assign)) {
                // Parse default parameters
                auto expression = parseMathematicalExpression();
                if (expression == nullptr)
                    return nullptr;

                defaultParameters.push_back(std::move(expression));
            } else {
                if (!defaultParameters.empty()) {
                    error("Expected default argument value for parameter '{}', got {}.", params.back().first,  getFormattedToken(0));
                    break;
                }
            }

            if (!sequence(tkn::Separator::Comma)) {
                break;
            }
        }

        if (!sequence(tkn::Separator::RightParenthesis)) {
            error("Expected ')' after parameter list, got {}.", getFormattedToken(0));
            return nullptr;
        }

        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' after function head, got {}.", getFormattedToken(0));
            return nullptr;
        }


        // Parse function body
        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> body;

        while (!sequence(tkn::Separator::RightBrace)) {
            auto statement = parseFunctionStatement();
            if (statement == nullptr)
                continue;

            body.push_back(std::move(statement));
        }

        std::vector<std::pair<std::string, std::unique_ptr<ast::ASTNode>>> unwrappedParams;
        for (auto &[name, node] : params) {
            unwrappedParams.emplace_back(std::move(name), std::move(node.unwrap()));
        }

        return create<ast::ASTNodeFunctionDefinition>(getNamespacePrefixedNames(functionName).back(), std::move(unwrappedParams), unwrapSafePointerVector(std::move(body)), parameterPack, unwrapSafePointerVector(std::move(defaultParameters)));
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseArrayInitExpression(std::string identifier) {
        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' after array assignment, got {}.", getFormattedToken(0));
            return nullptr;
        }

        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> values;
        while (true) {
            if (sequence(tkn::Separator::RightBrace)) {
                break;
            }

            auto expression = parseMathematicalExpression();
            if (expression == nullptr)
                continue;

            values.push_back(std::move(expression));

            if (sequence(tkn::Separator::Comma)) {
                if (sequence(tkn::Separator::RightBrace)) {
                    error("Expected value after ',' in array assignment, got {}.", getFormattedToken(0));
                    return nullptr;
                }
            } else if (sequence(tkn::Separator::RightBrace)) {
                break;
            } else {
                error("Expected ',' or '}}' in array assignment, got {}.", getFormattedToken(0));
                return nullptr;
            }
        }

        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> result;
        for (u64 i = 0; i < values.size(); i += 1) {
            ast::ASTNodeRValue::Path path;
            path.emplace_back(identifier);
            path.emplace_back(std::unique_ptr<ast::ASTNode>(new ast::ASTNodeLiteral(u128(i))));

            auto lvalue = create<ast::ASTNodeRValue>(std::move(path));
            auto &rvalue = values[i];

            result.emplace_back(create<ast::ASTNodeRValueAssignment>(std::move(lvalue), std::move(rvalue)));
        }

        return create<ast::ASTNodeCompoundStatement>(unwrapSafePointerVector(std::move(result)));
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseFunctionVariableDecl(const bool constant) {
        hlp::safe_unique_ptr<ast::ASTNode> statement;
        auto type = parseType();
        if (type == nullptr)
            return nullptr;

        if (sequence(tkn::Literal::Identifier)) {
            auto identifier = getValue<Token::Identifier>(-1).get();
            auto functionIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));

            if (MATCHES(sequence(tkn::Separator::LeftBracket) && !peek(tkn::Separator::LeftBracket))) {
                statement = parseMemberArrayVariable(std::move(type), constant);

                if (sequence(tkn::Operator::Assign)) {
                    std::vector<hlp::safe_unique_ptr<ast::ASTNode>> compoundStatement;
                    {
                        compoundStatement.emplace_back(std::move(statement));

                        auto initStatement = parseArrayInitExpression(identifier);
                        if (initStatement == nullptr)
                            return nullptr;

                        compoundStatement.emplace_back(std::move(initStatement));
                    }
                    if (functionIdentifier != nullptr)
                            functionIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
                    statement = create<ast::ASTNodeCompoundStatement>(unwrapSafePointerVector(std::move(compoundStatement)));
                } else {
                    if (functionIdentifier != nullptr) {
                        if (peek(tkn::Operator::At, 0))
                            functionIdentifier->setType(Token::Identifier::IdentifierType::View);
                        else
                            functionIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
                    }
                }
            } else {
                statement = parseMemberVariable(std::move(type), constant, identifier);

                if (sequence(tkn::Operator::Assign)) {
                    auto expression = parseMathematicalExpression();
                    if (expression == nullptr)
                        return nullptr;

                    std::vector<hlp::safe_unique_ptr<ast::ASTNode>> compoundStatement;
                    {
                        compoundStatement.emplace_back(std::move(statement));
                        compoundStatement.emplace_back(create<ast::ASTNodeLValueAssignment>(identifier, std::move(expression)));
                    }
                    if (functionIdentifier != nullptr)
                        functionIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);

                    statement = create<ast::ASTNodeCompoundStatement>(unwrapSafePointerVector(std::move(compoundStatement)));
                } else {
                    if (functionIdentifier != nullptr) {
                        if (peek(tkn::Operator::At, 0))
                            functionIdentifier->setType(Token::Identifier::IdentifierType::View);
                        else
                            functionIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
                    }
                }
            }
        } else {
            error("Expected identifier in variable declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        return statement;
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseFunctionStatement(bool needsSemicolon) {
        hlp::safe_unique_ptr<ast::ASTNode> statement;

        if (sequence(tkn::Literal::Identifier, tkn::Operator::Assign))
            statement = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (sequence(tkn::Operator::Dollar, tkn::Operator::Assign))
            statement = parseFunctionVariableAssignment("$");
        else if (const auto identifierOffset = parseCompoundAssignment(tkn::Literal::Identifier); identifierOffset.has_value())
            statement = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(*identifierOffset).get());
        else if (parseCompoundAssignment(tkn::Operator::Dollar).has_value())
            statement = parseFunctionVariableCompoundAssignment("$");
        else if (oneOf(tkn::Keyword::Return, tkn::Keyword::Break, tkn::Keyword::Continue))
            statement = parseFunctionControlFlowStatement();
        else if (sequence(tkn::Keyword::If)) {
            statement      = parseConditional([&]() { return parseFunctionStatement(); });
            needsSemicolon = false;
        } else if (sequence(tkn::Keyword::Match)) {
            statement      = parseMatchStatement([&]() { return parseFunctionStatement(); });
            needsSemicolon = false;
        } else if (sequence(tkn::Keyword::Try, tkn::Separator::LeftBrace)) {
            statement      = parseTryCatchStatement([&]() { return parseFunctionStatement(); });
            needsSemicolon = false;
        } else if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)) {
            statement      = parseFunctionWhileLoop();
            needsSemicolon = false;
        } else if (sequence(tkn::Keyword::For, tkn::Separator::LeftParenthesis)) {
            statement      = parseFunctionForLoop();
            needsSemicolon = false;
        } else if (MATCHES(sequence(tkn::Literal::Identifier) && (peek(tkn::Separator::Dot) || (peek(tkn::Separator::LeftBracket, 0) && !peek(tkn::Separator::LeftBracket, 1))))) {
            statement = parseRValueAssignment();
        } else if (sequence(tkn::Literal::Identifier)) {
            const auto originalPos = this->m_curr;
            parseNamespaceResolution();

            if (peek(tkn::Separator::LeftParenthesis)) { // is function
                this->m_curr = originalPos;
                statement    = parseFunctionCall();
            } else {
                this->m_curr = originalPos - 1;
                statement    = parseFunctionVariableDecl();
            }
        } else if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any)) {
            statement = parseFunctionVariableDecl();
        } else if (sequence(tkn::Keyword::Const)) {
            statement = parseFunctionVariableDecl(true);
        } else {
            errorHere("Invalid function statement.");
            next();
            return nullptr;
        }

        if (needsSemicolon && !sequence(tkn::Separator::Semicolon)) {
            error("Expected ';' at end of statement, got {}.", getFormattedToken(0));
            return nullptr;
        }

        // Consume superfluous semicolons
        while (needsSemicolon && sequence(tkn::Separator::Semicolon))
            ;

        return statement;
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseFunctionVariableAssignment(const std::string &lvalue) {
        auto rvalue = this->parseMathematicalExpression();
        if (rvalue == nullptr)
            return nullptr;

        return create<ast::ASTNodeLValueAssignment>(lvalue, std::move(rvalue));
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseFunctionVariableCompoundAssignment(const std::string &lvalue) {
        auto op = getValue<Token::Operator>(-2);

        if (op == Token::Operator::BoolLessThan)
            op = Token::Operator::LeftShift;
        else if (op == Token::Operator::BoolGreaterThan)
            op = Token::Operator::RightShift;

        auto rvalue = this->parseMathematicalExpression();
        if (rvalue == nullptr)
            return nullptr;

        return create<ast::ASTNodeLValueAssignment>(lvalue, create<ast::ASTNodeMathematicalExpression>(create<ast::ASTNodeRValue>(hlp::moveToVector<ast::ASTNodeRValue::PathSegment>(lvalue)), std::move(rvalue), op));
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseFunctionControlFlowStatement() {
        ControlFlowStatement type;
        if (peek(tkn::Keyword::Return, -1))
            type = ControlFlowStatement::Return;
        else if (peek(tkn::Keyword::Break, -1))
            type = ControlFlowStatement::Break;
        else if (peek(tkn::Keyword::Continue, -1))
            type = ControlFlowStatement::Continue;
        else {
            errorDesc("Invalid control flow statement.", "Control flow statements include 'return', 'break' and 'continue'.");
            return nullptr;
        }

        if (peek(tkn::Separator::Semicolon))
            return create<ast::ASTNodeControlFlowStatement>(type, nullptr);
        if (type == ControlFlowStatement::Return) {
            auto expression = this->parseMathematicalExpression();
            if (expression == nullptr)
                return nullptr;

            return create<ast::ASTNodeControlFlowStatement>(type, std::move(expression));
        }

        error("Return value can only be passed to a 'return' statement.");
        return nullptr;
    }

    std::vector<hlp::safe_unique_ptr<ast::ASTNode>> Parser::parseStatementBody(const std::function<hlp::safe_unique_ptr<ast::ASTNode>()> &memberParser) {
        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> body;

        if (sequence(tkn::Separator::LeftBrace)) {
            while (!sequence(tkn::Separator::RightBrace)) {
                auto member = memberParser();
                if (member == nullptr)
                    return {};
                body.push_back(std::move(member));
            }
        } else {
            auto member = memberParser();
            if (member == nullptr)
                return {};
            body.push_back(std::move(member));
        }

        return body;
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseFunctionWhileLoop() {
        auto condition = parseMathematicalExpression();
        if (condition == nullptr)
            return nullptr;

        if (!sequence(tkn::Separator::RightParenthesis)) {
            error("Expected ')' at end of while head, got {}.", getFormattedToken(0));
            return nullptr;
        }

        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> body = parseStatementBody([&] { return parseFunctionStatement(); });

        return create<ast::ASTNodeWhileStatement>(std::move(condition), unwrapSafePointerVector(std::move(body)));
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseFunctionForLoop() {
        auto preExpression = parseFunctionStatement(false);

        if (!sequence(tkn::Separator::Comma)) {
            error("Expected ',' after for loop expression, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto condition = parseMathematicalExpression();
        if (condition == nullptr)
            return nullptr;

        if (!sequence(tkn::Separator::Comma)) {
            error("Expected ',' after for loop expression, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto postExpression = parseFunctionStatement(false);

        if (!sequence(tkn::Separator::RightParenthesis)) {
            error("Expected ')' at end of for loop head, got {}.", getFormattedToken(0));
            return nullptr;
        }

        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> body = parseStatementBody([&] { return parseFunctionStatement(); });

        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> compoundStatement;
        {
            compoundStatement.emplace_back(std::move(preExpression));
            compoundStatement.emplace_back(create<ast::ASTNodeWhileStatement>(std::move(condition), unwrapSafePointerVector(std::move(body)), std::move(postExpression)));
        }

        return create<ast::ASTNodeCompoundStatement>(unwrapSafePointerVector(std::move(compoundStatement)), true);
    }

    /* Control flow */

    // if ((parseMathematicalExpression)) { (parseMember) }
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseConditional(const std::function<hlp::safe_unique_ptr<ast::ASTNode>()> &memberParser) {
        if (!sequence(tkn::Separator::LeftParenthesis)) {
            error("Expected '(' after 'if', got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto condition = parseMathematicalExpression();
        if (condition == nullptr)
            return nullptr;

        if (!sequence(tkn::Separator::RightParenthesis)) {
            error("Expected ')' after if head, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto trueBody = parseStatementBody(memberParser);

        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> falseBody;
        if (sequence(tkn::Keyword::Else))
            falseBody = parseStatementBody(memberParser);

        return create<ast::ASTNodeConditionalStatement>(std::move(condition), unwrapSafePointerVector(std::move(trueBody)), unwrapSafePointerVector(std::move(falseBody)));
    }

    std::pair<hlp::safe_unique_ptr<ast::ASTNode>, bool> Parser::parseCaseParameters(const std::vector<hlp::safe_unique_ptr<ast::ASTNode>> &matchParameters) {
        hlp::safe_unique_ptr<ast::ASTNode> condition = nullptr;

        size_t caseIndex = 0;
        bool isDefault = true;
        while (!sequence(tkn::Separator::RightParenthesis)) {
            if (caseIndex >= matchParameters.size()) {
                error("Size of case parameters bigger than size of match condition.");
                break;
            }

            hlp::safe_unique_ptr<ast::ASTNode> currentCondition = nullptr;
            if (sequence(tkn::Keyword::Underscore)) {
                // if '_' is found, act as wildcard, push literal(true)
                currentCondition = std::make_unique<ast::ASTNodeLiteral>(true);
            } else {
                isDefault = false;
                auto &param = matchParameters[caseIndex];

                do {
                    auto first = parseMathematicalExpression(false, true);
                    if (first == nullptr)
                        continue;

                    auto nextCondition = [&]() -> hlp::safe_unique_ptr<ast::ASTNode> {
                        if (sequence(tkn::Separator::Dot, tkn::Separator::Dot, tkn::Separator::Dot)) {
                            // range a ... b should compile to
                            // param >= a && param <= b
                            auto last = parseMathematicalExpression(false, true);
                            if (last == nullptr)
                                return nullptr;

                            auto firstCondition = create<ast::ASTNodeMathematicalExpression>(param->clone(), std::move(first), Token::Operator::BoolGreaterThanOrEqual);
                            auto lastCondition = create<ast::ASTNodeMathematicalExpression>(param->clone(), std::move(last), Token::Operator::BoolLessThanOrEqual);
                            return create<ast::ASTNodeMathematicalExpression>(std::move(firstCondition), std::move(lastCondition), Token::Operator::BoolAnd);
                        }

                        // else just compile to param == a
                        return create<ast::ASTNodeMathematicalExpression>(param->clone(), std::move(first), Token::Operator::BoolEqual);
                    }();

                    if (currentCondition == nullptr) {
                        currentCondition = std::move(nextCondition);
                    } else {
                        // we've matched a previous |, add a
                        currentCondition = create<ast::ASTNodeMathematicalExpression>(std::move(currentCondition), std::move(nextCondition), Token::Operator::BoolOr);
                    }
                } while (sequence(tkn::Operator::BitOr));
            }

            if (condition == nullptr) {
                condition = std::move(currentCondition);
            } else {
                condition = create<ast::ASTNodeMathematicalExpression>(
                        std::move(condition), std::move(currentCondition), Token::Operator::BoolAnd);
            }

            caseIndex++;
            if (sequence(tkn::Separator::Comma, tkn::Separator::RightParenthesis)) {
                error("Expected ')' at end of parameter list, got {}.", getFormattedToken(0));
                return {};
            }
            if (sequence(tkn::Separator::RightParenthesis))
                break;
            if (!sequence(tkn::Separator::Comma)) {
                error("Expected ',' in-between parameters, got {}.", getFormattedToken(0));
                return {};
            }
        }

        if (caseIndex != matchParameters.size()) {
            error("Size of case parameters smaller than size of match condition.");
            return {};
        }

        return { std::move(condition), isDefault };
    }

    // match ((parseParameters)) { (parseParameters { (parseMember) })*, default { (parseMember) } }
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseMatchStatement(const std::function<hlp::safe_unique_ptr<ast::ASTNode>()> &memberParser) {
        if (!sequence(tkn::Separator::LeftParenthesis)) {
            error("Expected '(' after 'match', got {}.", getFormattedToken(0));
            return nullptr;
        }

        const auto condition = parseParameters();

        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' after match head, got {}.", getFormattedToken(0));
            return nullptr;
        }

        std::vector<ast::MatchCase> cases;
        std::optional<ast::MatchCase> defaultCase;

        while (!sequence(tkn::Separator::RightBrace)) {
            if (!sequence(tkn::Separator::LeftParenthesis)) {
                error("Expected '(', got {}.", getFormattedToken(0));
                break;
            }

            auto [caseCondition, isDefault] = parseCaseParameters(condition);
            if (caseCondition == nullptr)
                return nullptr;

            if (!sequence(tkn::Operator::Colon)) {
                error("Expected ':' after case condition, got {}.", getFormattedToken(0));
                break;
            }

            auto body = parseStatementBody(memberParser);

            if (isDefault)
                defaultCase = ast::MatchCase(std::move(caseCondition), unwrapSafePointerVector(std::move(body)));
            else
                cases.emplace_back(std::move(caseCondition), unwrapSafePointerVector(std::move(body)));

            if (sequence(tkn::Separator::RightBrace))
                break;
        }

        return create<ast::ASTNodeMatchStatement>(std::move(cases), std::move(defaultCase));
    }

    // try { (parseMember) } catch { (parseMember) }
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseTryCatchStatement(const std::function<hlp::safe_unique_ptr<ast::ASTNode>()> &memberParser) {
        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> tryBody, catchBody;
        while (!sequence(tkn::Separator::RightBrace)) {
            auto member = memberParser();
            if (member == nullptr)
                continue;

            tryBody.emplace_back(std::move(member));
        }

        if (sequence(tkn::Keyword::Catch)) {
            if (!sequence(tkn::Separator::LeftBrace)) {
                error("Expected '{{' after catch, got {}.", getFormattedToken(0));
                return nullptr;
            }

            while (!sequence(tkn::Separator::RightBrace)) {
                auto member = memberParser();
                if (member == nullptr)
                    continue;

                catchBody.emplace_back(std::move(member));
            }
        }


        return create<ast::ASTNodeTryCatchStatement>(unwrapSafePointerVector(std::move(tryBody)), unwrapSafePointerVector(std::move(catchBody)));
    }

    // while ((parseMathematicalExpression))
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseWhileStatement() {
        auto condition = parseMathematicalExpression();
        if (condition == nullptr)
            return nullptr;

        if (!sequence(tkn::Separator::RightParenthesis)) {
            error("Expected ')' after while head, got {}.", getFormattedToken(0));
            return nullptr;
        }

        return create<ast::ASTNodeWhileStatement>(std::move(condition), std::vector<std::unique_ptr<ast::ASTNode>>{});
    }

    /* Type declarations */

    hlp::safe_unique_ptr<ast::ASTNodeTypeDecl> Parser::getCustomType(const std::string &baseTypeName) {
        if (!this->m_currTemplateType.empty()) {
            for (const auto &templateParameter : this->m_currTemplateType.front()->getTemplateParameters()) {
                if (const auto templateType = dynamic_cast<ast::ASTNodeTypeDecl*>(templateParameter.get()); templateType != nullptr)
                    if (templateType->getName() == baseTypeName)
                        return create<ast::ASTNodeTypeDecl>("", templateParameter);
            }
        }

        for (const auto &typeName : getNamespacePrefixedNames(baseTypeName)) {
            if (this->m_types.contains(typeName)) {
                auto type = this->m_types[typeName];

                if (type == nullptr)
                    return nullptr;

                if (type->isValid() && type->getType() == nullptr) {
                    return nullptr;
                }

                return create<ast::ASTNodeTypeDecl>("", type.unwrapUnchecked());
            }
        }

        return nullptr;
    }

    // <Identifier[, Identifier]>
    void Parser::parseCustomTypeParameters(hlp::safe_unique_ptr<ast::ASTNodeTypeDecl> &type) {
        if (const auto actualType = dynamic_cast<ast::ASTNodeTypeDecl*>(type->getType().get()); actualType != nullptr)
            if (const auto &templateTypes = actualType->getTemplateParameters(); !templateTypes.empty()) {
                if (!sequence(tkn::Operator::BoolLessThan)) {
                    error("Cannot use template type without template parameters.");
                    return;
                }

                u32 index = 0;
                do {
                    if (index >= templateTypes.size()) {
                        error("Provided more template parameters than expected. Type only has {} parameters", templateTypes.size());
                        return;
                    }

                    auto parameter = templateTypes[index];
                    if (const auto typeDecl = dynamic_cast<ast::ASTNodeTypeDecl*>(parameter.get()); typeDecl != nullptr) {
                        auto newType = parseType();
                        if (newType == nullptr)
                            return;
                        if (newType->isForwardDeclared()) {
                            error("Cannot use forward declared type as template parameter.");
                        }

                        typeDecl->setType(std::move(newType), true);
                        typeDecl->setName("");
                    } else if (const auto value = dynamic_cast<ast::ASTNodeLValueAssignment*>(parameter.get()); value != nullptr) {
                        auto rvalue = parseMathematicalExpression(true);
                        if (rvalue == nullptr)
                            return;

                        value->setRValue(std::move(rvalue));
                    } else {
                        error("Invalid template parameter type.");
                        return;
                    }

                    index++;
                } while (sequence(tkn::Separator::Comma));

                if (index < templateTypes.size()) {
                    error("Not enough template parameters provided, expected {} parameters.", templateTypes.size());
                    return;
                }

                if (!sequence(tkn::Operator::BoolGreaterThan)) {
                    error("Expected '>' to close template list, got {}.", getFormattedToken(0));
                    return;
                }

                type = hlp::safe_unique_ptr<ast::ASTNodeTypeDecl>(dynamic_cast<ast::ASTNodeTypeDecl*>(type->clone().release()));
            }
    }

    // Identifier
    hlp::safe_unique_ptr<ast::ASTNodeTypeDecl> Parser::parseCustomType() {
        auto baseTypeName = parseNamespaceResolution();
        auto type = getCustomType(baseTypeName);

        if (type == nullptr) {
            errorDesc(fmt::format("Type {} has not been declared yet.", baseTypeName), fmt::format("If this type is being declared further down in the code, consider forward declaring it with 'using {};'.", baseTypeName));
            return nullptr;
        }

        parseCustomTypeParameters(type);

        return type;
    }

    // [be|le] <Identifier|u8|u16|u24|u32|u48|u64|u96|u128|s8|s16|s24|s32|s48|s64|s96|s128|float|double|str>
    hlp::safe_unique_ptr<ast::ASTNodeTypeDecl> Parser::parseType() {
        const bool reference = sequence(tkn::Keyword::Reference);

        std::optional<std::endian> endian;
        if (sequence(tkn::Keyword::LittleEndian))
            endian = std::endian::little;
        else if (sequence(tkn::Keyword::BigEndian))
            endian = std::endian::big;

        hlp::safe_unique_ptr<ast::ASTNodeTypeDecl> result = nullptr;
        if (sequence(tkn::Literal::Identifier)) {    // Custom type

            result = parseCustomType();
        } else if (sequence(tkn::ValueType::Any)) {    // Builtin type
            auto type = getValue<Token::ValueType>(-1);
            result = create<ast::ASTNodeTypeDecl>(Token::getTypeName(type), create<ast::ASTNodeBuiltinType>(type));
        }

        if (result == nullptr) {
            error("Invalid type. Expected built-in type or custom type name, got {}.", getFormattedToken(0));
            return nullptr;
        }

        result->setReference(reference);
        if (endian.has_value())
            result->setEndian(endian.value());

        return result;
    }

    // <(parseType), ...>
    std::vector<hlp::safe_shared_ptr<ast::ASTNode>> Parser::parseTemplateList() {
        std::vector<hlp::safe_shared_ptr<ast::ASTNode>> result;

        if (sequence(tkn::Operator::BoolLessThan)) {
            do {
                if (sequence(tkn::Literal::Identifier)) {
                    result.emplace_back(createShared<ast::ASTNodeTypeDecl>(getValue<Token::Identifier>(-1).get()));
                    auto templateIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
                    if (templateIdentifier != nullptr)
                        templateIdentifier->setType(Token::Identifier::IdentifierType::TemplateArgument);
                } else if (sequence(tkn::ValueType::Auto, tkn::Literal::Identifier)) {
                    result.emplace_back(createShared<ast::ASTNodeLValueAssignment>(getValue<Token::Identifier>(-1).get(), nullptr));
                    auto templateIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
                    if (templateIdentifier != nullptr)
                        templateIdentifier->setType(Token::Identifier::IdentifierType::TemplateArgument);
                }
                else {
                    error("Expected identifier for template type, got {}.", getFormattedToken(0));
                    return { };
                }
            } while (sequence(tkn::Separator::Comma));

            if (!sequence(tkn::Operator::BoolGreaterThan)) {
                error("Expected '>' after template declaration, got {}.", getFormattedToken(0));
                return { };
            }
        }

        return result;
    }

    // import (String | ( Identifier [dot Identifier] )) (parseNamespaceResolution)? (as parseNamespaceResolution)?
    hlp::safe_shared_ptr<ast::ASTNode> Parser::parseImportStatement() {
        std::string path;

        bool importAll = false;
        if (sequence(tkn::Operator::Star)) {
            if (!sequence(tkn::Keyword::From)) {
                error("Expected 'from' after import *.");
                return nullptr;
            }

            importAll = true;
        }

        if (sequence(tkn::Literal::String)) {
            path = std::get<std::string>(getValue<Token::Literal>(-1));
        } else if (sequence(tkn::Literal::Identifier)) {
            path = getValue<Token::Identifier>(-1).get();

            while (sequence(tkn::Separator::Dot, tkn::Literal::Identifier))
                path += "/" + getValue<Token::Identifier>(-1).get();
        } else {
            error("Expected string or identifier after 'import', got {}.", getFormattedToken(0));
        }

        if (importAll) {
            if (!sequence(tkn::Keyword::As)) {
                error("Alias name required for import *.");
                return nullptr;
            }

            if (!sequence(tkn::Literal::Identifier)) {
                error("Expected identifier after 'as', got {}.", getFormattedToken(0));
                return nullptr;
            }

            const auto importName = getValue<Token::Identifier>(-1).get();

            return addType(importName, create<ast::ASTNodeImportedType>(path));
        }

        // TODO: struct import

        std::string alias;

        if (sequence(tkn::Keyword::As)) {
            if (!sequence(tkn::Literal::Identifier)) {
                error("Expected identifier after 'as', got {}.", getFormattedToken(0));
                return nullptr;
            }

            alias = parseNamespaceResolution();
        }

        const auto& resolver = m_parserManager->getResolver();
        if (!resolver) {
            errorDesc("No valid resolver set", "This is a parser bug, please report it.");
            return nullptr;
        }

        auto [resolvedSource, resolverErrors] = resolver(path);
        if (!resolverErrors.empty()) {
            for (const auto & resolverError : resolverErrors)
                error("Failed to resolve import: {}", resolverError);
            return nullptr;
        }

        auto [parsedData, parserErrors] = m_parserManager->parse(resolvedSource.value(), alias);
        if (!parserErrors.empty()) {
            for (auto & parserError : parserErrors)
                error(parserError); // rethrow
            return nullptr;
        }

        // Merge type definitions together
        for (auto &[typeName, typeDecl] : parsedData.value().types)
            this->m_types[typeName] = std::move(typeDecl);

        // Use ast in a virtual compound statement
        return create<ast::ASTNodeCompoundStatement>(std::move(parsedData.value().astNodes), false);
    }


    // using Identifier = (parseType)
    hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> Parser::parseUsingDeclaration() {
        const auto name = getValue<Token::Identifier>(-1).get();
        auto typedefIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
        if (typedefIdentifier != nullptr)
            typedefIdentifier->setType(Token::Identifier::IdentifierType::Typedef);

        auto templateList = this->parseTemplateList();

        if (!sequence(tkn::Operator::Assign)) {
            error("Expected '=' after using declaration type name, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto type = addType(name, nullptr);
        if (type == nullptr)
            return nullptr;

        type->setTemplateParameters(unwrapSafePointerVector(std::move(templateList)));

        this->m_currTemplateType.push_back(type);

        auto replaceType = parseType();
        if (replaceType == nullptr)
            return nullptr;

        this->m_currTemplateType.pop_back();


        if (const auto containedType = replaceType.get(); containedType != nullptr && !containedType->isTemplateType())
            replaceType->setType(containedType->clone());

        const auto endian = replaceType->getEndian();
        type->setType(std::move(replaceType));

        if (endian.has_value())
            type->setEndian(*endian);

        return type;
    }

    // padding[(parseMathematicalExpression)]
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parsePadding() {
        hlp::safe_unique_ptr<ast::ASTNode> size;
        if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
            size = parseWhileStatement();
        else
            size = parseMathematicalExpression();

        if (size == nullptr)
            return nullptr;

        if (!sequence(tkn::Separator::RightBracket)) {
            error("Expected ']' at end of array declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        return create<ast::ASTNodeArrayVariableDecl>("$padding$", createShared<ast::ASTNodeTypeDecl>("", createShared<ast::ASTNodeBuiltinType>(Token::ValueType::Padding)), std::move(size));
    }

    // (parseType) Identifier
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseMemberVariable(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type, bool constant, const std::string &identifier) {
        auto memberIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));

        if (peek(tkn::Separator::Comma)) {

            std::vector<hlp::safe_shared_ptr<ast::ASTNode>> variables;
            if (memberIdentifier != nullptr) {
                if (m_currTemplateType.empty())
                    memberIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
                else
                    memberIdentifier->setType(Token::Identifier::IdentifierType::PatternVariable);
            }
            std::string variableName = identifier;
            do {
                if (sequence(tkn::Literal::Identifier)) {
                    variableName = getValue<Token::Identifier>(-1).get();
                    memberIdentifier = (Token::Identifier *)std::get_if<Token::Identifier>(&((m_curr[-1]).value));
                    if (memberIdentifier != nullptr) {
                        if (m_currTemplateType.empty())
                            memberIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
                        else
                            memberIdentifier->setType(Token::Identifier::IdentifierType::PatternVariable);
                    }
                }
                variables.emplace_back(createShared<ast::ASTNodeVariableDecl>(variableName, type.unwrapUnchecked(), nullptr, nullptr, false, false, constant));
            } while (sequence(tkn::Separator::Comma));

            return create<ast::ASTNodeMultiVariableDecl>(unwrapSafePointerVector(std::move(variables)));
        }

        if (sequence(tkn::Operator::At)) {
            if (constant) {
                errorDesc("Cannot mark placed variable as 'const'.", "Variables placed in memory are always implicitly const.");
            }

            std::string variableName = memberIdentifier == nullptr ? "" : memberIdentifier->get();

            if (memberIdentifier != nullptr) {
                if (m_currTemplateType.empty())
                    memberIdentifier->setType(Token::Identifier::IdentifierType::View);
                else
                    memberIdentifier->setType(Token::Identifier::IdentifierType::CalculatedPointer);
            }
            hlp::safe_unique_ptr<ast::ASTNode> placementSection;
            hlp::safe_unique_ptr<ast::ASTNode> placementOffset = parseMathematicalExpression();
            if (placementOffset == nullptr)
                return nullptr;

            if (sequence(tkn::Keyword::In)) {
                placementSection = parseMathematicalExpression();
                if (placementSection == nullptr)
                    return nullptr;
            }

            return create<ast::ASTNodeVariableDecl>(variableName, type.unwrapUnchecked(), std::move(placementOffset.unwrapUnchecked()), std::move(placementSection.unwrapUnchecked()), false, false, constant);
        }

        if (sequence(tkn::Operator::Assign)) {
            std::vector<hlp::safe_unique_ptr<ast::ASTNode>> compounds;
            compounds.emplace_back(create<ast::ASTNodeVariableDecl>(identifier, type.unwrapUnchecked(), nullptr, create<ast::ASTNodeLiteral>(u128(ptrn::Pattern::PatternLocalSectionId)), false, false, constant));
            auto expression = parseMathematicalExpression();
            if (expression == nullptr)
                return nullptr;

            compounds.emplace_back(create<ast::ASTNodeLValueAssignment>(identifier, std::move(expression)));

            if (memberIdentifier != nullptr) {
                if (m_currTemplateType.empty())
                    memberIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
                else
                    memberIdentifier->setType(Token::Identifier::IdentifierType::LocalVariable);
            }
            return create<ast::ASTNodeCompoundStatement>(unwrapSafePointerVector(std::move(compounds)));
        } else {
            if (constant) {
                errorDesc("Cannot mark placed variable as 'const'.", "Variables placed in memory are always implicitly const.");
            }
        }

        if (memberIdentifier != nullptr) {
            if (m_currTemplateType.empty())
                memberIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
            else
                memberIdentifier->setType(Token::Identifier::IdentifierType::PatternVariable);
        }
        return create<ast::ASTNodeVariableDecl>(identifier, type.unwrapUnchecked(), nullptr, nullptr, false, false, constant);
    }

    // (parseType) Identifier[(parseMathematicalExpression)]
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseMemberArrayVariable(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type, bool constant) {
        auto &nameToken = m_curr[-2];
        auto name = getValue<Token::Identifier>(-2).get();
        auto memberIdentifier = std::get_if<Token::Identifier>(&nameToken.value);

        hlp::safe_unique_ptr<ast::ASTNode> size;

        if (!sequence(tkn::Separator::RightBracket)) {
            if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (size == nullptr)
                return nullptr;

            if (!sequence(tkn::Separator::RightBracket)) {
                error("Expected ']' at end of array declaration, got {}.", getFormattedToken(0));
                return nullptr;
            }
        }

        if (sequence(tkn::Operator::At)) {
            if (constant) {
                errorDesc("Cannot mark placed variable as 'const'.", "Variables placed in memory are always implicitly const.");
            }

            if (memberIdentifier != nullptr) {
                if (m_currTemplateType.empty())
                    memberIdentifier->setType(Token::Identifier::IdentifierType::View);
                else
                    memberIdentifier->setType(Token::Identifier::IdentifierType::CalculatedPointer);
            }
            hlp::safe_unique_ptr<ast::ASTNode> placementSection;
            hlp::safe_unique_ptr<ast::ASTNode> placementOffset = parseMathematicalExpression();
            if (placementOffset == nullptr)
                return nullptr;

            if (sequence(tkn::Keyword::In)) {
                placementSection = parseMathematicalExpression();

                if (placementSection == nullptr)
                    return nullptr;
            }

            return create<ast::ASTNodeArrayVariableDecl>(name, type.unwrapUnchecked(), std::move(size.unwrapUnchecked()), std::move(placementOffset.unwrapUnchecked()), std::move(placementSection.unwrapUnchecked()), constant);
        }

        if (sequence(tkn::Operator::Assign)) {
            auto statement = create<ast::ASTNodeArrayVariableDecl>(name, type.unwrapUnchecked(), std::move(size.unwrapUnchecked()), nullptr, create<ast::ASTNodeLiteral>(u128(ptrn::Pattern::PatternLocalSectionId)), constant);
            std::vector<hlp::safe_unique_ptr<ast::ASTNode>> compoundStatement;
            {
                compoundStatement.emplace_back(std::move(statement));

                auto initStatement = parseArrayInitExpression(name);
                if (initStatement == nullptr)
                    return nullptr;

                compoundStatement.emplace_back(std::move(initStatement));
            }

            if (memberIdentifier != nullptr) {
                if (m_currTemplateType.empty())
                    memberIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
                else
                    memberIdentifier->setType(Token::Identifier::IdentifierType::LocalVariable);
            }
            return create<ast::ASTNodeCompoundStatement>(unwrapSafePointerVector(std::move(compoundStatement)));
        } else {
            if (constant) {
                errorDesc("Cannot mark placed variable as 'const'.", "Variables placed in memory are always implicitly const.");
            }
        }

        if (memberIdentifier != nullptr) {
            if (m_currTemplateType.empty())
                memberIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
            else
                memberIdentifier->setType(Token::Identifier::IdentifierType::PatternVariable);
        }
        return createWithLocation<ast::ASTNodeArrayVariableDecl>(nameToken.location, name, type.unwrapUnchecked(), std::move(size.unwrapUnchecked()), nullptr, nullptr, constant);
    }

    // (parseType) *Identifier : (parseType)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseMemberPointerVariable(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();
        auto memberIdentifier = std::get_if<Token::Identifier>(&((m_curr[-2]).value));

        auto sizeType = parseType();

        if (sizeType == nullptr)
            return nullptr;

        if (sequence(tkn::Operator::At)) {
            auto expression = parseMathematicalExpression();
            if (expression == nullptr)
                return nullptr;
            if (memberIdentifier != nullptr) {
                if (m_currTemplateType.empty())
                    memberIdentifier->setType(Token::Identifier::IdentifierType::View);
                else
                    memberIdentifier->setType(Token::Identifier::IdentifierType::CalculatedPointer);
            }
            return create<ast::ASTNodePointerVariableDecl>(name, type.unwrapUnchecked(), std::move(sizeType), std::move(expression));
        }
        if (memberIdentifier != nullptr) {
            if (m_currTemplateType.empty())
                memberIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
            else
                memberIdentifier->setType(Token::Identifier::IdentifierType::PatternVariable);
        }

        return create<ast::ASTNodePointerVariableDecl>(name, type.unwrapUnchecked(), std::move(sizeType));
    }

    // (parseType) *Identifier[[(parseMathematicalExpression)]]  : (parseType)
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseMemberPointerArrayVariable(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();
        auto memberIdentifier = std::get_if<Token::Identifier>(&((m_curr[-2]).value));

        hlp::safe_unique_ptr<ast::ASTNode> size;

        if (!sequence(tkn::Separator::RightBracket)) {
            if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (size == nullptr)
                return nullptr;

            if (!sequence(tkn::Separator::RightBracket)) {
                error("Expected ']' at end of array declaration, got {}.", getFormattedToken(0));
                return nullptr;
            }
        }

        if (!sequence(tkn::Operator::Colon)) {
            errorDesc("Expected ':' after pointer definition, got {}.", "A pointer requires a integral type to specify its own size.", getFormattedToken(0));
            return nullptr;
        }

        auto sizeType = parseType();
        if (sizeType == nullptr)
            return nullptr;

        auto arrayType = createShared<ast::ASTNodeArrayVariableDecl>("", type.unwrapUnchecked(), std::move(size.unwrapUnchecked()));

        if (sequence(tkn::Operator::At)) {
            auto expression = parseMathematicalExpression();
            if (expression == nullptr)
                return nullptr;
            if (memberIdentifier != nullptr) {
                if (m_currTemplateType.empty())
                    memberIdentifier->setType(Token::Identifier::IdentifierType::View);
                else
                    memberIdentifier->setType(Token::Identifier::IdentifierType::CalculatedPointer);
            }
            return create<ast::ASTNodePointerVariableDecl>(name, std::move(arrayType), std::move(sizeType), std::move(expression));
        }
        if (memberIdentifier != nullptr) {
            if (m_currTemplateType.empty())
                memberIdentifier->setType(Token::Identifier::IdentifierType::FunctionVariable);
            else
                memberIdentifier->setType(Token::Identifier::IdentifierType::PatternVariable);
        }
        return create<ast::ASTNodePointerVariableDecl>(name, std::move(arrayType), std::move(sizeType));
    }

    // [(parsePadding)|(parseMemberVariable)|(parseMemberArrayVariable)|(parseMemberPointerVariable)|(parseMemberArrayPointerVariable)]
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseMember() {
        hlp::safe_unique_ptr<ast::ASTNode> member;

        if (sequence(tkn::Operator::Dollar, tkn::Operator::Assign))
            member = parseFunctionVariableAssignment("$");
        else if (parseCompoundAssignment(tkn::Operator::Dollar).has_value())
            member = parseFunctionVariableCompoundAssignment("$");
        else if (sequence(tkn::Literal::Identifier, tkn::Operator::Assign)) {
            member = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        } else if (const auto identifierOffset = parseCompoundAssignment(tkn::Literal::Identifier); identifierOffset.has_value())
            member = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(*identifierOffset).get());
        else if (MATCHES(sequence(tkn::Literal::Identifier) && (peek(tkn::Separator::Dot) || (peek(tkn::Separator::LeftBracket, 0) && !peek(tkn::Separator::LeftBracket, 1)))))
            member = parseRValueAssignment();
        else if (peek(tkn::Keyword::Const) || peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any) || peek(tkn::Literal::Identifier)) {
            // Some kind of variable definition

            bool isFunction = false;

            if (peek(tkn::Literal::Identifier)) {
                const auto originalPos = this->m_curr;
                ++this->m_curr;
                parseNamespaceResolution();
                isFunction   = peek(tkn::Separator::LeftParenthesis);
                this->m_curr = originalPos;

                if (isFunction) {
                    ++this->m_curr;
                    member = parseFunctionCall();
                }
            }


            if (!isFunction) {
                bool constant = sequence(tkn::Keyword::Const);
                auto type = parseType();
                if (type == nullptr)
                    return nullptr;

                if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Separator::LeftBracket) && sequence<Not>(tkn::Separator::LeftBracket)))
                    member = parseMemberArrayVariable(std::move(type), constant);
                else if (sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Operator::Colon))
                    member = parseMemberPointerVariable(std::move(type));
                else if (sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Separator::LeftBracket))
                    member = parseMemberPointerArrayVariable(std::move(type));
                else if (sequence(tkn::Literal::Identifier)) {
                    auto identifier = getValue<Token::Identifier>(-1).get();
                    member = parseMemberVariable(std::move(type), constant, identifier);
                } else
                    member = parseMemberVariable(std::move(type), constant, "");
            }
        } else if (sequence(tkn::ValueType::Padding, tkn::Separator::LeftBracket))
            member = parsePadding();
        else if (sequence(tkn::Keyword::If))
            return parseConditional([this] { return parseMember(); });
        else if (sequence(tkn::Keyword::Match))
            return parseMatchStatement([this] { return parseMember(); });
        else if (sequence(tkn::Keyword::Try, tkn::Separator::LeftBrace))
            return parseTryCatchStatement([this] { return parseMember(); });
        else if (oneOf(tkn::Keyword::Return, tkn::Keyword::Break, tkn::Keyword::Continue))
            member = parseFunctionControlFlowStatement();
        else {
            errorHere("Invalid struct member definition.");
            next();
            return nullptr;
        }

        if (member == nullptr)
            return nullptr;

        if (sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket))
            parseAttribute(dynamic_cast<ast::Attributable *>(member.get()));

        if (!sequence(tkn::Separator::Semicolon)) {
            error("Expected ';' at end of statement, got {}.", getFormattedToken(0));
            return nullptr;
        }

        // Consume superfluous semicolons
        while (sequence(tkn::Separator::Semicolon))
            ;

        return member;
    }

    // struct Identifier { <(parseMember)...> }
    hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> Parser::parseStruct() {
        const auto &typeName = getValue<Token::Identifier>(-1).get();
        auto userDefinedTypeIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
        if (userDefinedTypeIdentifier != nullptr)
            userDefinedTypeIdentifier->setType(Token::Identifier::IdentifierType::UDT);

        auto typeDecl   = addType(typeName, create<ast::ASTNodeStruct>());
        if(typeDecl == nullptr)
            return nullptr;

        const auto structNode = dynamic_cast<ast::ASTNodeStruct *>(typeDecl->getType().get());
        if (structNode == nullptr)
            return nullptr;

        typeDecl->setTemplateParameters(unwrapSafePointerVector(this->parseTemplateList()));

        this->m_currTemplateType.push_back(typeDecl);

        if (sequence(tkn::Operator::Colon)) {
            // Inheritance
            do {
                if (sequence(tkn::ValueType::Any)) {
                    error("Cannot inherit from built-in type.");
                    return nullptr;
                }
                if (!sequence(tkn::Literal::Identifier)) {
                    error("Expected type to inherit from, got {}.", getFormattedToken(0));
                    return nullptr;
                }

                auto customType = parseCustomType();
                if (customType == nullptr)
                    continue;

                structNode->addInheritance(std::move(customType));
            } while (sequence(tkn::Separator::Comma));
        }

        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' after struct declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        while (!sequence(tkn::Separator::RightBrace)) {
            auto member = parseMember();
            if (hasErrors())
                break;
            else if (member == nullptr)
                continue;

            structNode->addMember(std::move(member));
        }
        this->m_currTemplateType.pop_back();

        return typeDecl;
    }

    // union Identifier { <(parseMember)...> }
    hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> Parser::parseUnion() {
        const auto &typeName = getValue<Token::Identifier>(-1).get();
        auto userDefinedTypeIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
        if (userDefinedTypeIdentifier != nullptr)
            userDefinedTypeIdentifier->setType(Token::Identifier::IdentifierType::UDT);

        auto typeDecl  = addType(typeName, create<ast::ASTNodeUnion>());
        if (typeDecl == nullptr)
            return nullptr;

        const auto unionNode = dynamic_cast<ast::ASTNodeUnion *>(typeDecl->getType().get());
        if (unionNode == nullptr)
            return nullptr;

        typeDecl->setTemplateParameters(unwrapSafePointerVector(this->parseTemplateList()));

        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' after union declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        this->m_currTemplateType.push_back(typeDecl);
        while (!sequence(tkn::Separator::RightBrace)) {
            auto member = parseMember();
            if(member == nullptr)
                continue;

            unionNode->addMember(std::move(member));
        }
        this->m_currTemplateType.pop_back();

        return typeDecl;
    }

    // enum Identifier : (parseType) { <<Identifier|Identifier = (parseMathematicalExpression)[,]>...> }
    hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> Parser::parseEnum() {
        const auto typeName = getValue<Token::Identifier>(-1).get();
        auto userDefinedTypeIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
        if (userDefinedTypeIdentifier != nullptr)
            userDefinedTypeIdentifier->setType(Token::Identifier::IdentifierType::UDT);

        if (!sequence(tkn::Operator::Colon)) {
            error("Expected ':' after enum declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto underlyingType = parseType();
        if(underlyingType == nullptr)
            return nullptr;

        if (underlyingType->getEndian().has_value()) {
            errorDesc("Underlying enum type may not have an endian specifier.", "Use the 'be' or 'le' keyword when declaring a variable instead.");
            return nullptr;
        }

        auto typeDecl = addType(typeName, create<ast::ASTNodeEnum>(std::move(underlyingType)));
        if(typeDecl == nullptr)
            return nullptr;

        const auto enumNode = dynamic_cast<ast::ASTNodeEnum *>(typeDecl->getType().get());
        if (enumNode == nullptr)
            return nullptr;

        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' after enum declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        hlp::safe_unique_ptr<ast::ASTNode> lastEntry;
        while (!sequence(tkn::Separator::RightBrace)) {
            hlp::safe_unique_ptr<ast::ASTNode> enumValue;
            std::string name;

            if (sequence(tkn::Literal::Identifier, tkn::Operator::Assign)) {
                name  = getValue<Token::Identifier>(-2).get();
                auto identifier = std::get_if<Token::Identifier>(&((m_curr[-2]).value));
                if (identifier != nullptr)
                    identifier->setType(Token::Identifier::IdentifierType::LocalVariable);
                enumValue = parseMathematicalExpression();

                if (enumValue == nullptr)
                    return nullptr;

                lastEntry = enumValue->clone();
            } else if (sequence(tkn::Literal::Identifier)) {
                name = getValue<Token::Identifier>(-1).get();
                auto identifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
                if (identifier != nullptr)
                    identifier->setType(Token::Identifier::IdentifierType::PatternVariable);
                if (enumNode->getEntries().empty())
                    enumValue = create<ast::ASTNodeLiteral>(u128(0));
                else
                    enumValue = create<ast::ASTNodeMathematicalExpression>(lastEntry->clone(), create<ast::ASTNodeLiteral>(u128(1)), Token::Operator::Plus);

                lastEntry = enumValue->clone();
            } else {
                errorDesc("Invalid enum entry definition.", "Enum entries can consist of either just a name or a name followed by a value assignment.");
                break;
            }

            if (sequence(tkn::Separator::Dot, tkn::Separator::Dot, tkn::Separator::Dot)) {
                auto endExpr = parseMathematicalExpression();
                if (endExpr == nullptr)
                    return nullptr;

                enumNode->addEntry(name, std::move(enumValue), std::move(endExpr));
            } else {
                enumNode->addEntry(name, std::move(enumValue), nullptr);
            }

            if (!sequence(tkn::Separator::Comma)) {
                if (sequence(tkn::Separator::RightBrace))
                    break;

                error("Expected ',' at end of enum entry, got {}.", getFormattedToken(0));
                break;
            }
        }

        return typeDecl;
    }


    // [Identifier : (parseMathematicalExpression);|Identifier identifier;|(parseFunctionControlFlowStatement)|(parseIfStatement)|(parseMatchStatement)]
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseBitfieldEntry() {
        hlp::safe_unique_ptr<ast::ASTNode> member = nullptr;

        if (sequence(tkn::Literal::Identifier, tkn::Operator::Assign)) {
            const auto variableName = getValue<Token::Identifier>(-2).get();
            auto identifier = std::get_if<Token::Identifier>(&((m_curr[-2]).value));
            if (identifier != nullptr)
                identifier->setType(Token::Identifier::IdentifierType::LocalVariable);
            member = parseFunctionVariableAssignment(variableName);
        } else if (const auto identifierOffset = parseCompoundAssignment(tkn::Literal::Identifier); identifierOffset.has_value())
            member = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(*identifierOffset).get());
        else if (MATCHES(optional(tkn::Keyword::Unsigned) && sequence(tkn::Literal::Identifier, tkn::Operator::Colon))) {
            auto identToken = m_curr[-2];
            auto fieldName = getValue<Token::Identifier>(-2).get();
            auto identifier = std::get_if<Token::Identifier>(&((m_curr[-2]).value));
            if (identifier != nullptr)
                identifier->setType(Token::Identifier::IdentifierType::PatternVariable);

            auto bitfieldSize = parseMathematicalExpression();
            if (bitfieldSize == nullptr)
                return nullptr;

            member = createWithLocation<ast::ASTNodeBitfieldField>(identToken.location, fieldName, std::move(bitfieldSize));
        } else if (sequence(tkn::Keyword::Signed, tkn::Literal::Identifier, tkn::Operator::Colon)) {
            auto fieldName = getValue<Token::Identifier>(-2).get();
            auto identifier = std::get_if<Token::Identifier>(&((m_curr[-2]).value));
            if (identifier != nullptr)
                identifier->setType(Token::Identifier::IdentifierType::PatternVariable);

            auto bitfieldSize = parseMathematicalExpression();
            if (bitfieldSize == nullptr)
                return nullptr;

            member = create<ast::ASTNodeBitfieldFieldSigned>(fieldName, std::move(bitfieldSize));
        } else if (sequence(tkn::ValueType::Padding, tkn::Operator::Colon)) {
            auto bitfieldSize = parseMathematicalExpression();
            if (bitfieldSize == nullptr)
                return nullptr;

            member = create<ast::ASTNodeBitfieldField>("$padding$", std::move(bitfieldSize));
        } else if (peek(tkn::Literal::Identifier) || peek(tkn::ValueType::Any)) {
            hlp::safe_unique_ptr<ast::ASTNodeTypeDecl> type = nullptr;

            if (sequence(tkn::ValueType::Any)) {
                const auto typeToken = getValue<Token::ValueType>(-1);
                if (typeToken == Token::ValueType::CustomType) {
                    auto userDefinedTypeIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
                    if (userDefinedTypeIdentifier != nullptr)
                        userDefinedTypeIdentifier->setType(Token::Identifier::IdentifierType::UDT);
                }
                type = create<ast::ASTNodeTypeDecl>(Token::getTypeName(typeToken), create<ast::ASTNodeBuiltinType>(typeToken));
            } else if (sequence(tkn::Literal::Identifier)) {
                const auto originalPosition = m_curr;
                auto name = parseNamespaceResolution();

                if (sequence(tkn::Separator::LeftParenthesis)) {
                    m_curr = originalPosition;
                    member = parseFunctionCall();
                } else {
                    type = getCustomType(name);

                    auto userDefinedTypeIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
                    if (userDefinedTypeIdentifier != nullptr)
                        userDefinedTypeIdentifier->setType(Token::Identifier::IdentifierType::UDT);
                    if (type == nullptr) {
                        error("Expected a variable name followed by ':', a function call or a bitfield type name, got '{}'.", name);
                        return nullptr;
                    }
                    parseCustomTypeParameters(type);
                }
            }

            if (type == nullptr) {
                // We called a function, do no more parsing.
            } else if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Separator::LeftBracket) && sequence<Not>(tkn::Separator::LeftBracket))){
                // (parseType) Identifier[[(parseMathematicalExpression)|(parseWhileStatement)]];
                auto fieldName = getValue<Token::Identifier>(-2).get();
                auto identifier = std::get_if<Token::Identifier>(&((m_curr[-2]).value));

                hlp::safe_unique_ptr<ast::ASTNode> size;
                if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
                    size = parseWhileStatement();
                else
                    size = parseMathematicalExpression();

                if (size == nullptr)
                    return nullptr;

                if (!sequence(tkn::Separator::RightBracket)) {
                    error("Expected ']' at end of array declaration, got {}.", getFormattedToken(0));
                    return nullptr;
                }
                if (identifier != nullptr)
                    identifier->setType(Token::Identifier::IdentifierType::PatternVariable);
                member = create<ast::ASTNodeBitfieldArrayVariableDecl>(fieldName, std::move(type), std::move(size));
            } else if (sequence(tkn::Literal::Identifier)) {
                // (parseType) Identifier;
                if (sequence(tkn::Operator::At)) {
                    error("Placement syntax is invalid within bitfields.");
                    return nullptr;
                }

                auto variableName = getValue<Token::Identifier>(-1).get();

                auto identifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
                if (identifier != nullptr)
                    identifier->setType(Token::Identifier::IdentifierType::PatternVariable);
                if (sequence(tkn::Operator::Colon)) {
                    auto bitfieldSize = parseMathematicalExpression();
                    if (bitfieldSize == nullptr)
                        return nullptr;

                    member = create<ast::ASTNodeBitfieldFieldSizedType>(variableName, std::move(type), std::move(bitfieldSize));
                } else
                    member = parseMemberVariable(std::move(type), false, variableName);
            } else {
                error("Expected a variable name, got {}.", getFormattedToken(0));
                return nullptr;
            }
        } else if (sequence(tkn::Keyword::If))
            return parseConditional([this]() { return parseBitfieldEntry(); });
        else if (sequence(tkn::Keyword::Match))
            return parseMatchStatement([this]() { return parseBitfieldEntry(); });
        else if (sequence(tkn::Keyword::Try, tkn::Separator::LeftBrace))
            return parseTryCatchStatement([this]() { return parseBitfieldEntry(); });
        else if (oneOf(tkn::Keyword::Return, tkn::Keyword::Break, tkn::Keyword::Continue))
            member = parseFunctionControlFlowStatement();
        else {
            error("Invalid bitfield member definition.");
            next();
            return nullptr;
        }

        if (sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket)) {
            parseAttribute(dynamic_cast<ast::Attributable *>(member.get()));
        }

        if (!sequence(tkn::Separator::Semicolon)) {
            error("Expected ';' at end of statement, got {}.", getFormattedToken(0));
            return nullptr;
        }

        // Consume superfluous semicolons
        while (sequence(tkn::Separator::Semicolon))
            ;

        return member;
    }

    // bitfield Identifier { ... }
    hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> Parser::parseBitfield() {
        const std::string typeName = getValue<Token::Identifier>(-1).get();

        auto userDefinedTypeIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
        if (userDefinedTypeIdentifier != nullptr)
            userDefinedTypeIdentifier->setType(Token::Identifier::IdentifierType::UDT);
        auto typeDecl = addType(typeName, create<ast::ASTNodeBitfield>());
        if (typeDecl == nullptr)
            return nullptr;

        typeDecl->setTemplateParameters(unwrapSafePointerVector(this->parseTemplateList()));
        const auto bitfieldNode = dynamic_cast<ast::ASTNodeBitfield *>(typeDecl->getType().get());
        if (bitfieldNode == nullptr)
            return nullptr;

        if (!sequence(tkn::Separator::LeftBrace)) {
            errorDesc("Expected '{{' after bitfield declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        while (!sequence(tkn::Separator::RightBrace)) {
            auto entry = parseBitfieldEntry();
            if (entry == nullptr)
                continue;

            bitfieldNode->addEntry(std::move(entry));

            // Consume superfluous semicolons
            while (sequence(tkn::Separator::Semicolon))
                ;
        }

        return typeDecl;
    }

    // using Identifier;
    void Parser::parseForwardDeclaration() {
        std::string typeName = getNamespacePrefixedNames(getValue<Token::Identifier>(-1).get()).back();

        auto userDefinedTypeIdentifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
        if (userDefinedTypeIdentifier != nullptr)
            userDefinedTypeIdentifier->setType(Token::Identifier::IdentifierType::UDT);
        if (this->m_types.contains(typeName))
            return;

        this->m_types.insert({ typeName, createShared<ast::ASTNodeTypeDecl>(typeName) });
    }

    // (parseType) Identifier @ Integer
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseVariablePlacement(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type) {
        bool inVariable  = false;
        bool outVariable = false;

        auto name = getValue<Token::Identifier>(-1).get();
        auto identifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));


        hlp::safe_unique_ptr<ast::ASTNode> placementOffset, placementSection;
        if (sequence(tkn::Operator::At)) {
            if (identifier != nullptr) {
                if (m_currTemplateType.empty())
                    identifier->setType(Token::Identifier::IdentifierType::View);
                else
                    identifier->setType(Token::Identifier::IdentifierType::PlacedVariable);
            }
            placementOffset = parseMathematicalExpression();
            if (placementOffset == nullptr)
                return nullptr;

            if (sequence(tkn::Keyword::In)) {
                placementSection = parseMathematicalExpression();
                if (placementSection == nullptr)
                    return nullptr;
            }
        } else if (sequence(tkn::Keyword::In)) {
            inVariable = true;
        } else if (sequence(tkn::Keyword::Out)) {
            outVariable = true;
        } else if (sequence(tkn::Operator::Assign)) {
            std::vector<hlp::safe_unique_ptr<ast::ASTNode>> compounds;

            compounds.emplace_back(create<ast::ASTNodeVariableDecl>(name, type.unwrapUnchecked(), std::move(placementOffset.unwrapUnchecked()), nullptr, inVariable, outVariable));
            auto expression = parseMathematicalExpression();
            if (expression == nullptr)
                return nullptr;

            compounds.emplace_back(create<ast::ASTNodeLValueAssignment>(name, std::move(expression)));
            if (identifier != nullptr)
                identifier->setType(Token::Identifier::IdentifierType::GlobalVariable);
            return create<ast::ASTNodeCompoundStatement>(unwrapSafePointerVector(std::move(compounds)));
        }

        if (inVariable || outVariable) {
            bool invalidType = false;
            if (const auto builtinType = dynamic_cast<ast::ASTNodeBuiltinType*>(type->getType().get()); builtinType != nullptr) {
                if (const auto valueType = builtinType->getType(); !Token::isInteger(valueType)
                                                                   && !Token::isFloatingPoint(valueType)
                                                                   && valueType != Token::ValueType::Boolean
                                                                   && valueType != Token::ValueType::Character
                                                                   && valueType != Token::ValueType::String)
                    invalidType = true;
            } else {
                invalidType = true;
            }

            if (invalidType) {
                errorDesc("Invalid in/out parameter type.", "Allowed types are: 'char', 'bool', 'str', floating point types or integral types.");
                return nullptr;
            }
        }
        if (identifier != nullptr)
            identifier->setType(Token::Identifier::IdentifierType::GlobalVariable);
        return create<ast::ASTNodeVariableDecl>(name, type.unwrapUnchecked(), std::move(placementOffset.unwrapUnchecked()), std::move(placementSection.unwrapUnchecked()), inVariable, outVariable);
    }

    // (parseType) Identifier[[(parseMathematicalExpression)]] @ Integer
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parseArrayVariablePlacement(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();
        auto typedefIdentifier = std::get_if<Token::Identifier>(&((m_curr[-2]).value));

        hlp::safe_unique_ptr<ast::ASTNode> size;

        if (!sequence(tkn::Separator::RightBracket)) {
            if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (size == nullptr)
                return nullptr;

            if (!sequence(tkn::Separator::RightBracket)) {
                errorDesc("Expected ']' at end of array declaration, got {}.", getFormattedToken(0));
                return nullptr;
            }
        }

        hlp::safe_unique_ptr<ast::ASTNode> placementOffset, placementSection;
        if (sequence(tkn::Operator::At)) {
            placementOffset = parseMathematicalExpression();

            if (typedefIdentifier != nullptr)
                typedefIdentifier->setType(Token::Identifier::IdentifierType::PlacedVariable);

            if (placementOffset == nullptr)
                return nullptr;

            if (sequence(tkn::Keyword::In)) {
                placementSection = parseMathematicalExpression();
                if (placementSection == nullptr)
                    return nullptr;
            }

            return create<ast::ASTNodeArrayVariableDecl>(name, type.unwrapUnchecked(), std::move(size.unwrapUnchecked()), std::move(placementOffset.unwrapUnchecked()), std::move(placementSection.unwrapUnchecked()));
        } else if (sequence(tkn::Operator::Assign)) {
            auto statement = create<ast::ASTNodeArrayVariableDecl>(name, type.unwrapUnchecked(), std::move(size.unwrapUnchecked()), std::move(placementOffset.unwrapUnchecked()), std::move(placementSection.unwrapUnchecked()));
            std::vector<hlp::safe_unique_ptr<ast::ASTNode>> compoundStatement;
            {
                compoundStatement.emplace_back(std::move(statement));

                auto initStatement = parseArrayInitExpression(name);
                if (initStatement == nullptr)
                    return nullptr;
                if (typedefIdentifier != nullptr)
                    typedefIdentifier->setType(Token::Identifier::IdentifierType::GlobalVariable);
                compoundStatement.emplace_back(std::move(initStatement));
            }

            return create<ast::ASTNodeCompoundStatement>(unwrapSafePointerVector(std::move(compoundStatement)));
        }
        if (typedefIdentifier != nullptr)
            typedefIdentifier->setType(Token::Identifier::IdentifierType::GlobalVariable);

        return create<ast::ASTNodeArrayVariableDecl>(name, type.unwrapUnchecked(), std::move(size.unwrapUnchecked()), std::move(placementOffset.unwrapUnchecked()), std::move(placementSection.unwrapUnchecked()));
    }

    // (parseType) *Identifier : (parseType) @ Integer
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parsePointerVariablePlacement(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();
        auto typedefIdentifier = std::get_if<Token::Identifier>(&((m_curr[-2]).value));

        auto sizeType = parseType();
        if (sizeType == nullptr)
            return nullptr;

        if (!sequence(tkn::Operator::At)) {
            error("Expected '@' after pointer placement, got {}.", getFormattedToken(0));
            return nullptr;
        }

        if (typedefIdentifier != nullptr)
            typedefIdentifier->setType(Token::Identifier::IdentifierType::PlacedVariable);

        auto placementOffset = parseMathematicalExpression();
        if (placementOffset == nullptr)
            return nullptr;

        hlp::safe_unique_ptr<ast::ASTNode> placementSection;
        if (sequence(tkn::Keyword::In)) {
            placementSection = parseMathematicalExpression();
            if (placementSection == nullptr)
                return nullptr;
        }

        return create<ast::ASTNodePointerVariableDecl>(name, type.unwrapUnchecked(), std::move(sizeType), std::move(placementOffset.unwrapUnchecked()), std::move(placementSection.unwrapUnchecked()));
    }

    // (parseType) *Identifier[[(parseMathematicalExpression)]] : (parseType) @ Integer
    hlp::safe_unique_ptr<ast::ASTNode> Parser::parsePointerArrayVariablePlacement(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();
        auto placedIdentifier = std::get_if<Token::Identifier>(&((m_curr[-2]).value));

        hlp::safe_unique_ptr<ast::ASTNode> size;

        if (!sequence(tkn::Separator::RightBracket)) {
            if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (size == nullptr)
                return nullptr;

            if (!sequence(tkn::Separator::RightBracket)) {
                error("Expected ']' at end of array declaration, got {}.", getFormattedToken(0));
                return nullptr;
            }
        }

        if (!sequence(tkn::Operator::Colon)) {
            error("Expected ':' after pointer definition, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto sizeType = parseType();
        if (sizeType == nullptr)
            return nullptr;

        if (!sequence(tkn::Operator::At)) {
            error("Expected '@' after pointer placement, got {}.", getFormattedToken(0));
            return nullptr;
        }
        if (placedIdentifier != nullptr)
            placedIdentifier->setType(Token::Identifier::IdentifierType::PlacedVariable);
        auto placementOffset = parseMathematicalExpression();
        if (placementOffset == nullptr)
            return nullptr;

        hlp::safe_unique_ptr<ast::ASTNode> placementSection;
        if (sequence(tkn::Keyword::In)) {
            placementSection = parseMathematicalExpression();
            if (placementSection == nullptr)
                return nullptr;
        }

        return create<ast::ASTNodePointerVariableDecl>(name, createShared<ast::ASTNodeArrayVariableDecl>("", type.unwrapUnchecked(), std::move(size.unwrapUnchecked())), std::move(sizeType), std::move(placementOffset.unwrapUnchecked()), std::move(placementSection.unwrapUnchecked()));
    }

    std::vector<hlp::safe_shared_ptr<ast::ASTNode>> Parser::parseNamespace() {
        std::vector<hlp::safe_shared_ptr<ast::ASTNode>> statements;

        bool isAliased = false;
        if (sequence(tkn::ValueType::Auto)) {
            isAliased = !this->m_aliasNamespace.empty();
        }

        if (!sequence(tkn::Literal::Identifier)) {
            error("Expected identifier after 'namespace', got {}.", getFormattedToken(0));
            return { };
        }

        this->m_currNamespace.push_back(this->m_currNamespace.back());

        std::string name;
        while (true) {
            this->m_currNamespace.back().push_back(getValue<Token::Identifier>(-1).get());
            name += getValue<Token::Identifier>(-1).get();

            auto identifier = std::get_if<Token::Identifier>(&((m_curr[-1]).value));
            if (identifier != nullptr)
                identifier->setType(Token::Identifier::IdentifierType::NameSpace);
            if (sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier)) {
                name += "::";
            } else {
                break;
            }
        }

        if(isAliased) {
            this->m_autoNamespace = name;
            this->m_currNamespace.pop_back();
        }

        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' at beginning of namespace, got {}.", getFormattedToken(0));
            return { };
        }

        while (!sequence(tkn::Separator::RightBrace)) {
            auto newStatements = parseStatements();
            std::ranges::move(newStatements, std::back_inserter(statements));
        }

        if(!isAliased) // if aliased already done this
            this->m_currNamespace.pop_back();

        return statements;
    }

    hlp::safe_unique_ptr<ast::ASTNode> Parser::parsePlacement() {
        auto type = parseType();
        if (type == nullptr)
            return nullptr;

        if (sequence(tkn::Literal::Identifier, tkn::Separator::LeftBracket))
            return parseArrayVariablePlacement(std::move(type));
        if (sequence(tkn::Literal::Identifier))
            return parseVariablePlacement(std::move(type));
        if (sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Operator::Colon))
            return parsePointerVariablePlacement(std::move(type));
        if (sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Separator::LeftBracket))
            return parsePointerArrayVariablePlacement(std::move(type));

        errorHere("Invalid placement syntax.");
        next();
        return nullptr;
    }

    void Parser::includeGuard() {
        if (m_curr->location.source->mainSource)
            return;
        
        ParserManager::OnceIncludePair key = {const_cast<api::Source *>(m_curr->location.source), ""};
        if (m_parserManager->getPreprocessorOnceIncluded().contains(key))
            m_parserManager->getOnceIncluded().insert(key);
    }

    /* Program */

    // <(parseUsingDeclaration)|(parseVariablePlacement)|(parseStruct)>
    std::vector<hlp::safe_shared_ptr<ast::ASTNode>> Parser::parseStatements() {
        hlp::safe_shared_ptr<ast::ASTNode> statement;
        bool requiresSemicolon = true;

        includeGuard();

        if (const auto docComment = parseDocComment(true); docComment.has_value())
            this->addGlobalDocComment(docComment->comment);

        if (sequence(tkn::Literal::Identifier, tkn::Operator::Assign))
            statement = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (sequence(tkn::Operator::Dollar, tkn::Operator::Assign))
            statement = parseFunctionVariableAssignment("$");
        else if (const auto identifierOffset = parseCompoundAssignment(tkn::Literal::Identifier); identifierOffset.has_value())
            statement = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(*identifierOffset).get());
        else if (MATCHES(sequence(tkn::Keyword::Using, tkn::Literal::Identifier) && (peek(tkn::Operator::Assign) || peek(tkn::Operator::BoolLessThan))))
            statement = parseUsingDeclaration();
        else if (sequence(tkn::Keyword::Using, tkn::Literal::Identifier))
            parseForwardDeclaration();
        else if (sequence(tkn::Keyword::Import))
            statement = parseImportStatement();
        else if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any))
            statement = parsePlacement();
        else if (peek(tkn::Literal::Identifier) && !peek(tkn::Operator::Assign, 1) && !peek(tkn::Separator::Dot, 1)  && !peek(tkn::Separator::LeftBracket, 1)) {
            const auto originalPos = this->m_curr;
            ++this->m_curr;
            parseNamespaceResolution();
            const bool isFunction = peek(tkn::Separator::LeftParenthesis);
            this->m_curr    = originalPos;

            if (isFunction) {
                ++this->m_curr;
                statement = parseFunctionCall();
            } else
                statement = parsePlacement();
        }
        else if (sequence(tkn::Keyword::Struct, tkn::Literal::Identifier))
            statement = parseStruct();
        else if (sequence(tkn::Keyword::Union, tkn::Literal::Identifier))
            statement = parseUnion();
        else if (sequence(tkn::Keyword::Enum, tkn::Literal::Identifier))
            statement = parseEnum();
        else if (sequence(tkn::Keyword::Bitfield, tkn::Literal::Identifier))
            statement = parseBitfield();
        else if (sequence(tkn::Keyword::Function, tkn::Literal::Identifier))
            statement = parseFunctionDefinition();
        else if (sequence(tkn::Keyword::Namespace))
            return parseNamespace();
        else {
            statement = parseFunctionStatement();
            requiresSemicolon = false;
        }

        if (statement != nullptr && sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket))
            parseAttribute(dynamic_cast<ast::Attributable *>(statement.get()));

        if (requiresSemicolon && !sequence(tkn::Separator::Semicolon)) {
            error("Expected ';' at end of statement, got {}.", getFormattedToken(0));
            return { };
        }

        if (statement == nullptr)
            return { };

        if (const auto docComment = parseDocComment(false); docComment.has_value()) {
            statement->setDocComment(docComment->comment);
        }
        statement->setShouldDocument(this->m_ignoreDocsCount == 0);

        // Consume superfluous semicolons
        while (sequence(tkn::Separator::Semicolon))
            ;

        return hlp::moveToVector(std::move(statement));
    }

    std::optional<i32> Parser::parseCompoundAssignment(const Token &token) {
        const static std::array SingleTokens = { tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor };
        const static std::array DoubleTokens = { tkn::Operator::BoolLessThan, tkn::Operator::BoolGreaterThan };

        for (auto &singleToken : SingleTokens) {
            if (sequence(token, singleToken, tkn::Operator::Assign))
                return -3;
        }

        for (auto &doubleTokens : DoubleTokens) {
            if (sequence(token, doubleTokens, doubleTokens, tkn::Operator::Assign))
                return -4;
        }

        return std::nullopt;
    }

    hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> Parser::addType(const std::string &name, hlp::safe_unique_ptr<ast::ASTNode> &&node, std::optional<std::endian> endian) {
        auto typeName = getNamespacePrefixedNames(name).back();

        if (this->m_types.contains(typeName) && this->m_types.at(typeName)->isForwardDeclared()) {
            this->m_types.at(typeName)->setType(std::move(node));

            return this->m_types.at(typeName);
        }

        if (!this->m_types.contains(typeName)) {
            auto typeDecl = createShared<ast::ASTNodeTypeDecl>(typeName, std::move(std::move(node).unwrapUnchecked()), endian);
            this->m_types.insert({typeName, typeDecl});

            return typeDecl;
        }

        error("Type with name '{}' has already been declared.", typeName);
        return nullptr;
    }

    std::optional<Token::DocComment> Parser::parseDocComment(const bool global) {
        auto token = this->m_curr;

        auto commentProcessGuard = SCOPE_GUARD {
            if (global)
                this->m_processedDocComments.push_back(token);
        };

        if (token > this->m_startToken)
            --token;

        while (true) {
            if (token->type == Token::Type::DocComment) {
                if (std::ranges::find(this->m_processedDocComments, token) != this->m_processedDocComments.end())
                    return std::nullopt;

                auto content = std::get<Token::DocComment>(token->value);
                if (content.global != global)
                    return std::nullopt;

                const auto trimmed = wolv::util::trim(content.comment);
                if (trimmed.starts_with("DOCS IGNORE ON")) {
                    this->m_ignoreDocsCount += 1;
                    return std::nullopt;
                }
                if (trimmed.starts_with("DOCS IGNORE OFF")) {
                    if (this->m_ignoreDocsCount != 0)
                        this->m_ignoreDocsCount -= 1;
                    else
                        errorHere("Unmatched DOCS IGNORE OFF without previous DOCS IGNORE ON");

                    return std::nullopt;
                }

                if (this->m_ignoreDocsCount > 0)
                    return std::nullopt;
                return content;
            }

            if (token > this->m_startToken)
                --token;
            else
                break;
        }

        commentProcessGuard.release();

        return std::nullopt;
    }

    // <(parseNamespace)...> EndOfProgram
    hlp::CompileResult<std::vector<std::shared_ptr<ast::ASTNode>>> Parser::parse(std::vector<Token> &tokens) {

        this->m_curr = this->m_startToken = this->m_originalPosition = this->m_partOriginalPosition
            = TokenIter(tokens.begin(), tokens.end());

        this->m_types.clear();
        this->m_currTemplateType.clear();
        this->m_matchedOptionals.clear();
        this->m_processedDocComments.clear();

        this->m_currNamespace.clear();
        this->m_currNamespace.emplace_back();
        if (!this->m_aliasNamespace.empty())
            this->m_currNamespace.push_back(this->m_aliasNamespace);

        for (const auto &[name, type] : m_parserManager->getBuiltinTypes())
            this->m_types.emplace(name, type);

        try {
            auto program = parseTillToken(tkn::Separator::EndOfProgram);
            for (const auto &type : this->m_types)
                type.second->setCompleted();

            return { unwrapSafePointerVector(std::move(program)), this->collectErrors() };
        }
        catch (const std::out_of_range&) {
            error("Unexpected end of input");
        }
        catch (const std::logic_error&) {
            error("Tried to dereference a nullptr. This is a parser bug!");
        }
        catch (const UnrecoverableParserException&) {
            error("This is a parser bug!");
        }

        return { std::nullopt, this->collectErrors() };
    }

    Location Parser::location() {
        // get location of previous token
        return this->m_curr == this->m_startToken ? this->m_startToken->location : this->m_curr[-1].location;
    }

    void Parser::errorHere(const std::string &message) {
        errorAt(peek(tkn::Separator::EndOfProgram) ? m_curr[-1].location : m_curr->location, message);
    }

    void Parser::errorDescHere(const std::string &message, const std::string &description) {
        errorAtDesc(peek(tkn::Separator::EndOfProgram) ? m_curr[-1].location : m_curr->location, message, description);
    }

}