#include <pl/core/parser.hpp>

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
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_type_operator.hpp>
#include <pl/core/ast/ast_node_union.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_while_statement.hpp>

#include <optional>

#define MATCHES(x) (begin() && resetIfFailed(x))

// Definition syntax:
// [A]          : Either A or no token
// [A|B]        : Either A, B or no token
// <A|B>        : Either A or B
// <A...>       : One or more of A
// A B C        : Sequence of tokens A then B then C
// (parseXXXX)  : Parsing handled by other function
namespace pl::core {

    /* Mathematical expressions */
    // ([(parseMathematicalExpression)|<(parseMathematicalExpression),...>(parseMathematicalExpression)]
    std::vector<std::unique_ptr<ast::ASTNode>> Parser::parseParameters() {
        std::vector<std::unique_ptr<ast::ASTNode>> params;

        while (!MATCHES(sequence(tkn::Separator::RightParenthesis))) {
            params.push_back(parseMathematicalExpression());

            if (MATCHES(sequence(tkn::Separator::Comma, tkn::Separator::RightParenthesis)))
                err::P0002.throwError(fmt::format("Expected ')' at end of parameter list, got {}.", getFormattedToken(0)), {}, 1);
            else if (MATCHES(sequence(tkn::Separator::RightParenthesis)))
                break;
            else if (!MATCHES(sequence(tkn::Separator::Comma)))
                err::P0002.throwError(fmt::format("Expected ',' in-between parameters, got {}.", getFormattedToken(0)), {}, 1);
        }

        return params;
    }

    // Identifier(<parseParameters>)
    std::unique_ptr<ast::ASTNode> Parser::parseFunctionCall() {
        std::string functionName = parseNamespaceResolution();

        if (!MATCHES(sequence(tkn::Separator::LeftParenthesis)))
            err::P0002.throwError(fmt::format("Expected '(' after function name, got {}.", getFormattedToken(0)), {}, 1);

        auto params = parseParameters();

        return create<ast::ASTNodeFunctionCall>(functionName, std::move(params));
    }

    std::unique_ptr<ast::ASTNode> Parser::parseStringLiteral() {
        return create<ast::ASTNodeLiteral>(getValue<Token::Literal>(-1));
    }

    std::string Parser::parseNamespaceResolution() {
        std::string name;

        while (true) {
            name += getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier))) {
                name += "::";
                continue;
            } else
                break;
        }

        return name;
    }

    std::unique_ptr<ast::ASTNode> Parser::parseScopeResolution() {
        std::string typeName;

        while (true) {
            typeName += getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier))) {
                if (peek(tkn::Operator::ScopeResolution, 0) && peek(tkn::Literal::Identifier, 1)) {
                    typeName += "::";
                    continue;
                } else {
                    if (this->m_types.contains(typeName))
                        return create<ast::ASTNodeScopeResolution>(this->m_types[typeName], getValue<Token::Identifier>(-1).get());
                    else {
                        for (auto &potentialName : getNamespacePrefixedNames(typeName)) {
                            if (this->m_types.contains(potentialName)) {
                                return create<ast::ASTNodeScopeResolution>(this->m_types[potentialName], getValue<Token::Identifier>(-1).get());
                            }
                        }

                        err::P0004.throwError("No namespace with this name found.", { }, 1);
                    }
                }
            } else
                break;
        }

        err::P0004.throwError("Invalid scope resolution.", "Expected statement in the form of 'NamespaceA::NamespaceB::TypeName'.", 1);
    }

    std::unique_ptr<ast::ASTNode> Parser::parseRValue() {
        ast::ASTNodeRValue::Path path;
        return this->parseRValue(path);
    }

    // <Identifier[.]...>
    std::unique_ptr<ast::ASTNode> Parser::parseRValue(ast::ASTNodeRValue::Path &path) {
        if (peek(tkn::Literal::Identifier, -1))
            path.emplace_back(getValue<Token::Identifier>(-1).get());
        else if (peek(tkn::Keyword::Parent, -1))
            path.emplace_back("parent");
        else if (peek(tkn::Keyword::This, -1))
            path.emplace_back("this");
        else if (peek(tkn::Operator::Dollar, -1))
            path.emplace_back("$");
        else if (peek(tkn::Keyword::Null, -1))
            path.emplace_back("null");

        if (MATCHES(sequence(tkn::Separator::LeftBracket) && !peek(tkn::Separator::LeftBracket))) {
            path.emplace_back(parseMathematicalExpression());
            if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                err::P0002.throwError(fmt::format("Expected ']' at end of array indexing, got {}.", getFormattedToken(0)), {}, 1);
        }

        if (MATCHES(sequence(tkn::Separator::Dot))) {
            if (MATCHES(oneOf(tkn::Literal::Identifier, tkn::Keyword::Parent)))
                return this->parseRValue(path);
            else
                err::P0002.throwError("Invalid member access, expected variable identifier or parent keyword.", {}, 1);
        } else
            return create<ast::ASTNodeRValue>(std::move(path));
    }

    // <Integer|((parseMathematicalExpression))>
    std::unique_ptr<ast::ASTNode> Parser::parseFactor() {
        if (MATCHES(sequence(tkn::Literal::Numeric)))
            return create<ast::ASTNodeLiteral>(getValue<Token::Literal>(-1));
        else if (peek(tkn::Operator::Plus) || peek(tkn::Operator::Minus) || peek(tkn::Operator::BitNot) || peek(tkn::Operator::BoolNot))
            return this->parseMathematicalExpression();
        else if (MATCHES(sequence(tkn::Separator::LeftParenthesis))) {
            auto node = this->parseMathematicalExpression();
            if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
                err::P0002.throwError("Mismatched '(' in mathematical expression.", {}, 1);

            return node;
        } else if (MATCHES(sequence(tkn::Literal::Identifier))) {
            auto originalPos = this->m_curr;
            parseNamespaceResolution();

            bool isFunction = peek(tkn::Separator::LeftParenthesis);
            this->m_curr    = originalPos;

            if (isFunction) {
                return this->parseFunctionCall();
            } else if (peek(tkn::Operator::ScopeResolution, 0)) {
                return this->parseScopeResolution();
            } else {
                return this->parseRValue();
            }
        } else if (MATCHES(oneOf(tkn::Keyword::Parent, tkn::Keyword::This, tkn::Operator::Dollar, tkn::Keyword::Null))) {
            return this->parseRValue();
        } else if (MATCHES(oneOf(tkn::Operator::AddressOf, tkn::Operator::SizeOf) && sequence(tkn::Separator::LeftParenthesis))) {
            auto op = getValue<Token::Operator>(-2);

            std::unique_ptr<ast::ASTNode> result;

            if (MATCHES(oneOf(tkn::Literal::Identifier))) {
                auto startToken = this->m_curr;
                if (op == Token::Operator::SizeOf) {
                    auto type = getCustomType(parseNamespaceResolution());
                    if (type != nullptr) {
                        parseCustomTypeParameters(type);
                        result = create<ast::ASTNodeTypeOperator>(op, move(type));
                    }
                }

                if (result == nullptr) {
                    this->m_curr = startToken;
                    result = create<ast::ASTNodeTypeOperator>(op, this->parseRValue());
                }
            } else if (MATCHES(oneOf(tkn::Keyword::Parent, tkn::Keyword::This))) {
                result = create<ast::ASTNodeTypeOperator>(op, this->parseRValue());
            } else if (op == Token::Operator::SizeOf && MATCHES(sequence(tkn::ValueType::Any))) {
                auto type = getValue<Token::ValueType>(-1);

                result = create<ast::ASTNodeLiteral>(u128(Token::getTypeSize(type)));
            } else if (MATCHES(sequence(tkn::Operator::Dollar))) {
                result = create<ast::ASTNodeTypeOperator>(op);
            } else {
                if (op == Token::Operator::SizeOf)
                    err::P0005.throwError("Expected rvalue, type or '$' operator.", {}, 1);
                else if (op == Token::Operator::AddressOf)
                    err::P0005.throwError("Expected rvalue or '$' operator.", {}, 1);
            }

            if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
                err::P0002.throwError("Mismatched '(' of type operator expression.", {}, 1);

            return result;
        } else {
            err::P0002.throwError(fmt::format("Expected value, got {}.", getFormattedToken(0)), {}, 1);
        }
    }

    std::unique_ptr<ast::ASTNode> Parser::parseCastExpression() {
        if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any)) {
            auto type        = parseType();
            auto builtinType = dynamic_cast<ast::ASTNodeBuiltinType *>(type->getType().get());

            if (builtinType == nullptr)
                err::P0006.throwError("Cannot use non-built-in type in cast expression.", {}, 1);

            if (!peek(tkn::Separator::LeftParenthesis))
                err::P0002.throwError(fmt::format("Expected '(' after type cast, got {}.", getFormattedToken(0)), {}, 1);

            auto node = parseFactor();

            return create<ast::ASTNodeCast>(std::move(node), std::move(type));
        } else return parseFactor();
    }

    // <+|-|!|~> (parseFactor)
    std::unique_ptr<ast::ASTNode> Parser::parseUnaryExpression() {
        if (MATCHES(oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::BoolNot, tkn::Operator::BitNot))) {
            auto op = getValue<Token::Operator>(-1);

            return create<ast::ASTNodeMathematicalExpression>(create<ast::ASTNodeLiteral>(0), this->parseCastExpression(), op);
        } else if (MATCHES(sequence(tkn::Literal::String))) {
            return this->parseStringLiteral();
        }

        return this->parseCastExpression();
    }

    // (parseUnaryExpression) <*|/|%> (parseUnaryExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseMultiplicativeExpression() {
        auto node = this->parseUnaryExpression();

        while (MATCHES(oneOf(tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseUnaryExpression(), op);
        }

        return node;
    }

    // (parseMultiplicativeExpression) <+|-> (parseMultiplicativeExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseAdditiveExpression() {
        auto node = this->parseMultiplicativeExpression();

        while (MATCHES(variant(tkn::Operator::Plus, tkn::Operator::Minus))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseMultiplicativeExpression(), op);
        }

        return node;
    }

    // (parseAdditiveExpression) < >>|<< > (parseAdditiveExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseShiftExpression() {
        auto node = this->parseAdditiveExpression();

        while (MATCHES(variant(tkn::Operator::LeftShift, tkn::Operator::RightShift))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseAdditiveExpression(), op);
        }

        return node;
    }

    // (parseShiftExpression) & (parseShiftExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseBinaryAndExpression() {
        auto node = this->parseShiftExpression();

        while (MATCHES(sequence(tkn::Operator::BitAnd))) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseShiftExpression(), Token::Operator::BitAnd);
        }

        return node;
    }

    // (parseBinaryAndExpression) ^ (parseBinaryAndExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseBinaryXorExpression() {
        auto node = this->parseBinaryAndExpression();

        while (MATCHES(sequence(tkn::Operator::BitXor))) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBinaryAndExpression(), Token::Operator::BitXor);
        }

        return node;
    }

    // (parseBinaryXorExpression) | (parseBinaryXorExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseBinaryOrExpression(bool inMatchRange) {
        auto node = this->parseBinaryXorExpression();

        if (inMatchRange && peek(tkn::Operator::BitOr))
            return node;
        while (MATCHES(sequence(tkn::Operator::BitOr))) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBinaryXorExpression(), Token::Operator::BitOr);
        }

        return node;
    }

    // (parseBinaryOrExpression) < >=|<=|>|< > (parseBinaryOrExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseRelationExpression(bool inTemplate, bool inMatchRange) {
        auto node = this->parseBinaryOrExpression(inMatchRange);

        if (inTemplate && peek(tkn::Operator::BoolGreaterThan))
            return node;

        while (MATCHES(sequence(tkn::Operator::BoolGreaterThan) || sequence(tkn::Operator::BoolLessThan) || sequence(tkn::Operator::BoolGreaterThanOrEqual) || sequence(tkn::Operator::BoolLessThanOrEqual))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBinaryOrExpression(inMatchRange), op);
        }

        return node;
    }

    // (parseRelationExpression) <==|!=> (parseRelationExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseEqualityExpression(bool inTemplate, bool inMatchRange) {
        auto node = this->parseRelationExpression(inTemplate, inMatchRange);

        while (MATCHES(sequence(tkn::Operator::BoolEqual) || sequence(tkn::Operator::BoolNotEqual))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseRelationExpression(inTemplate, inMatchRange), op);
        }

        return node;
    }

    // (parseEqualityExpression) && (parseEqualityExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseBooleanAnd(bool inTemplate, bool inMatchRange) {
        auto node = this->parseEqualityExpression(inTemplate, inMatchRange);

        while (MATCHES(sequence(tkn::Operator::BoolAnd))) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseEqualityExpression(inTemplate, inMatchRange), Token::Operator::BoolAnd);
        }

        return node;
    }

    // (parseBooleanAnd) ^^ (parseBooleanAnd)
    std::unique_ptr<ast::ASTNode> Parser::parseBooleanXor(bool inTemplate, bool inMatchRange) {
        auto node = this->parseBooleanAnd(inTemplate, inMatchRange);

        while (MATCHES(sequence(tkn::Operator::BoolXor))) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBooleanAnd(inTemplate, inMatchRange), Token::Operator::BoolXor);
        }

        return node;
    }

    // (parseBooleanXor) || (parseBooleanXor)
    std::unique_ptr<ast::ASTNode> Parser::parseBooleanOr(bool inTemplate, bool inMatchRange) {
        auto node = this->parseBooleanXor(inTemplate, inMatchRange);

        while (MATCHES(sequence(tkn::Operator::BoolOr))) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBooleanXor(inTemplate, inMatchRange), Token::Operator::BoolOr);
        }

        return node;
    }

    // (parseBooleanOr) ? (parseBooleanOr) : (parseBooleanOr)
    std::unique_ptr<ast::ASTNode> Parser::parseTernaryConditional(bool inTemplate, bool inMatchRange) {
        auto node = this->parseBooleanOr(inTemplate, inMatchRange);

        while (MATCHES(sequence(tkn::Operator::TernaryConditional))) {
            auto second = this->parseBooleanOr(inTemplate, inMatchRange);

            if (!MATCHES(sequence(tkn::Operator::Colon)))
                err::P0002.throwError(fmt::format("Expected ':' after ternary condition, got {}", getFormattedToken(0)), {}, 1);

            auto third = this->parseBooleanOr(inTemplate, inMatchRange);
            node = create<ast::ASTNodeTernaryExpression>(std::move(node), std::move(second), std::move(third), Token::Operator::TernaryConditional);
        }

        return node;
    }

    // (parseTernaryConditional)
    std::unique_ptr<ast::ASTNode> Parser::parseMathematicalExpression(bool inTemplate, bool inMatchRange) {
        return this->parseTernaryConditional(inTemplate, inMatchRange);
    }

    // [[ <Identifier[( (parseStringLiteral) )], ...> ]]
    void Parser::parseAttribute(ast::Attributable *currNode) {
        if (currNode == nullptr)
            err::P0007.throwError("Cannot use attribute here.", "Attributes can only be applied after type or variable definitions.", 1);

        do {
            if (!MATCHES(sequence(tkn::Literal::Identifier)))
                err::P0002.throwError(fmt::format("Expected attribute instruction name, got {}", getFormattedToken(0)), {}, 1);

            auto attribute = parseNamespaceResolution();

            if (MATCHES(sequence(tkn::Separator::LeftParenthesis))) {
                std::vector<std::unique_ptr<ast::ASTNode>> args;
                do {
                    args.push_back(parseMathematicalExpression());
                } while (MATCHES(sequence(tkn::Separator::Comma)));

                if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
                    err::P0002.throwError(fmt::format("Expected ')', got {}", getFormattedToken(0)), {}, 1);

                currNode->addAttribute(create<ast::ASTNodeAttribute>(attribute, std::move(args)));
            } else
                currNode->addAttribute(create<ast::ASTNodeAttribute>(attribute));

        } while (MATCHES(sequence(tkn::Separator::Comma)));

        if (!MATCHES(sequence(tkn::Separator::RightBracket, tkn::Separator::RightBracket)))
            err::P0002.throwError(fmt::format("Expected ']]' after attribute, got {}.", getFormattedToken(0)), {}, 1);
    }

    /* Functions */

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionDefinition() {
        const auto &functionName = getValue<Token::Identifier>(-1).get();
        std::vector<std::pair<std::string, std::unique_ptr<ast::ASTNode>>> params;
        std::optional<std::string> parameterPack;

        if (!MATCHES(sequence(tkn::Separator::LeftParenthesis)))
            err::P0002.throwError(fmt::format("Expected '(' after function declaration, got {}.", getFormattedToken(0)), {}, 1);

        // Parse parameter list
        bool hasParams        = !peek(tkn::Separator::RightParenthesis);
        u32 unnamedParamCount = 0;
        std::vector<std::unique_ptr<ast::ASTNode>> defaultParameters;

        while (hasParams) {
            if (MATCHES(sequence(tkn::ValueType::Auto, tkn::Separator::Dot, tkn::Separator::Dot, tkn::Separator::Dot, tkn::Literal::Identifier))) {
                parameterPack = getValue<Token::Identifier>(-1).get();

                if (MATCHES(sequence(tkn::Separator::Comma)))
                    err::P0008.throwError("Parameter pack can only appear at the end of the parameter list.", {}, 1);

                break;
            } else {
                auto type = parseType();

                if (MATCHES(sequence(tkn::Literal::Identifier)))
                    params.emplace_back(getValue<Token::Identifier>(-1).get(), std::move(type));
                else {
                    params.emplace_back(std::to_string(unnamedParamCount), std::move(type));
                    unnamedParamCount++;
                }

                if (MATCHES(sequence(tkn::Operator::Assign))) {
                    // Parse default parameters
                    defaultParameters.push_back(parseMathematicalExpression());
                } else {
                    if (!defaultParameters.empty())
                        err::P0002.throwError(fmt::format("Expected default argument value for parameter '{}', got {}.", params.back().first, getFormattedToken(0)), {}, 1);
                }

                if (!MATCHES(sequence(tkn::Separator::Comma))) {
                    break;
                }
            }
        }

        if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
            err::P0002.throwError(fmt::format("Expected ')' after parameter list, got {}.", getFormattedToken(0)), {}, 1);

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            err::P0002.throwError(fmt::format("Expected '{{' after function head, got {}.", getFormattedToken(0)), {}, 1);


        // Parse function body
        std::vector<std::unique_ptr<ast::ASTNode>> body;

        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            body.push_back(this->parseFunctionStatement());
        }

        return create<ast::ASTNodeFunctionDefinition>(getNamespacePrefixedNames(functionName).back(), std::move(params), std::move(body), parameterPack, std::move(defaultParameters));
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionVariableDecl(bool constant) {
        std::unique_ptr<ast::ASTNode> statement;
        auto type = parseType();

        if (MATCHES(sequence(tkn::Literal::Identifier))) {
            auto identifier = getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(tkn::Separator::LeftBracket) && !peek(tkn::Separator::LeftBracket))) {
                statement = parseMemberArrayVariable(std::move(type), true, constant);
            } else {
                statement = parseMemberVariable(std::move(type), true, constant, identifier);

                if (MATCHES(sequence(tkn::Operator::Assign))) {
                    auto expression = parseMathematicalExpression();

                    std::vector<std::unique_ptr<ast::ASTNode>> compoundStatement;
                    {
                        compoundStatement.push_back(std::move(statement));
                        compoundStatement.push_back(create<ast::ASTNodeLValueAssignment>(identifier, std::move(expression)));
                    }

                    statement = create<ast::ASTNodeCompoundStatement>(std::move(compoundStatement));
                }
            }
        } else
            err::P0002.throwError(fmt::format("Expected identifier in variable declaration, got {}.", getFormattedToken(0)), {}, 1);

        return statement;
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionStatement(bool needsSemicolon) {
        std::unique_ptr<ast::ASTNode> statement;

        if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Operator::Assign)))
            statement = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (MATCHES(sequence(tkn::Operator::Dollar, tkn::Operator::Assign)))
            statement = parseFunctionVariableAssignment("$");
        else if (MATCHES(oneOf(tkn::Literal::Identifier) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            statement = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(-3).get());
        else if (MATCHES(oneOf(tkn::Operator::Dollar) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            statement = parseFunctionVariableCompoundAssignment("$");
        else if (MATCHES(oneOf(tkn::Keyword::Return, tkn::Keyword::Break, tkn::Keyword::Continue)))
            statement = parseFunctionControlFlowStatement();
        else if (MATCHES(sequence(tkn::Keyword::If))) {
            statement      = parseFunctionConditional();
            needsSemicolon = false;
        } else if (MATCHES(sequence(tkn::Keyword::Match))) {
            statement      = parseFunctionMatch();
            needsSemicolon = false;
        } else if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))) {
            statement      = parseFunctionWhileLoop();
            needsSemicolon = false;
        } else if (MATCHES(sequence(tkn::Keyword::For, tkn::Separator::LeftParenthesis))) {
            statement      = parseFunctionForLoop();
            needsSemicolon = false;
        } else if (MATCHES(sequence(tkn::Literal::Identifier) && (peek(tkn::Separator::Dot) || peek(tkn::Separator::LeftBracket)))) {
            auto lhs = parseRValue();

            if (!MATCHES(sequence(tkn::Operator::Assign)))
                err::P0002.throwError(fmt::format("Expected value after '=' in variable assignment, got {}.", getFormattedToken(0)), {}, 0);

            auto rhs = parseMathematicalExpression();

            statement = create<ast::ASTNodeRValueAssignment>(std::move(lhs), std::move(rhs));
        } else if (MATCHES(sequence(tkn::Literal::Identifier))) {
            auto originalPos = this->m_curr;
            parseNamespaceResolution();
            bool isFunction = peek(tkn::Separator::LeftParenthesis);

            if (isFunction) {
                this->m_curr = originalPos;
                statement    = parseFunctionCall();
            } else {
                this->m_curr = originalPos - 1;
                statement    = parseFunctionVariableDecl();
            }
        } else if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any)) {
            statement = parseFunctionVariableDecl();
        } else if (MATCHES(sequence(tkn::Keyword::Const))) {
            statement = parseFunctionVariableDecl(true);
        } else
            err::P0002.throwError("Invalid function statement.", {}, 0);

        if (needsSemicolon && !MATCHES(sequence(tkn::Separator::Semicolon)))
            err::P0002.throwError(fmt::format("Expected ';' at end of statement, got {}.", getFormattedToken(0)), {}, 1);

        // Consume superfluous semicolons
        while (needsSemicolon && MATCHES(sequence(tkn::Separator::Semicolon)))
            ;

        return statement;
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionVariableAssignment(const std::string &lvalue) {
        auto rvalue = this->parseMathematicalExpression();

        return create<ast::ASTNodeLValueAssignment>(lvalue, std::move(rvalue));
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionVariableCompoundAssignment(const std::string &lvalue) {
        const auto &op = getValue<Token::Operator>(-2);

        auto rvalue = this->parseMathematicalExpression();

        return create<ast::ASTNodeLValueAssignment>(lvalue, create<ast::ASTNodeMathematicalExpression>(create<ast::ASTNodeRValue>(hlp::moveToVector<ast::ASTNodeRValue::PathSegment>(lvalue)), std::move(rvalue), op));
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionControlFlowStatement() {
        ControlFlowStatement type;
        if (peek(tkn::Keyword::Return, -1))
            type = ControlFlowStatement::Return;
        else if (peek(tkn::Keyword::Break, -1))
            type = ControlFlowStatement::Break;
        else if (peek(tkn::Keyword::Continue, -1))
            type = ControlFlowStatement::Continue;
        else
            err::P0002.throwError("Invalid control flow statement.", "Control flow statements include 'return', 'break' and 'continue'.", 1);

        if (peek(tkn::Separator::Semicolon))
            return create<ast::ASTNodeControlFlowStatement>(type, nullptr);
        else if (type == ControlFlowStatement::Return)
            return create<ast::ASTNodeControlFlowStatement>(type, this->parseMathematicalExpression());
        else
            err::P0002.throwError("Return value can only be passed to a 'return' statement.", {}, 1);
    }

    std::vector<std::unique_ptr<ast::ASTNode>> Parser::parseStatementBody() {
        std::vector<std::unique_ptr<ast::ASTNode>> body;

        if (MATCHES(sequence(tkn::Separator::LeftBrace))) {
            while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
                body.push_back(parseFunctionStatement());
            }
        } else {
            body.push_back(parseFunctionStatement());
        }

        return body;
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionConditional() {
        if (!MATCHES(sequence(tkn::Separator::LeftParenthesis)))
            err::P0002.throwError(fmt::format("Expected '(' after 'if', got {}.", getFormattedToken(0)), {}, 1);

        auto condition = parseMathematicalExpression();
        std::vector<std::unique_ptr<ast::ASTNode>> trueBody, falseBody;

        if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
            err::P0002.throwError(fmt::format("Expected ')' at end of if head, got {}.", getFormattedToken(0)), {}, 1);

        trueBody = parseStatementBody();

        if (MATCHES(sequence(tkn::Keyword::Else)))
            falseBody = parseStatementBody();

        return create<ast::ASTNodeConditionalStatement>(std::move(condition), std::move(trueBody), std::move(falseBody));
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionMatch() {
        if (!MATCHES(sequence(tkn::Separator::LeftParenthesis)))
            err::P0002.throwError(fmt::format("Expected '(' after 'match', got {}.", getFormattedToken(0)), {}, 1);

        auto condition = parseParameters();

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            err::P0002.throwError(fmt::format("Expected '{{' after match head, got {}.", getFormattedToken(0)), {}, 1);

        std::vector<ast::MatchCase> cases;
        std::optional<ast::MatchCase> defaultCase;

        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            if (!MATCHES(sequence(tkn::Separator::LeftParenthesis)))
                err::P0002.throwError(fmt::format("Expected '(', got {}.", getFormattedToken(0)), {}, 1);

            auto [caseCondition, isDefault] = parseCaseParameters(condition);
            if (!MATCHES(sequence(tkn::Operator::Colon)))
                err::P0002.throwError(fmt::format("Expected ':', got {}.", getFormattedToken(0)), {}, 1);

            auto body = parseStatementBody();

            if (isDefault)
                defaultCase = ast::MatchCase(std::move(caseCondition), std::move(body));
            else
                cases.emplace_back(std::move(caseCondition), std::move(body));

            if (MATCHES(sequence(tkn::Separator::RightBrace)))
                break;
        }

        return create<ast::ASTNodeMatchStatement>(std::move(cases), std::move(defaultCase));
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionWhileLoop() {
        auto condition = parseMathematicalExpression();
        std::vector<std::unique_ptr<ast::ASTNode>> body;

        if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
            err::P0002.throwError(fmt::format("Expected ')' at end of while head, got {}.", getFormattedToken(0)), {}, 1);

        body = parseStatementBody();

        return create<ast::ASTNodeWhileStatement>(std::move(condition), std::move(body));
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionForLoop() {
        auto preExpression = parseFunctionStatement(false);

        if (!MATCHES(sequence(tkn::Separator::Comma)))
            err::P0002.throwError(fmt::format("Expected ',' after for loop expression, got {}.", getFormattedToken(0)), {}, 1);

        auto condition = parseMathematicalExpression();

        if (!MATCHES(sequence(tkn::Separator::Comma)))
            err::P0002.throwError(fmt::format("Expected ',' after for loop expression, got {}.", getFormattedToken(0)), {}, 1);

        auto postExpression = parseFunctionStatement(false);

        std::vector<std::unique_ptr<ast::ASTNode>> body;


        if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
            err::P0002.throwError(fmt::format("Expected ')' at end of for loop head, got {}.", getFormattedToken(0)), {}, 1);

        body = parseStatementBody();

        std::vector<std::unique_ptr<ast::ASTNode>> compoundStatement;
        {
            compoundStatement.push_back(std::move(preExpression));
            compoundStatement.push_back(create<ast::ASTNodeWhileStatement>(std::move(condition), std::move(body), std::move(postExpression)));
        }

        return create<ast::ASTNodeCompoundStatement>(std::move(compoundStatement), true);
    }

    /* Control flow */

    // if ((parseMathematicalExpression)) { (parseMember) }
    std::unique_ptr<ast::ASTNode> Parser::parseConditional(const std::function<std::unique_ptr<ast::ASTNode>()> &memberParser) {
        if (!MATCHES(sequence(tkn::Separator::LeftParenthesis)))
            err::P0002.throwError(fmt::format("Expected '(' after 'if', got {}.", getFormattedToken(0)), {}, 1);

        auto condition = parseMathematicalExpression();
        std::vector<std::unique_ptr<ast::ASTNode>> trueBody, falseBody;

        if (MATCHES(sequence(tkn::Separator::RightParenthesis, tkn::Separator::LeftBrace))) {
            while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
                trueBody.push_back(memberParser());
            }
        } else if (MATCHES(sequence(tkn::Separator::RightParenthesis))) {
            trueBody.push_back(memberParser());
        } else
            err::P0002.throwError(fmt::format("Expected ')' after if head, got {}.", getFormattedToken(0)), {}, 1);

        if (MATCHES(sequence(tkn::Keyword::Else, tkn::Separator::LeftBrace))) {
            while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
                falseBody.push_back(memberParser());
            }
        } else if (MATCHES(sequence(tkn::Keyword::Else))) {
            falseBody.push_back(memberParser());
        }

        return create<ast::ASTNodeConditionalStatement>(std::move(condition), std::move(trueBody), std::move(falseBody));
    }

    std::pair<std::unique_ptr<ast::ASTNode>, bool> Parser::parseCaseParameters(std::vector<std::unique_ptr<ast::ASTNode>> &condition) {
        std::vector<std::unique_ptr<ast::ASTNode>> compiledConditions;

        size_t caseIndex = 0;
        bool isDefault = true;
        while (!MATCHES(sequence(tkn::Separator::RightParenthesis))) {
            if (caseIndex > condition.size() - 1) {
                err::P0002.throwError("Size of case parameters bigger than size of match condition.", {}, 1);
            }
            if (MATCHES(sequence(tkn::Keyword::Underscore))) {
                // if '_' is found, act as wildcard, push literal(true)
                compiledConditions.push_back(std::make_unique<ast::ASTNodeLiteral>(true));
            } else {
                isDefault = false;
                auto &param = condition[caseIndex];
                auto first = parseMathematicalExpression(false, true);
                if (peek(tkn::Operator::BitOr)) {
                    // check for multiple options
                    // a | b | c should compile to
                    // param == a || param == b || param == c
                    std::vector<std::unique_ptr<ast::ASTNode>> options;
                    while (MATCHES(sequence(tkn::Operator::BitOr))) {
                        options.push_back(parseMathematicalExpression(false, true));
                    }
                    auto cond = create<ast::ASTNodeMathematicalExpression>(param->clone(), std::move(first), Token::Operator::BoolEqual);
                    for (auto &option : options) {
                        auto eq = create<ast::ASTNodeMathematicalExpression>(param->clone(), std::move(option), Token::Operator::BoolEqual);
                        cond = create<ast::ASTNodeMathematicalExpression>(std::move(cond), std::move(eq), Token::Operator::BoolOr);
                    }
                    compiledConditions.push_back(std::move(cond));
                } else if (MATCHES(sequence(tkn::Separator::Dot, tkn::Separator::Dot, tkn::Separator::Dot))) {
                    // range a ... b should compile to
                    // param >= a && param <= b
                    auto last = parseMathematicalExpression();
                    auto cond = create<ast::ASTNodeMathematicalExpression>(param->clone(), std::move(first), Token::Operator::BoolGreaterThanOrEqual);
                    auto cond2 = create<ast::ASTNodeMathematicalExpression>(param->clone(), std::move(last), Token::Operator::BoolLessThanOrEqual);
                    cond = create<ast::ASTNodeMathematicalExpression>(std::move(cond), std::move(cond2), Token::Operator::BoolAnd);
                    compiledConditions.push_back(std::move(cond));
                } else {
                    // else just compile to param == a
                    auto cond = create<ast::ASTNodeMathematicalExpression>(param->clone(), std::move(first), Token::Operator::BoolEqual);
                    compiledConditions.push_back(std::move(cond));
                }
            }

            caseIndex++;
            if (MATCHES(sequence(tkn::Separator::Comma, tkn::Separator::RightParenthesis)))
                err::P0002.throwError(fmt::format("Expected ')' at end of parameter list, got {}.", getFormattedToken(0)), {}, 1);
            else if (MATCHES(sequence(tkn::Separator::RightParenthesis)))
                break;
            else if (!MATCHES(sequence(tkn::Separator::Comma)))
                err::P0002.throwError(fmt::format("Expected ',' in-between parameters, got {}.", getFormattedToken(0)), {}, 1);
        }

        if (compiledConditions.empty()) {
            err::P0002.throwError("No parameters found in case statement.", {}, 1);
        }

        if (caseIndex != condition.size()) {
            err::P0002.throwError("Size of case parameters smaller than size of match condition.", {}, 1);
        }

        // make multi expressions into single expression
        auto cond = std::move(compiledConditions[0]);
        for (size_t condIndex = 1; condIndex < compiledConditions.size(); condIndex++) {
            cond = create<ast::ASTNodeMathematicalExpression>(
                    std::move(cond), std::move(compiledConditions[condIndex]), Token::Operator::BoolAnd);
        }

        return {std::move(cond), isDefault};
    }

    // match ((parseParameters)) { (parseParameters { (parseMember) })*, default { (parseMember) } }
    std::unique_ptr<ast::ASTNode> Parser::parseMatchStatement(const std::function<std::unique_ptr<ast::ASTNode>()> &memberParser) {
        if (!MATCHES(sequence(tkn::Separator::LeftParenthesis)))
            err::P0002.throwError(fmt::format("Expected '(' after 'match', got {}.", getFormattedToken(0)), {}, 1);

        auto condition = parseParameters();

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            err::P0002.throwError(fmt::format("Expected '{{' after match head, got {}.", getFormattedToken(0)), {}, 1);

        std::vector<ast::MatchCase> cases;
        std::optional<ast::MatchCase> defaultCase;

        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            if (!MATCHES(sequence(tkn::Separator::LeftParenthesis)))
                err::P0002.throwError(fmt::format("Expected '(', got {}.", getFormattedToken(0)), {}, 1);

            auto [caseCondition, isDefault] = parseCaseParameters(condition);
            std::vector<std::unique_ptr<ast::ASTNode>> body;

            if (MATCHES(sequence(tkn::Operator::Colon, tkn::Separator::LeftBrace))) {
                while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
                    body.push_back(memberParser());
                }
            } else if (MATCHES(sequence(tkn::Operator::Colon))) {
                body.push_back(memberParser());
            } else
                err::P0002.throwError(fmt::format("Expected ':' after case condition, got {}.", getFormattedToken(0)), {}, 1);

            if (isDefault)
                defaultCase = ast::MatchCase(std::move(caseCondition), std::move(body));
            else
                cases.emplace_back(std::move(caseCondition), std::move(body));

            if (MATCHES(sequence(tkn::Separator::RightBrace)))
                break;
        }

        return create<ast::ASTNodeMatchStatement>(std::move(cases), std::move(defaultCase));
    }

    // while ((parseMathematicalExpression))
    std::unique_ptr<ast::ASTNode> Parser::parseWhileStatement() {
        auto condition = parseMathematicalExpression();

        if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
            err::P0002.throwError(fmt::format("Expected ')' after while head, got {}.", getFormattedToken(0)), {}, 1);

        return create<ast::ASTNodeWhileStatement>(std::move(condition), std::vector<std::unique_ptr<ast::ASTNode>>{});
    }

    /* Type declarations */

    std::unique_ptr<ast::ASTNodeTypeDecl> Parser::getCustomType(std::string baseTypeName) {
        if (!this->m_currTemplateType.empty())
            for (const auto &templateParameter : this->m_currTemplateType.front()->getTemplateParameters()) {
                if (auto templateType = dynamic_cast<ast::ASTNodeTypeDecl*>(templateParameter.get()); templateType != nullptr)
                    if (templateType->getName() == baseTypeName)
                        return create<ast::ASTNodeTypeDecl>("", templateParameter);
            }

        for (const auto &typeName : getNamespacePrefixedNames(baseTypeName)) {
            if (this->m_types.contains(typeName))
                return create<ast::ASTNodeTypeDecl>("", this->m_types[typeName]);
        }

        return nullptr;
    }

    // <Identifier[, Identifier]>
    void Parser::parseCustomTypeParameters(std::unique_ptr<ast::ASTNodeTypeDecl> &type) {
        if (auto actualType = dynamic_cast<ast::ASTNodeTypeDecl*>(type->getType().get()); actualType != nullptr)
            if (const auto &templateTypes = actualType->getTemplateParameters(); !templateTypes.empty()) {
                if (!MATCHES(sequence(tkn::Operator::BoolLessThan)))
                    err::P0002.throwError("Cannot use template type without template parameters.", {}, 1);

                u32 index = 0;
                do {
                    if (index >= templateTypes.size())
                        err::P0002.throwError(fmt::format("Provided more template parameters than expected. Type only has {} parameters", templateTypes.size()), {}, 1);

                    auto parameter = templateTypes[index];
                    if (auto type = dynamic_cast<ast::ASTNodeTypeDecl*>(parameter.get()); type != nullptr) {
                        auto newType = parseType();
                        if (newType->isForwardDeclared())
                            err::P0002.throwError("Cannot use forward declared type as template parameter.", {}, 1);

                        type->setType(std::move(newType), true);
                        type->setName("");
                    } else if (auto value = dynamic_cast<ast::ASTNodeLValueAssignment*>(parameter.get()); value != nullptr) {
                        value->setRValue(parseMathematicalExpression(true));
                    } else
                        err::P0002.throwError("Invalid template parameter type.", {}, 1);

                    index++;
                } while (MATCHES(sequence(tkn::Separator::Comma)));

                if (index < templateTypes.size())
                    err::P0002.throwError(fmt::format("Not enough template parameters provided, expected {} parameters.", templateTypes.size()), {}, 1);

                if (!MATCHES(sequence(tkn::Operator::BoolGreaterThan)))
                    err::P0002.throwError(fmt::format("Expected '>' to close template list, got {}.", getFormattedToken(0)), {}, 1);

                type = std::unique_ptr<ast::ASTNodeTypeDecl>(static_cast<ast::ASTNodeTypeDecl*>(type->clone().release()));
            }
    }

    // Identifier
    std::unique_ptr<ast::ASTNodeTypeDecl> Parser::parseCustomType() {
        auto baseTypeName = parseNamespaceResolution();
        auto type = getCustomType(baseTypeName);

        if (type == nullptr)
            err::P0003.throwError(fmt::format("Type {} has not been declared yet.", baseTypeName), fmt::format("If this type is being declared further down in the code, consider forward declaring it with 'using {};'.", baseTypeName), 1);

        parseCustomTypeParameters(type);

        return type;
    }

    // [be|le] <Identifier|u8|u16|u24|u32|u48|u64|u96|u128|s8|s16|s24|s32|s48|s64|s96|s128|float|double|str>
    std::unique_ptr<ast::ASTNodeTypeDecl> Parser::parseType() {
        bool reference = MATCHES(sequence(tkn::Keyword::Reference));

        std::optional<std::endian> endian;
        if (MATCHES(sequence(tkn::Keyword::LittleEndian)))
            endian = std::endian::little;
        else if (MATCHES(sequence(tkn::Keyword::BigEndian)))
            endian = std::endian::big;

        std::unique_ptr<ast::ASTNodeTypeDecl> result = nullptr;
        if (MATCHES(sequence(tkn::Literal::Identifier))) {    // Custom type
            result = parseCustomType();
        } else if (MATCHES(sequence(tkn::ValueType::Any))) {    // Builtin type
            auto type = getValue<Token::ValueType>(-1);
            result = create<ast::ASTNodeTypeDecl>("", create<ast::ASTNodeBuiltinType>(type));
        } else {
            err::P0002.throwError(fmt::format("Invalid type. Expected built-in type or custom type name, got {}.", getFormattedToken(0)), {}, 1);
        }

        result->setReference(reference);
        if (endian.has_value())
            result->setEndian(endian.value());
        return result;
    }

    // <(parseType), ...>
    std::vector<std::shared_ptr<ast::ASTNode>> Parser::parseTemplateList() {
        std::vector<std::shared_ptr<ast::ASTNode>> result;

        if (MATCHES(sequence(tkn::Operator::BoolLessThan))) {
            do {
                if (MATCHES(sequence(tkn::Literal::Identifier)))
                    result.push_back(createShared<ast::ASTNodeTypeDecl>(getValue<Token::Identifier>(-1).get()));
                else if (MATCHES(sequence(tkn::ValueType::Auto, tkn::Literal::Identifier))) {
                    result.push_back(createShared<ast::ASTNodeLValueAssignment>(getValue<Token::Identifier>(-1).get(), nullptr));
                }
                else
                    err::P0002.throwError(fmt::format("Expected identifier for template type, got {}.", getFormattedToken(0)), {}, 1);
            } while (MATCHES(sequence(tkn::Separator::Comma)));

            if (!MATCHES(sequence(tkn::Operator::BoolGreaterThan)))
                err::P0002.throwError(fmt::format("Expected '>' after template declaration, got {}.", getFormattedToken(0)), {}, 1);
        }

        return result;
    }

    // using Identifier = (parseType)
    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::parseUsingDeclaration() {
        auto name = getValue<Token::Identifier>(-1).get();

        auto templateList = this->parseTemplateList();

        if (!MATCHES(sequence(tkn::Operator::Assign)))
            err::P0002.throwError(fmt::format("Expected '=' after using declaration type name, got {}.", getFormattedToken(0)), {}, 1);

        auto type = addType(name, nullptr);
        type->setTemplateParameters(std::move(templateList));

        this->m_currTemplateType.push_back(type);
        auto replaceType = parseType();
        this->m_currTemplateType.pop_back();

        auto endian = replaceType->getEndian();
        type->setType(std::move(replaceType));

        if (endian.has_value())
            type->setEndian(*endian);

        return type;
    }

    // padding[(parseMathematicalExpression)]
    std::unique_ptr<ast::ASTNode> Parser::parsePadding() {
        std::unique_ptr<ast::ASTNode> size;
        if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
            size = parseWhileStatement();
        else
            size = parseMathematicalExpression();

        if (!MATCHES(sequence(tkn::Separator::RightBracket)))
            err::P0002.throwError(fmt::format("Expected ']' at end of array declaration, got {}.", getFormattedToken(0)), {}, 1);

        return create<ast::ASTNodeArrayVariableDecl>("$padding$", createShared<ast::ASTNodeTypeDecl>("", createShared<ast::ASTNodeBuiltinType>(Token::ValueType::Padding)), std::move(size));
    }

    // (parseType) Identifier
    std::unique_ptr<ast::ASTNode> Parser::parseMemberVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type, bool allowSection, bool constant, const std::string &identifier) {
        if (peek(tkn::Separator::Comma)) {

            std::vector<std::shared_ptr<ast::ASTNode>> variables;

            std::string variableName = identifier;
            do {
                if (MATCHES(sequence(tkn::Literal::Identifier)))
                    variableName = getValue<Token::Identifier>(-1).get();
                variables.push_back(createShared<ast::ASTNodeVariableDecl>(variableName, type, nullptr, nullptr, false, false, constant));
            } while (MATCHES(sequence(tkn::Separator::Comma)));

            return create<ast::ASTNodeMultiVariableDecl>(std::move(variables));
        } else if (MATCHES(sequence(tkn::Operator::At))) {
            if (constant)
                err::P0002.throwError("Cannot mark placed variable as 'const'.", "Variables placed in memory are always implicitly const.", 1);

            auto variableName = getValue<Token::Identifier>(-2).get();

            std::unique_ptr<ast::ASTNode> placementOffset, placementSection;
            placementOffset = parseMathematicalExpression();

            if (MATCHES(sequence(tkn::Keyword::In))) {
                if (!allowSection)
                    err::P0002.throwError("Cannot place a member variable in a separate section.", {}, 1);

                placementSection = parseMathematicalExpression();
            }

            return create<ast::ASTNodeVariableDecl>(variableName, type, std::move(placementOffset), std::move(placementSection), false, false, constant);
        } else if (MATCHES(sequence(tkn::Operator::Assign))) {
            std::vector<std::unique_ptr<ast::ASTNode>> compounds;
            compounds.push_back(create<ast::ASTNodeVariableDecl>(identifier, type, nullptr, create<ast::ASTNodeLiteral>(u128(ptrn::Pattern::PatternLocalSectionId)), false, false, constant));
            compounds.push_back(create<ast::ASTNodeLValueAssignment>(identifier, parseMathematicalExpression()));

            return create<ast::ASTNodeCompoundStatement>(std::move(compounds));
        }
        else
            return create<ast::ASTNodeVariableDecl>(identifier, type, nullptr, nullptr, false, false, constant);
    }

    // (parseType) Identifier[(parseMathematicalExpression)]
    std::unique_ptr<ast::ASTNode> Parser::parseMemberArrayVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type, bool allowSection, bool constant) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ast::ASTNode> size;

        if (!MATCHES(sequence(tkn::Separator::RightBracket))) {
            if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                err::P0002.throwError(fmt::format("Expected ']' at end of array declaration, got {}.", getFormattedToken(0)), {}, 1);
        }

        if (MATCHES(sequence(tkn::Operator::At))) {
            if (constant)
                err::P0002.throwError("Cannot mark placed variable as 'const'.", "Variables placed in memory are always implicitly const.", 1);

            std::unique_ptr<ast::ASTNode> placementOffset, placementSection;
            placementOffset = parseMathematicalExpression();

            if (MATCHES(sequence(tkn::Keyword::In))) {
                if (!allowSection)
                    err::P0002.throwError("Cannot place a member variable in a separate section.", {}, 1);

                placementSection = parseMathematicalExpression();
            }

            return create<ast::ASTNodeArrayVariableDecl>(name, type, std::move(size), std::move(placementOffset), std::move(placementSection), constant);
        }
        else
            return create<ast::ASTNodeArrayVariableDecl>(name, type, std::move(size), nullptr, nullptr, constant);
    }

    // (parseType) *Identifier : (parseType)
    std::unique_ptr<ast::ASTNode> Parser::parseMemberPointerVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();
        auto sizeType = parseType();

        if (MATCHES(sequence(tkn::Operator::At)))
            return create<ast::ASTNodePointerVariableDecl>(name, type, std::move(sizeType), parseMathematicalExpression());
        else
            return create<ast::ASTNodePointerVariableDecl>(name, type, std::move(sizeType));
    }

    // (parseType) *Identifier[[(parseMathematicalExpression)]]  : (parseType)
    std::unique_ptr<ast::ASTNode> Parser::parseMemberPointerArrayVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();
        std::unique_ptr<ast::ASTNode> size;

        if (!MATCHES(sequence(tkn::Separator::RightBracket))) {
            if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                err::P0002.throwError(fmt::format("Expected ']' at end of array declaration, got {}.", getFormattedToken(0)), {}, 1);
        }

        if (!MATCHES(sequence(tkn::Operator::Colon))) {
            err::P0002.throwError(fmt::format("Expected ':' after pointer definition, got {}.", getFormattedToken(0)), "A pointer requires a integral type to specify its own size.", 1);
        }

        auto sizeType = parseType();
        auto arrayType = createShared<ast::ASTNodeArrayVariableDecl>("", type, std::move(size));

        if (MATCHES(sequence(tkn::Operator::At)))
            return create<ast::ASTNodePointerVariableDecl>(name, std::move(arrayType), std::move(sizeType), parseMathematicalExpression());
        else
            return create<ast::ASTNodePointerVariableDecl>(name, std::move(arrayType), std::move(sizeType));
    }

    // [(parsePadding)|(parseMemberVariable)|(parseMemberArrayVariable)|(parseMemberPointerVariable)|(parseMemberArrayPointerVariable)]
    std::unique_ptr<ast::ASTNode> Parser::parseMember() {
        std::unique_ptr<ast::ASTNode> member;

        if (MATCHES(sequence(tkn::Operator::Dollar, tkn::Operator::Assign)))
            member = parseFunctionVariableAssignment("$");
        else if (MATCHES(sequence(tkn::Operator::Dollar) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            member = parseFunctionVariableCompoundAssignment("$");
        else if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Operator::Assign)))
            member = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (MATCHES(sequence(tkn::Literal::Identifier) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            member = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(-3).get());
        else if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any) || peek(tkn::Literal::Identifier)) {
            // Some kind of variable definition

            bool isFunction = false;

            if (peek(tkn::Literal::Identifier)) {
                auto originalPos = this->m_curr;
                this->m_curr++;
                parseNamespaceResolution();
                isFunction   = peek(tkn::Separator::LeftParenthesis);
                this->m_curr = originalPos;

                if (isFunction) {
                    this->m_curr++;
                    member = parseFunctionCall();
                }
            }


            if (!isFunction) {
                auto type = parseType();

                if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Separator::LeftBracket) && sequence<Not>(tkn::Separator::LeftBracket)))
                    member = parseMemberArrayVariable(std::move(type), false, false);
                else if (MATCHES(sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Operator::Colon)))
                    member = parseMemberPointerVariable(std::move(type));
                else if (MATCHES(sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Separator::LeftBracket)))
                    member = parseMemberPointerArrayVariable(std::move(type));
                else if (MATCHES(sequence(tkn::Literal::Identifier)))
                    member = parseMemberVariable(std::move(type), false, false, getValue<Token::Identifier>(-1).get());
                else
                    member = parseMemberVariable(std::move(type), false, false, "");
            }
        } else if (MATCHES(sequence(tkn::ValueType::Padding, tkn::Separator::LeftBracket)))
            member = parsePadding();
        else if (MATCHES(sequence(tkn::Keyword::If)))
            return parseConditional([this]() { return parseMember(); });
        else if (MATCHES(sequence(tkn::Keyword::Match)))
            return parseMatchStatement([this]() { return parseMember(); });
        else if (MATCHES(oneOf(tkn::Keyword::Return, tkn::Keyword::Break, tkn::Keyword::Continue)))
            member = parseFunctionControlFlowStatement();
        else
            err::P0002.throwError("Invalid struct member definition.", {}, 0);

        if (MATCHES(sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket)))
            parseAttribute(dynamic_cast<ast::Attributable *>(member.get()));

        if (!MATCHES(sequence(tkn::Separator::Semicolon)))
            err::P0002.throwError(fmt::format("Expected ';' at end of statement, got {}.", getFormattedToken(0)), {}, 1);

        // Consume superfluous semicolons
        while (MATCHES(sequence(tkn::Separator::Semicolon)))
            ;

        return member;
    }

    // struct Identifier { <(parseMember)...> }
    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::parseStruct() {
        const auto &typeName = getValue<Token::Identifier>(-1).get();

        auto typeDecl   = addType(typeName, create<ast::ASTNodeStruct>());
        auto structNode = static_cast<ast::ASTNodeStruct *>(typeDecl->getType().get());

        typeDecl->setTemplateParameters(this->parseTemplateList());

        this->m_currTemplateType.push_back(typeDecl);

        if (MATCHES(sequence(tkn::Operator::Colon))) {
            // Inheritance
            do {
                if (MATCHES(sequence(tkn::ValueType::Any)))
                    err::P0002.throwError("Cannot inherit from built-in type.", {}, 1);
                if (!MATCHES(sequence(tkn::Literal::Identifier)))
                    err::P0002.throwError(fmt::format("Expected type to inherit from, got {}.", getFormattedToken(0)), {}, 0);
                structNode->addInheritance(parseCustomType());
            } while (MATCHES(sequence(tkn::Separator::Comma)));
        }

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            err::P0002.throwError(fmt::format("Expected '{{' after struct declaration, got {}.", getFormattedToken(0)), {}, 1);

        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            structNode->addMember(parseMember());
        }
        this->m_currTemplateType.pop_back();

        return typeDecl;
    }

    // union Identifier { <(parseMember)...> }
    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::parseUnion() {
        const auto &typeName = getValue<Token::Identifier>(-1).get();

        auto typeDecl  = addType(typeName, create<ast::ASTNodeUnion>());
        auto unionNode = static_cast<ast::ASTNodeUnion *>(typeDecl->getType().get());

        typeDecl->setTemplateParameters(this->parseTemplateList());

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            err::P0002.throwError(fmt::format("Expected '{{' after union declaration, got {}.", getFormattedToken(0)), {}, 1);

        this->m_currTemplateType.push_back(typeDecl);
        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            unionNode->addMember(parseMember());
        }
        this->m_currTemplateType.pop_back();

        return typeDecl;
    }

    // enum Identifier : (parseType) { <<Identifier|Identifier = (parseMathematicalExpression)[,]>...> }
    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::parseEnum() {
        auto typeName = getValue<Token::Identifier>(-1).get();

        if (!MATCHES(sequence(tkn::Operator::Colon)))
            err::P0002.throwError(fmt::format("Expected ':' after enum declaration, got {}.", getFormattedToken(0)), {}, 1);

        auto underlyingType = parseType();
        if (underlyingType->getEndian().has_value())
            err::P0002.throwError("Underlying enum type may not have an endian specifier.", "Use the 'be' or 'le' keyword when declaring a variable instead.", 2);

        auto typeDecl = addType(typeName, create<ast::ASTNodeEnum>(std::move(underlyingType)));
        auto enumNode = static_cast<ast::ASTNodeEnum *>(typeDecl->getType().get());

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            err::P0002.throwError(fmt::format("Expected '{{' after enum declaration, got {}.", getFormattedToken(0)), {}, 1);

        std::unique_ptr<ast::ASTNode> lastEntry;
        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            std::unique_ptr<ast::ASTNode> enumValue;
            std::string name;

            if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Operator::Assign))) {
                name  = getValue<Token::Identifier>(-2).get();
                enumValue = parseMathematicalExpression();

                lastEntry = enumValue->clone();
            } else if (MATCHES(sequence(tkn::Literal::Identifier))) {
                name = getValue<Token::Identifier>(-1).get();
                if (enumNode->getEntries().empty())
                    enumValue = create<ast::ASTNodeLiteral>(u128(0));
                else
                    enumValue = create<ast::ASTNodeMathematicalExpression>(lastEntry->clone(), create<ast::ASTNodeLiteral>(u128(1)), Token::Operator::Plus);

                lastEntry = enumValue->clone();
            } else
                err::P0002.throwError("Invalid enum entry definition.", "Enum entries can consist of either just a name or a name followed by a value assignment.", 1);

            if (MATCHES(sequence(tkn::Separator::Dot, tkn::Separator::Dot, tkn::Separator::Dot))) {
                auto endExpr = parseMathematicalExpression();
                enumNode->addEntry(name, std::move(enumValue), std::move(endExpr));
            } else {
                auto clonedExpr = enumValue->clone();
                enumNode->addEntry(name, std::move(enumValue), std::move(clonedExpr));
            }

            if (!MATCHES(sequence(tkn::Separator::Comma))) {
                if (MATCHES(sequence(tkn::Separator::RightBrace)))
                    break;
                else
                    err::P0002.throwError(fmt::format("Expected ',' at end of enum entry, got {}.", getFormattedToken(0)), {}, 1);
            }
        }

        return typeDecl;
    }


    // [Identifier : (parseMathematicalExpression);|Identifier identifier;|(parseFunctionControlFlowStatement)|(parseIfStatement)|(parseMatchStatement)]
    std::unique_ptr<ast::ASTNode> Parser::parseBitfieldEntry() {
        std::unique_ptr<ast::ASTNode> member = nullptr;

        if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Operator::Assign))) {
            auto variableName = getValue<Token::Identifier>(-2).get();
            member = parseFunctionVariableAssignment(variableName);
        } else if (MATCHES(sequence(tkn::Literal::Identifier) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            member = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(-3).get());
        else if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Operator::Colon))) {
            auto fieldName = getValue<Token::Identifier>(-2).get();
            member = create<ast::ASTNodeBitfieldField>(fieldName, parseMathematicalExpression());
        } else if (MATCHES(sequence(tkn::ValueType::Padding, tkn::Operator::Colon)))
            member = create<ast::ASTNodeBitfieldField>("$padding$", parseMathematicalExpression());
        else if (peek(tkn::Literal::Identifier) || peek(tkn::ValueType::Any)) {
            std::unique_ptr<ast::ASTNodeTypeDecl> type = nullptr;

            if (MATCHES(sequence(tkn::ValueType::Any))) {
                type = create<ast::ASTNodeTypeDecl>("", create<ast::ASTNodeBuiltinType>(getValue<Token::ValueType>(-1)));
            } else if (MATCHES(sequence(tkn::Literal::Identifier))) {
                auto originalPosition = m_curr;
                auto name = parseNamespaceResolution();

                if (MATCHES(sequence(tkn::Separator::LeftParenthesis))) {
                    m_curr = originalPosition;
                    member = parseFunctionCall();
                } else {
                    type = getCustomType(name);

                    if (type == nullptr)
                        err::P0002.throwError(fmt::format("Expected a variable name followed by ':', a function call or a bitfield type name, got {}.", getFormattedToken(1)), {}, 1);
                    parseCustomTypeParameters(type);

                    ast::ASTNodeTypeDecl *topmostTypeDecl = type.get();
                    while (auto *parentTypeDecl = dynamic_cast<ast::ASTNodeTypeDecl*>(topmostTypeDecl->getType().get()))
                        topmostTypeDecl = parentTypeDecl;
                    if (auto *nestedBitfield = dynamic_cast<ast::ASTNodeBitfield*>(topmostTypeDecl->getType().get()); nestedBitfield != nullptr)
                        nestedBitfield->setNested();
                    else
                        err::P0003.throwError("Only bitfields can be nested within other bitfields.", {}, 1);
                }
            }

            if (type == nullptr) {
                // We called a function, do no more parsing.
            } else if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Separator::LeftBracket) && sequence<Not>(tkn::Separator::LeftBracket))){
                // (parseType) Identifier[[(parseMathematicalExpression)|(parseWhileStatement)]];
                auto fieldName = getValue<Token::Identifier>(-2).get();

                std::unique_ptr<ast::ASTNode> size;
                if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
                    size = parseWhileStatement();
                else
                    size = parseMathematicalExpression();

                if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                    err::P0002.throwError(fmt::format("Expected ']' at end of array declaration, got {}.", getFormattedToken(0)), {}, 1);

                member = create<ast::ASTNodeBitfieldArrayVariableDecl>(fieldName, move(type), move(size));
            } else if (MATCHES(sequence(tkn::Literal::Identifier))) {
                // (parseType) Identifier;
                if (MATCHES(sequence(tkn::Operator::At)))
                    err::P0002.throwError(fmt::format("Placement syntax is invalid within bitfields."), {}, 0);

                auto variableName = getValue<Token::Identifier>(-1).get();
                member = parseMemberVariable(std::move(type), false, false, variableName);
            } else
                err::P0002.throwError(fmt::format("Expected a variable name, got {}.", getFormattedToken(0)), {}, 0);
        } else if (MATCHES(sequence(tkn::Keyword::If)))
            return parseConditional([this]() { return parseBitfieldEntry(); });
        else if (MATCHES(sequence(tkn::Keyword::Match)))
            return parseMatchStatement([this]() { return parseBitfieldEntry(); });
        else if (MATCHES(oneOf(tkn::Keyword::Return, tkn::Keyword::Break, tkn::Keyword::Continue)))
            member = parseFunctionControlFlowStatement();
        else
            err::P0002.throwError("Invalid bitfield member definition.", {}, 0);

        if (MATCHES(sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket)))
            parseAttribute(dynamic_cast<ast::Attributable *>(member.get()));

        if (!MATCHES(sequence(tkn::Separator::Semicolon)))
            err::P0002.throwError(fmt::format("Expected ';' at end of statement, got {}.", getFormattedToken(0)), {}, 1);

        // Consume superfluous semicolons
        while (MATCHES(sequence(tkn::Separator::Semicolon)))
            ;

        return member;
    }

    // bitfield Identifier { ... }
    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::parseBitfield() {
        std::string typeName = getValue<Token::Identifier>(-1).get();

        auto typeDecl = addType(typeName, create<ast::ASTNodeBitfield>());
        typeDecl->setTemplateParameters(this->parseTemplateList());
        auto bitfieldNode = static_cast<ast::ASTNodeBitfield *>(typeDecl->getType().get());

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            err::P0002.throwError(fmt::format("Expected '{{' after bitfield declaration, got {}.", getFormattedToken(0)), {}, 1);

        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            bitfieldNode->addEntry(this->parseBitfieldEntry());

            // Consume superfluous semicolons
            while (MATCHES(sequence(tkn::Separator::Semicolon)))
                ;
        }

        return typeDecl;
    }

    // using Identifier;
    void Parser::parseForwardDeclaration() {
        std::string typeName = getNamespacePrefixedNames(getValue<Token::Identifier>(-1).get()).back();

        if (this->m_types.contains(typeName))
            return;

        this->m_types.insert({ typeName, createShared<ast::ASTNodeTypeDecl>(typeName) });
    }

    // (parseType) Identifier @ Integer
    std::unique_ptr<ast::ASTNode> Parser::parseVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        bool inVariable  = false;
        bool outVariable = false;

        auto name = getValue<Token::Identifier>(-1).get();

        std::unique_ptr<ast::ASTNode> placementOffset, placementSection;
        if (MATCHES(sequence(tkn::Operator::At))) {
            placementOffset = parseMathematicalExpression();

            if (MATCHES(sequence(tkn::Keyword::In)))
                placementSection = parseMathematicalExpression();
        } else if (MATCHES(sequence(tkn::Keyword::In))) {
            inVariable = true;
        } else if (MATCHES(sequence(tkn::Keyword::Out))) {
            outVariable = true;
        } else if (MATCHES(sequence(tkn::Operator::Assign))) {
            std::vector<std::unique_ptr<ast::ASTNode>> compounds;

            compounds.push_back(create<ast::ASTNodeVariableDecl>(name, type, std::move(placementOffset), nullptr, inVariable, outVariable));
            compounds.push_back(create<ast::ASTNodeLValueAssignment>(name, parseMathematicalExpression()));

            return create<ast::ASTNodeCompoundStatement>(std::move(compounds));
        }

        if (inVariable || outVariable) {
            bool invalidType = false;
            if (auto builtinType = dynamic_cast<ast::ASTNodeBuiltinType*>(type->getType().get()); builtinType != nullptr) {
                auto valueType = builtinType->getType();
                if (!Token::isInteger(valueType) && !Token::isFloatingPoint(valueType) && valueType != Token::ValueType::Boolean && valueType != Token::ValueType::Character)
                    invalidType = true;
            } else {
                invalidType = true;
            }

            if (invalidType)
                err::P0010.throwError("Invalid in/out parameter type.", "Allowed types are: 'char', 'bool', floating point types or integral types.", 1);
        }

        return create<ast::ASTNodeVariableDecl>(name, type, std::move(placementOffset), std::move(placementSection), inVariable, outVariable);
    }

    // (parseType) Identifier[[(parseMathematicalExpression)]] @ Integer
    std::unique_ptr<ast::ASTNode> Parser::parseArrayVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ast::ASTNode> size;

        if (!MATCHES(sequence(tkn::Separator::RightBracket))) {
            if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                err::P0002.throwError(fmt::format("Expected ']' at end of array declaration, got {}.", getFormattedToken(0)), {}, 1);
        }

        std::unique_ptr<ast::ASTNode> placementOffset, placementSection;
        if (MATCHES(sequence(tkn::Operator::At))) {
            placementOffset = parseMathematicalExpression();

            if (MATCHES(sequence(tkn::Keyword::In)))
                placementSection = parseMathematicalExpression();
        }

        return create<ast::ASTNodeArrayVariableDecl>(name, type, std::move(size), std::move(placementOffset), std::move(placementSection));
    }

    // (parseType) *Identifier : (parseType) @ Integer
    std::unique_ptr<ast::ASTNode> Parser::parsePointerVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        auto sizeType = parseType();

        if (!MATCHES(sequence(tkn::Operator::At)))
            err::P0002.throwError(fmt::format("Expected '@' after pointer placement, got {}.", getFormattedToken(0)), {}, 1);

        auto placementOffset = parseMathematicalExpression();

        std::unique_ptr<ast::ASTNode> placementSection;
        if (MATCHES(sequence(tkn::Keyword::In)))
            placementSection = parseMathematicalExpression();

        return create<ast::ASTNodePointerVariableDecl>(name, type, std::move(sizeType), std::move(placementOffset), std::move(placementSection));
    }

    // (parseType) *Identifier[[(parseMathematicalExpression)]] : (parseType) @ Integer
    std::unique_ptr<ast::ASTNode> Parser::parsePointerArrayVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ast::ASTNode> size;

        if (!MATCHES(sequence(tkn::Separator::RightBracket))) {
            if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                err::P0002.throwError(fmt::format("Expected ']' at end of array declaration, got {}.", getFormattedToken(0)), {}, 1);
        }

        if (!MATCHES(sequence(tkn::Operator::Colon))) {
            err::P0002.throwError(fmt::format("Expected ':' at end of pointer declaration, got {}.", getFormattedToken(0)), {}, 1);
        }

        auto sizeType = parseType();

        if (!MATCHES(sequence(tkn::Operator::At)))
            err::P0002.throwError(fmt::format("Expected '@' after array placement, got {}.", getFormattedToken(0)), {}, 1);

        auto placementOffset = parseMathematicalExpression();

        std::unique_ptr<ast::ASTNode> placementSection;
        if (MATCHES(sequence(tkn::Keyword::In)))
            placementSection = parseMathematicalExpression();

        return create<ast::ASTNodePointerVariableDecl>(name, createShared<ast::ASTNodeArrayVariableDecl>("", type, std::move(size)), std::move(sizeType), std::move(placementOffset), std::move(placementSection));
    }

    std::vector<std::shared_ptr<ast::ASTNode>> Parser::parseNamespace() {
        std::vector<std::shared_ptr<ast::ASTNode>> statements;

        if (!MATCHES(sequence(tkn::Literal::Identifier)))
            err::P0002.throwError(fmt::format("Expected namespace identifier, got {}.", getFormattedToken(0)), {}, 1);

        this->m_currNamespace.push_back(this->m_currNamespace.back());

        while (true) {
            this->m_currNamespace.back().push_back(getValue<Token::Identifier>(-1).get());

            if (MATCHES(sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier)))
                continue;
            else
                break;
        }

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            err::P0002.throwError(fmt::format("Expected '{{' at beginning of namespace, got {}.", getFormattedToken(0)), {}, 1);

        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            auto newStatements = parseStatements();
            std::move(newStatements.begin(), newStatements.end(), std::back_inserter(statements));
        }

        this->m_currNamespace.pop_back();

        return statements;
    }

    std::unique_ptr<ast::ASTNode> Parser::parsePlacement() {
        auto type = parseType();

        if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Separator::LeftBracket)))
            return parseArrayVariablePlacement(std::move(type));
        else if (MATCHES(sequence(tkn::Literal::Identifier)))
            return parseVariablePlacement(std::move(type));
        else if (MATCHES(sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Operator::Colon)))
            return parsePointerVariablePlacement(std::move(type));
        else if (MATCHES(sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Separator::LeftBracket)))
            return parsePointerArrayVariablePlacement(std::move(type));
        else
            err::P0002.throwError("Invalid placement sequence.", {}, 0);
    }

    /* Program */

    // <(parseUsingDeclaration)|(parseVariablePlacement)|(parseStruct)>
    std::vector<std::shared_ptr<ast::ASTNode>> Parser::parseStatements() {
        std::shared_ptr<ast::ASTNode> statement;
        bool requiresSemicolon = true;

        if (auto docComment = getDocComment(true); docComment.has_value())
            this->addGlobalDocComment(docComment->comment);

        if (MATCHES(sequence(tkn::Keyword::Using, tkn::Literal::Identifier) && (peek(tkn::Operator::Assign) || peek(tkn::Operator::BoolLessThan))))
            statement = parseUsingDeclaration();
        else if (MATCHES(sequence(tkn::Keyword::Using, tkn::Literal::Identifier)))
            parseForwardDeclaration();
        else if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any))
            statement = parsePlacement();
        else if (peek(tkn::Literal::Identifier) && !peek(tkn::Operator::Assign, 1) && !peek(tkn::Separator::Dot, 1)  && !peek(tkn::Separator::LeftBracket, 1)) {
            auto originalPos = this->m_curr;
            this->m_curr++;
            parseNamespaceResolution();
            bool isFunction = peek(tkn::Separator::LeftParenthesis);
            this->m_curr    = originalPos;

            if (isFunction) {
                this->m_curr++;
                statement = parseFunctionCall();
            } else
                statement = parsePlacement();
        }
        else if (MATCHES(sequence(tkn::Keyword::Struct, tkn::Literal::Identifier)))
            statement = parseStruct();
        else if (MATCHES(sequence(tkn::Keyword::Union, tkn::Literal::Identifier)))
            statement = parseUnion();
        else if (MATCHES(sequence(tkn::Keyword::Enum, tkn::Literal::Identifier)))
            statement = parseEnum();
        else if (MATCHES(sequence(tkn::Keyword::Bitfield, tkn::Literal::Identifier)))
            statement = parseBitfield();
        else if (MATCHES(sequence(tkn::Keyword::Function, tkn::Literal::Identifier)))
            statement = parseFunctionDefinition();
        else if (MATCHES(sequence(tkn::Keyword::Namespace)))
            return parseNamespace();
        else {
            statement = parseFunctionStatement();
            requiresSemicolon = false;
        }

        if (statement && MATCHES(sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket)))
            parseAttribute(dynamic_cast<ast::Attributable *>(statement.get()));

        if (requiresSemicolon && !MATCHES(sequence(tkn::Separator::Semicolon)))
            err::P0002.throwError(fmt::format("Expected ';' at end of statement, got {}.", getFormattedToken(0)), {}, 1);

        // Consume superfluous semicolons
        while (MATCHES(sequence(tkn::Separator::Semicolon)))
            ;

        if (!statement)
            return { };

        if (auto docComment = getDocComment(false); docComment.has_value()) {
            statement->setDocComment(docComment->comment);
        }
        statement->setShouldDocument(this->m_ignoreDocsCount == 0);

        return hlp::moveToVector(std::move(statement));
    }

    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::addType(const std::string &name, std::unique_ptr<ast::ASTNode> &&node, std::optional<std::endian> endian) {
        auto typeName = getNamespacePrefixedNames(name).back();

        if (this->m_types.contains(typeName) && this->m_types.at(typeName)->isForwardDeclared()) {
            this->m_types.at(typeName)->setType(std::move(node));

            return this->m_types.at(typeName);
        } else {
            if (this->m_types.contains(typeName))
                err::P0011.throwError(fmt::format("Type with name '{}' has already been declared.", typeName), "Try using another name for this type.");

            std::shared_ptr typeDecl = createShared<ast::ASTNodeTypeDecl>(typeName, std::move(node), endian);
            this->m_types.insert({ typeName, typeDecl });

            return typeDecl;
        }
    }

    // <(parseNamespace)...> EndOfProgram
    std::optional<std::vector<std::shared_ptr<ast::ASTNode>>> Parser::parse(const std::string &sourceCode, const std::vector<Token> &tokens) {
        this->m_curr = this->m_startToken = this->m_originalPosition = this->m_partOriginalPosition = tokens.begin();

        this->m_types.clear();
        this->m_currTemplateType.clear();
        this->m_matchedOptionals.clear();
        this->m_processedDocComments.clear();

        this->m_currNamespace.clear();
        this->m_currNamespace.emplace_back();

        try {
            auto program = parseTillToken(tkn::Separator::EndOfProgram);

            if (this->m_curr != tokens.end())
                err::P0002.throwError("Failed to parse entire input.", "Parsing stopped due to an invalid sequence before the entire input could be parsed. This is most likely a bug.", 1);

            for (auto &type : this->m_types)
                type.second->setCompleted();

            return program;
        } catch (err::ParserError::Exception &e) {
            this->m_curr -= e.getUserData();

            auto line   = this->m_curr->line;
            auto column = this->m_curr->column;

            this->m_error = err::PatternLanguageError(e.format(sourceCode, line, column), line, column);

            return std::nullopt;
        }
    }

}
