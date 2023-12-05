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

    /* Mathematical expressions */
    // ([(parseMathematicalExpression)|<(parseMathematicalExpression),...>(parseMathematicalExpression)]
    std::vector<std::unique_ptr<ast::ASTNode>> Parser::parseParameters() {
        std::vector<std::unique_ptr<ast::ASTNode>> params;

        while (!sequence(tkn::Separator::RightParenthesis)) {
            params.push_back(parseMathematicalExpression());

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
    std::unique_ptr<ast::ASTNode> Parser::parseFunctionCall() {
        std::string functionName = parseNamespaceResolution();

        if (!sequence(tkn::Separator::LeftParenthesis)) {
            error("Expected '(' after function name, got {}.", getFormattedToken(0));
            return nullptr;
        }

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

            if (sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier)) {
                name += "::";
                continue;
            }

            break;
        }

        return name;
    }

    std::unique_ptr<ast::ASTNode> Parser::parseScopeResolution() {
        std::string typeName;

        while (true) {
            typeName += getValue<Token::Identifier>(-1).get();

            if (sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier)) {
                if (peek(tkn::Operator::ScopeResolution, 0) && peek(tkn::Literal::Identifier, 1)) {
                    typeName += "::";
                    continue;
                }
                if (this->m_types.contains(typeName))
                    return create<ast::ASTNodeScopeResolution>(this->m_types[typeName], getValue<Token::Identifier>(-1).get());
                for (auto &potentialName : getNamespacePrefixedNames(typeName)) {
                    if (this->m_types.contains(potentialName)) {
                        return create<ast::ASTNodeScopeResolution>(this->m_types[potentialName], getValue<Token::Identifier>(-1).get());
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

    // <Integer|((parseMathematicalExpression))>
    std::unique_ptr<ast::ASTNode> Parser::parseFactor() {
        if (sequence(tkn::Literal::Numeric))
            return create<ast::ASTNodeLiteral>(getValue<Token::Literal>(-1));
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

            std::unique_ptr<ast::ASTNode> result;

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
                    result = create<ast::ASTNodeTypeOperator>(op, this->parseRValue());
                }
            } else if (oneOf(tkn::Keyword::Parent, tkn::Keyword::This)) {
                result = create<ast::ASTNodeTypeOperator>(op, this->parseRValue());
            } else if (op == Token::Operator::SizeOf && sequence(tkn::ValueType::Any)) {
                const auto type = getValue<Token::ValueType>(-1);

                result = create<ast::ASTNodeLiteral>(u128(Token::getTypeSize(type)));
            } else if (op == Token::Operator::TypeNameOf && sequence(tkn::ValueType::Any)) {
                const auto type = getValue<Token::ValueType>(-1);

                result = create<ast::ASTNodeLiteral>(Token::getTypeName(type));
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
        return nullptr;
    }

    std::unique_ptr<ast::ASTNode> Parser::parseCastExpression() {
        if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any)) {
            auto type        = parseType();
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

            return create<ast::ASTNodeCast>(std::move(node), std::move(type));
        }

        return parseFactor();
    }

    // <+|-|!|~> (parseFactor)
    std::unique_ptr<ast::ASTNode> Parser::parseUnaryExpression() {
        if (oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::BoolNot, tkn::Operator::BitNot)) {
            auto op = getValue<Token::Operator>(-1);

            return create<ast::ASTNodeMathematicalExpression>(create<ast::ASTNodeLiteral>(0), this->parseCastExpression(), op);
        }

        if (sequence(tkn::Literal::String)) {
            return this->parseStringLiteral();
        }

        return this->parseCastExpression();
    }

    // (parseUnaryExpression) <*|/|%> (parseUnaryExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseMultiplicativeExpression() {
        auto node = this->parseUnaryExpression();

        while (oneOf(tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent)) {
            auto op = getValue<Token::Operator>(-1);
            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseUnaryExpression(), op);
        }

        return node;
    }

    // (parseMultiplicativeExpression) <+|-> (parseMultiplicativeExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseAdditiveExpression() {
        auto node = this->parseMultiplicativeExpression();

        while (variant(tkn::Operator::Plus, tkn::Operator::Minus)) {
            auto op = getValue<Token::Operator>(-1);
            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseMultiplicativeExpression(), op);
        }

        return node;
    }

    // (parseAdditiveExpression) < >>|<< > (parseAdditiveExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseShiftExpression() {
        auto node = this->parseAdditiveExpression();

        while (true) {
            if (sequence(tkn::Operator::BoolGreaterThan, tkn::Operator::BoolGreaterThan)) {
                node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseAdditiveExpression(), Token::Operator::RightShift);
            } else if (sequence(tkn::Operator::BoolLessThan, tkn::Operator::BoolLessThan)) {
                node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseAdditiveExpression(), Token::Operator::LeftShift);
            } else {
                break;
            }
        }

        return node;
    }

    // (parseShiftExpression) & (parseShiftExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseBinaryAndExpression() {
        auto node = this->parseShiftExpression();

        while (sequence(tkn::Operator::BitAnd)) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseShiftExpression(), Token::Operator::BitAnd);
        }

        return node;
    }

    // (parseBinaryAndExpression) ^ (parseBinaryAndExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseBinaryXorExpression() {
        auto node = this->parseBinaryAndExpression();

        while (sequence(tkn::Operator::BitXor)) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBinaryAndExpression(), Token::Operator::BitXor);
        }

        return node;
    }

    // (parseBinaryXorExpression) | (parseBinaryXorExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseBinaryOrExpression(const bool inMatchRange) {
        auto node = this->parseBinaryXorExpression();

        if (inMatchRange && peek(tkn::Operator::BitOr))
            return node;
        while (sequence(tkn::Operator::BitOr)) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBinaryXorExpression(), Token::Operator::BitOr);
        }

        return node;
    }

    // (parseBinaryOrExpression) < >=|<=|>|< > (parseBinaryOrExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseRelationExpression(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseBinaryOrExpression(inMatchRange);

        if (inTemplate && peek(tkn::Operator::BoolGreaterThan))
            return node;

        while (true) {
            if (sequence(tkn::Operator::BoolGreaterThan, tkn::Operator::Assign))
                node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBinaryOrExpression(inMatchRange), Token::Operator::BoolGreaterThanOrEqual);
            else if (sequence(tkn::Operator::BoolLessThan, tkn::Operator::Assign))
                node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBinaryOrExpression(inMatchRange), Token::Operator::BoolLessThanOrEqual);
            else if (sequence(tkn::Operator::BoolGreaterThan))
                node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBinaryOrExpression(inMatchRange), Token::Operator::BoolGreaterThan);
            else if (sequence(tkn::Operator::BoolLessThan))
                node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBinaryOrExpression(inMatchRange), Token::Operator::BoolLessThan);
            else
                break;
        }

        return node;
    }

    // (parseRelationExpression) <==|!=> (parseRelationExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseEqualityExpression(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseRelationExpression(inTemplate, inMatchRange);

        while (MATCHES(sequence(tkn::Operator::BoolEqual) || sequence(tkn::Operator::BoolNotEqual))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseRelationExpression(inTemplate, inMatchRange), op);
        }

        return node;
    }

    // (parseEqualityExpression) && (parseEqualityExpression)
    std::unique_ptr<ast::ASTNode> Parser::parseBooleanAnd(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseEqualityExpression(inTemplate, inMatchRange);

        while (sequence(tkn::Operator::BoolAnd)) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseEqualityExpression(inTemplate, inMatchRange), Token::Operator::BoolAnd);
        }

        return node;
    }

    // (parseBooleanAnd) ^^ (parseBooleanAnd)
    std::unique_ptr<ast::ASTNode> Parser::parseBooleanXor(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseBooleanAnd(inTemplate, inMatchRange);

        while (sequence(tkn::Operator::BoolXor)) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBooleanAnd(inTemplate, inMatchRange), Token::Operator::BoolXor);
        }

        return node;
    }

    // (parseBooleanXor) || (parseBooleanXor)
    std::unique_ptr<ast::ASTNode> Parser::parseBooleanOr(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseBooleanXor(inTemplate, inMatchRange);

        while (sequence(tkn::Operator::BoolOr)) {
            node = create<ast::ASTNodeMathematicalExpression>(std::move(node), this->parseBooleanXor(inTemplate, inMatchRange), Token::Operator::BoolOr);
        }

        return node;
    }

    // (parseBooleanOr) ? (parseBooleanOr) : (parseBooleanOr)
    std::unique_ptr<ast::ASTNode> Parser::parseTernaryConditional(const bool inTemplate, const bool inMatchRange) {
        auto node = this->parseBooleanOr(inTemplate, inMatchRange);

        while (sequence(tkn::Operator::TernaryConditional)) {
            auto second = this->parseBooleanOr(inTemplate, inMatchRange);

            if (!sequence(tkn::Operator::Colon)) {
                error("Expected ':' after ternary condition, got {}", getFormattedToken(0));
                return nullptr;
            }

            auto third = this->parseBooleanOr(inTemplate, inMatchRange);
            node = create<ast::ASTNodeTernaryExpression>(std::move(node), std::move(second), std::move(third), Token::Operator::TernaryConditional);
        }

        return node;
    }

    // (parseTernaryConditional)
    std::unique_ptr<ast::ASTNode> Parser::parseMathematicalExpression(const bool inTemplate, const bool inMatchRange) {
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

            auto attribute = parseNamespaceResolution();

            if (sequence(tkn::Separator::LeftParenthesis)) {
                std::vector<std::unique_ptr<ast::ASTNode>> args;
                do {
                    args.push_back(parseMathematicalExpression());
                } while (sequence(tkn::Separator::Comma));

                if (!sequence(tkn::Separator::RightParenthesis)) {
                    error("Expected ')', got {}", getFormattedToken(0));
                    return;
                }

                currNode->addAttribute(create<ast::ASTNodeAttribute>(attribute, std::move(args)));
            } else
                currNode->addAttribute(create<ast::ASTNodeAttribute>(attribute));

        } while (sequence(tkn::Separator::Comma));

        if (!sequence(tkn::Separator::RightBracket, tkn::Separator::RightBracket))
            error("Expected ']]' after attribute, got {}.", getFormattedToken(0));
    }

    /* Functions */

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionDefinition() {
        const auto &functionName = getValue<Token::Identifier>(-1).get();
        std::vector<std::pair<std::string, std::unique_ptr<ast::ASTNode>>> params;
        std::optional<std::string> parameterPack;

        if (!sequence(tkn::Separator::LeftParenthesis)) {
            error("Expected '(' after function declaration, got {}.", getFormattedToken(0));
        }

        // Parse parameter list
        const bool hasParams        = !peek(tkn::Separator::RightParenthesis);
        u32 unnamedParamCount = 0;
        std::vector<std::unique_ptr<ast::ASTNode>> defaultParameters;

        while (hasParams) {
            if (sequence(tkn::ValueType::Auto, tkn::Separator::Dot, tkn::Separator::Dot, tkn::Separator::Dot, tkn::Literal::Identifier)) {
                parameterPack = getValue<Token::Identifier>(-1).get();

                if (sequence(tkn::Separator::Comma))
                    error("Parameter pack can only appear at the end of the parameter list.");

                break;
            }

            auto type = parseType();

            if (sequence(tkn::Literal::Identifier))
                params.emplace_back(getValue<Token::Identifier>(-1).get(), std::move(type));
            else {
                params.emplace_back(std::to_string(unnamedParamCount), std::move(type));
                unnamedParamCount++;
            }

            if (sequence(tkn::Operator::Assign)) {
                // Parse default parameters
                defaultParameters.push_back(parseMathematicalExpression());
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
        std::vector<std::unique_ptr<ast::ASTNode>> body;

        while (!sequence(tkn::Separator::RightBrace)) {
            body.push_back(this->parseFunctionStatement());
        }

        return create<ast::ASTNodeFunctionDefinition>(getNamespacePrefixedNames(functionName).back(), std::move(params), std::move(body), parameterPack, std::move(defaultParameters));
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionVariableDecl(const bool constant) {
        std::unique_ptr<ast::ASTNode> statement;
        auto type = parseType();

        if (sequence(tkn::Literal::Identifier)) {
            auto identifier = getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(tkn::Separator::LeftBracket) && !peek(tkn::Separator::LeftBracket))) {
                statement = parseMemberArrayVariable(std::move(type), true, constant);
            } else {
                statement = parseMemberVariable(std::move(type), true, constant, identifier);

                if (sequence(tkn::Operator::Assign)) {
                    auto expression = parseMathematicalExpression();

                    std::vector<std::unique_ptr<ast::ASTNode>> compoundStatement;
                    {
                        compoundStatement.push_back(std::move(statement));
                        compoundStatement.push_back(create<ast::ASTNodeLValueAssignment>(identifier, std::move(expression)));
                    }

                    statement = create<ast::ASTNodeCompoundStatement>(std::move(compoundStatement));
                }
            }
        } else {
            error("Expected identifier in variable declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        return statement;
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionStatement(bool needsSemicolon) {
        std::unique_ptr<ast::ASTNode> statement;

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
        } else if (MATCHES(sequence(tkn::Literal::Identifier) && (peek(tkn::Separator::Dot) || peek(tkn::Separator::LeftBracket)))) {
            auto lhs = parseRValue();

            if (!sequence(tkn::Operator::Assign)) {
                error("Expected value after '=' in variable assignment, got {}.", getFormattedToken(0));
                return nullptr;
            }

            auto rhs = parseMathematicalExpression();

            statement = create<ast::ASTNodeRValueAssignment>(std::move(lhs), std::move(rhs));
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
            error("Invalid function statement.");
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

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionVariableAssignment(const std::string &lvalue) {
        auto rvalue = this->parseMathematicalExpression();

        return create<ast::ASTNodeLValueAssignment>(lvalue, std::move(rvalue));
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionVariableCompoundAssignment(const std::string &lvalue) {
        auto op = getValue<Token::Operator>(-2);

        if (op == Token::Operator::BoolLessThan)
            op = Token::Operator::LeftShift;
        else if (op == Token::Operator::BoolGreaterThan)
            op = Token::Operator::RightShift;

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
        else {
            errorDesc("Invalid control flow statement.", "Control flow statements include 'return', 'break' and 'continue'.");
            return nullptr;
        }

        if (peek(tkn::Separator::Semicolon))
            return create<ast::ASTNodeControlFlowStatement>(type, nullptr);
        if (type == ControlFlowStatement::Return)
            return create<ast::ASTNodeControlFlowStatement>(type, this->parseMathematicalExpression());

        error("Return value can only be passed to a 'return' statement.");
        return nullptr;
    }

    std::vector<std::unique_ptr<ast::ASTNode>> Parser::parseStatementBody(const std::function<std::unique_ptr<ast::ASTNode>()> &memberParser) {
        std::vector<std::unique_ptr<ast::ASTNode>> body;

        if (sequence(tkn::Separator::LeftBrace)) {
            while (!sequence(tkn::Separator::RightBrace)) {
                body.push_back(memberParser());
            }
        } else {
            body.push_back(memberParser());
        }

        return body;
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionWhileLoop() {
        auto condition = parseMathematicalExpression();

        if (!sequence(tkn::Separator::RightParenthesis)) {
            error("Expected ')' at end of while head, got {}.", getFormattedToken(0));
            return nullptr;
        }

        std::vector<std::unique_ptr<ast::ASTNode>> body = parseStatementBody([&] { return parseFunctionStatement(); });

        return create<ast::ASTNodeWhileStatement>(std::move(condition), std::move(body));
    }

    std::unique_ptr<ast::ASTNode> Parser::parseFunctionForLoop() {
        auto preExpression = parseFunctionStatement(false);

        if (!sequence(tkn::Separator::Comma)) {
            error("Expected ',' after for loop expression, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto condition = parseMathematicalExpression();

        if (!sequence(tkn::Separator::Comma)) {
            error("Expected ',' after for loop expression, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto postExpression = parseFunctionStatement(false);

        if (!sequence(tkn::Separator::RightParenthesis)) {
            error("Expected ')' at end of for loop head, got {}.", getFormattedToken(0));
            return nullptr;
        }

        std::vector<std::unique_ptr<ast::ASTNode>> body = parseStatementBody([&] { return parseFunctionStatement(); });

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
        if (!sequence(tkn::Separator::LeftParenthesis)) {
            error("Expected '(' after 'if', got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto condition = parseMathematicalExpression();

        if (!sequence(tkn::Separator::RightParenthesis)) {
            error("Expected ')' after if head, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto trueBody = parseStatementBody(memberParser);

        std::vector<std::unique_ptr<ast::ASTNode>> falseBody;
        if (sequence(tkn::Keyword::Else))
            falseBody = parseStatementBody(memberParser);

        return create<ast::ASTNodeConditionalStatement>(std::move(condition), std::move(trueBody), std::move(falseBody));
    }

    std::pair<std::unique_ptr<ast::ASTNode>, bool> Parser::parseCaseParameters(const std::vector<std::unique_ptr<ast::ASTNode>> &matchParameters) {
        std::unique_ptr<ast::ASTNode> condition = nullptr;

        size_t caseIndex = 0;
        bool isDefault = true;
        while (!sequence(tkn::Separator::RightParenthesis)) {
            if (caseIndex >= matchParameters.size()) {
                error("Size of case parameters bigger than size of match condition.");
                break;
            }

            std::unique_ptr<ast::ASTNode> currentCondition = nullptr;
            if (sequence(tkn::Keyword::Underscore)) {
                // if '_' is found, act as wildcard, push literal(true)
                currentCondition = std::make_unique<ast::ASTNodeLiteral>(true);
            } else {
                isDefault = false;
                auto &param = matchParameters[caseIndex];

                do {
                    auto first = parseMathematicalExpression(false, true);
                    auto nextCondition = [&]() {
                        if (sequence(tkn::Separator::Dot, tkn::Separator::Dot, tkn::Separator::Dot)) {
                            // range a ... b should compile to
                            // param >= a && param <= b
                            auto last = parseMathematicalExpression(false, true);
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
                break;
            }
            if (sequence(tkn::Separator::RightParenthesis))
                break;
            if (!sequence(tkn::Separator::Comma)) {
                error("Expected ',' in-between parameters, got {}.", getFormattedToken(0));
                break;
            }
        }

        if (caseIndex != matchParameters.size()) {
            error("Size of case parameters smaller than size of match condition.");
            return {};
        }

        return {std::move(condition), isDefault};
    }

    // match ((parseParameters)) { (parseParameters { (parseMember) })*, default { (parseMember) } }
    std::unique_ptr<ast::ASTNode> Parser::parseMatchStatement(const std::function<std::unique_ptr<ast::ASTNode>()> &memberParser) {
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

            if (!sequence(tkn::Operator::Colon)) {
                error("Expected ':' after case condition, got {}.", getFormattedToken(0));
                break;
            }

            auto body = parseStatementBody(memberParser);

            if (isDefault)
                defaultCase = ast::MatchCase(std::move(caseCondition), std::move(body));
            else
                cases.emplace_back(std::move(caseCondition), std::move(body));

            if (sequence(tkn::Separator::RightBrace))
                break;
        }

        return create<ast::ASTNodeMatchStatement>(std::move(cases), std::move(defaultCase));
    }

    // try { (parseMember) } catch { (parseMember) }
    std::unique_ptr<ast::ASTNode> Parser::parseTryCatchStatement(const std::function<std::unique_ptr<ast::ASTNode>()> &memberParser) {
        std::vector<std::unique_ptr<ast::ASTNode>> tryBody, catchBody;
        while (!sequence(tkn::Separator::RightBrace)) {
            tryBody.emplace_back(memberParser());
        }

        if (sequence(tkn::Keyword::Catch)) {
            if (!sequence(tkn::Separator::LeftBrace)) {
                error("Expected '{{' after catch, got {}.", getFormattedToken(0));
                return nullptr;
            }

            while (!sequence(tkn::Separator::RightBrace)) {
                catchBody.emplace_back(memberParser());
            }
        }


        return create<ast::ASTNodeTryCatchStatement>(std::move(tryBody), std::move(catchBody));
    }

    // while ((parseMathematicalExpression))
    std::unique_ptr<ast::ASTNode> Parser::parseWhileStatement() {
        auto condition = parseMathematicalExpression();

        if (!sequence(tkn::Separator::RightParenthesis)) {
            error("Expected ')' after while head, got {}.", getFormattedToken(0));
            return nullptr;
        }

        return create<ast::ASTNodeWhileStatement>(std::move(condition), std::vector<std::unique_ptr<ast::ASTNode>>{});
    }

    /* Type declarations */

    std::unique_ptr<ast::ASTNodeTypeDecl> Parser::getCustomType(const std::string &baseTypeName) {
        if (!this->m_currTemplateType.empty())
            for (const auto &templateParameter : this->m_currTemplateType.front()->getTemplateParameters()) {
                if (const auto templateType = dynamic_cast<ast::ASTNodeTypeDecl*>(templateParameter.get()); templateType != nullptr)
                    if (templateType->getName() == baseTypeName)
                        return create<ast::ASTNodeTypeDecl>("", templateParameter);
            }

        for (const auto &typeName : getNamespacePrefixedNames(baseTypeName)) {
            if (this->m_types.contains(typeName)) {
                auto type = this->m_types[typeName];

                return create<ast::ASTNodeTypeDecl>("", type);
            }
        }

        return nullptr;
    }

    // <Identifier[, Identifier]>
    void Parser::parseCustomTypeParameters(std::unique_ptr<ast::ASTNodeTypeDecl> &type) {
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
                        if (newType->isForwardDeclared()) {
                            error("Cannot use forward declared type as template parameter.");
                        }

                        typeDecl->setType(std::move(newType), true);
                        typeDecl->setName("");
                    } else if (const auto value = dynamic_cast<ast::ASTNodeLValueAssignment*>(parameter.get()); value != nullptr) {
                        value->setRValue(parseMathematicalExpression(true));
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

                type = std::unique_ptr<ast::ASTNodeTypeDecl>(static_cast<ast::ASTNodeTypeDecl*>(type->clone().release()));
            }
    }

    // Identifier
    std::unique_ptr<ast::ASTNodeTypeDecl> Parser::parseCustomType() {
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
    std::unique_ptr<ast::ASTNodeTypeDecl> Parser::parseType() {
        const bool reference = sequence(tkn::Keyword::Reference);

        std::optional<std::endian> endian;
        if (sequence(tkn::Keyword::LittleEndian))
            endian = std::endian::little;
        else if (sequence(tkn::Keyword::BigEndian))
            endian = std::endian::big;

        std::unique_ptr<ast::ASTNodeTypeDecl> result = nullptr;
        if (sequence(tkn::Literal::Identifier)) {    // Custom type
            result = parseCustomType();
        } else if (sequence(tkn::ValueType::Any)) {    // Builtin type
            auto type = getValue<Token::ValueType>(-1);
            result = create<ast::ASTNodeTypeDecl>(Token::getTypeName(type), create<ast::ASTNodeBuiltinType>(type));
        } else {
            error("Invalid type. Expected built-in type or custom type name, got {}.", getFormattedToken(0));
            return nullptr;
        }

        result->setReference(reference);
        if (endian.has_value())
            result->setEndian(endian.value());
        return result;
    }

    // <(parseType), ...>
    std::vector<std::shared_ptr<ast::ASTNode>> Parser::parseTemplateList() {
        std::vector<std::shared_ptr<ast::ASTNode>> result;

        if (sequence(tkn::Operator::BoolLessThan)) {
            do {
                if (sequence(tkn::Literal::Identifier))
                    result.push_back(createShared<ast::ASTNodeTypeDecl>(getValue<Token::Identifier>(-1).get()));
                else if (sequence(tkn::ValueType::Auto, tkn::Literal::Identifier)) {
                    result.push_back(createShared<ast::ASTNodeLValueAssignment>(getValue<Token::Identifier>(-1).get(), nullptr));
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

    // using Identifier = (parseType)
    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::parseUsingDeclaration() {
        const auto name = getValue<Token::Identifier>(-1).get();

        auto templateList = this->parseTemplateList();

        if (!sequence(tkn::Operator::Assign)) {
            error("Expected '=' after using declaration type name, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto type = addType(name, nullptr);
        type->setTemplateParameters(std::move(templateList));

        this->m_currTemplateType.push_back(type);
        auto replaceType = parseType();
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
    std::unique_ptr<ast::ASTNode> Parser::parsePadding() {
        std::unique_ptr<ast::ASTNode> size;
        if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
            size = parseWhileStatement();
        else
            size = parseMathematicalExpression();

        if (!sequence(tkn::Separator::RightBracket)) {
            error("Expected ']' at end of array declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        return create<ast::ASTNodeArrayVariableDecl>("$padding$", createShared<ast::ASTNodeTypeDecl>("", createShared<ast::ASTNodeBuiltinType>(Token::ValueType::Padding)), std::move(size));
    }

    // (parseType) Identifier
    std::unique_ptr<ast::ASTNode> Parser::parseMemberVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type, bool allowSection, bool constant, const std::string &identifier) {
        if (peek(tkn::Separator::Comma)) {

            std::vector<std::shared_ptr<ast::ASTNode>> variables;

            std::string variableName = identifier;
            do {
                if (sequence(tkn::Literal::Identifier))
                    variableName = getValue<Token::Identifier>(-1).get();
                variables.push_back(createShared<ast::ASTNodeVariableDecl>(variableName, type, nullptr, nullptr, false, false, constant));
            } while (sequence(tkn::Separator::Comma));

            return create<ast::ASTNodeMultiVariableDecl>(std::move(variables));
        }
        if (sequence(tkn::Operator::At)) {
            if (constant) {
                errorDesc("Cannot mark placed variable as 'const'.", "Variables placed in memory are always implicitly const.");
                return nullptr;
            }

            auto variableName = getValue<Token::Identifier>(-2).get();

            std::unique_ptr<ast::ASTNode> placementSection;
            std::unique_ptr<ast::ASTNode> placementOffset = parseMathematicalExpression();

            if (sequence(tkn::Keyword::In)) {
                if (!allowSection) {
                    error("Cannot place a member variable in a separate section.");
                    return nullptr;
                }

                placementSection = parseMathematicalExpression();
            }

            return create<ast::ASTNodeVariableDecl>(variableName, type, std::move(placementOffset), std::move(placementSection), false, false, constant);
        }
        if (sequence(tkn::Operator::Assign)) {
            std::vector<std::unique_ptr<ast::ASTNode>> compounds;
            compounds.push_back(create<ast::ASTNodeVariableDecl>(identifier, type, nullptr, create<ast::ASTNodeLiteral>(u128(ptrn::Pattern::PatternLocalSectionId)), false, false, constant));
            compounds.push_back(create<ast::ASTNodeLValueAssignment>(identifier, parseMathematicalExpression()));

            return create<ast::ASTNodeCompoundStatement>(std::move(compounds));
        }

        return create<ast::ASTNodeVariableDecl>(identifier, type, nullptr, nullptr, false, false, constant);
    }

    // (parseType) Identifier[(parseMathematicalExpression)]
    std::unique_ptr<ast::ASTNode> Parser::parseMemberArrayVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type, bool allowSection, bool constant) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ast::ASTNode> size;

        if (!sequence(tkn::Separator::RightBracket)) {
            if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!sequence(tkn::Separator::RightBracket)) {
                error("Expected ']' at end of array declaration, got {}.", getFormattedToken(0));
                return nullptr;
            }
        }

        if (sequence(tkn::Operator::At)) {
            if (constant)
                error("Cannot mark placed variable as 'const'.", "Variables placed in memory are always implicitly const.");

            std::unique_ptr<ast::ASTNode> placementSection;
            std::unique_ptr<ast::ASTNode> placementOffset = parseMathematicalExpression();

            if (sequence(tkn::Keyword::In)) {
                if (!allowSection) {
                    error("Cannot place a member variable in a separate section.");
                    return nullptr;
                }

                placementSection = parseMathematicalExpression();
            }

            return create<ast::ASTNodeArrayVariableDecl>(name, type, std::move(size), std::move(placementOffset), std::move(placementSection), constant);
        }

        return create<ast::ASTNodeArrayVariableDecl>(name, type, std::move(size), nullptr, nullptr, constant);
    }

    // (parseType) *Identifier : (parseType)
    std::unique_ptr<ast::ASTNode> Parser::parseMemberPointerVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();
        auto sizeType = parseType();

        if (sequence(tkn::Operator::At))
            return create<ast::ASTNodePointerVariableDecl>(name, type, std::move(sizeType), parseMathematicalExpression());

        return create<ast::ASTNodePointerVariableDecl>(name, type, std::move(sizeType));
    }

    // (parseType) *Identifier[[(parseMathematicalExpression)]]  : (parseType)
    std::unique_ptr<ast::ASTNode> Parser::parseMemberPointerArrayVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();
        std::unique_ptr<ast::ASTNode> size;

        if (!sequence(tkn::Separator::RightBracket)) {
            if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

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
        auto arrayType = createShared<ast::ASTNodeArrayVariableDecl>("", type, std::move(size));

        if (sequence(tkn::Operator::At))
            return create<ast::ASTNodePointerVariableDecl>(name, std::move(arrayType), std::move(sizeType), parseMathematicalExpression());

        return create<ast::ASTNodePointerVariableDecl>(name, std::move(arrayType), std::move(sizeType));
    }

    // [(parsePadding)|(parseMemberVariable)|(parseMemberArrayVariable)|(parseMemberPointerVariable)|(parseMemberArrayPointerVariable)]
    std::unique_ptr<ast::ASTNode> Parser::parseMember() {
        std::unique_ptr<ast::ASTNode> member;

        if (sequence(tkn::Operator::Dollar, tkn::Operator::Assign))
            member = parseFunctionVariableAssignment("$");
        else if (parseCompoundAssignment(tkn::Operator::Dollar).has_value())
            member = parseFunctionVariableCompoundAssignment("$");
        else if (sequence(tkn::Literal::Identifier, tkn::Operator::Assign))
            member = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (const auto identifierOffset = parseCompoundAssignment(tkn::Literal::Identifier); identifierOffset.has_value())
            member = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(*identifierOffset).get());
        else if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any) || peek(tkn::Literal::Identifier)) {
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
                auto type = parseType();

                if (MATCHES(sequence(tkn::Literal::Identifier, tkn::Separator::LeftBracket) && sequence<Not>(tkn::Separator::LeftBracket)))
                    member = parseMemberArrayVariable(std::move(type), false, false);
                else if (sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Operator::Colon))
                    member = parseMemberPointerVariable(std::move(type));
                else if (sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Separator::LeftBracket))
                    member = parseMemberPointerArrayVariable(std::move(type));
                else if (sequence(tkn::Literal::Identifier))
                    member = parseMemberVariable(std::move(type), false, false, getValue<Token::Identifier>(-1).get());
                else
                    member = parseMemberVariable(std::move(type), false, false, "");
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
            error("Invalid struct member definition.");
            return nullptr;
        }

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
    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::parseStruct() {
        const auto &typeName = getValue<Token::Identifier>(-1).get();

        auto typeDecl   = addType(typeName, create<ast::ASTNodeStruct>());
        const auto structNode = static_cast<ast::ASTNodeStruct *>(typeDecl->getType().get());

        typeDecl->setTemplateParameters(this->parseTemplateList());

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
                structNode->addInheritance(parseCustomType());
            } while (sequence(tkn::Separator::Comma));
        }

        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' after struct declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        while (!sequence(tkn::Separator::RightBrace)) {
            structNode->addMember(parseMember());
        }
        this->m_currTemplateType.pop_back();

        return typeDecl;
    }

    // union Identifier { <(parseMember)...> }
    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::parseUnion() {
        const auto &typeName = getValue<Token::Identifier>(-1).get();

        auto typeDecl  = addType(typeName, create<ast::ASTNodeUnion>());
        const auto unionNode = static_cast<ast::ASTNodeUnion *>(typeDecl->getType().get());

        typeDecl->setTemplateParameters(this->parseTemplateList());

        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' after union declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        this->m_currTemplateType.push_back(typeDecl);
        while (!sequence(tkn::Separator::RightBrace)) {
            unionNode->addMember(parseMember());
        }
        this->m_currTemplateType.pop_back();

        return typeDecl;
    }

    // enum Identifier : (parseType) { <<Identifier|Identifier = (parseMathematicalExpression)[,]>...> }
    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::parseEnum() {
        const auto typeName = getValue<Token::Identifier>(-1).get();

        if (!sequence(tkn::Operator::Colon)) {
            error("Expected ':' after enum declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto underlyingType = parseType();
        if (underlyingType->getEndian().has_value()) {
            errorDesc("Underlying enum type may not have an endian specifier.", "Use the 'be' or 'le' keyword when declaring a variable instead.");
            return nullptr;
        }

        auto typeDecl = addType(typeName, create<ast::ASTNodeEnum>(std::move(underlyingType)));
        const auto enumNode = static_cast<ast::ASTNodeEnum *>(typeDecl->getType().get());

        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' after enum declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        std::unique_ptr<ast::ASTNode> lastEntry;
        while (!sequence(tkn::Separator::RightBrace)) {
            std::unique_ptr<ast::ASTNode> enumValue;
            std::string name;

            if (sequence(tkn::Literal::Identifier, tkn::Operator::Assign)) {
                name  = getValue<Token::Identifier>(-2).get();
                enumValue = parseMathematicalExpression();

                lastEntry = enumValue->clone();
            } else if (sequence(tkn::Literal::Identifier)) {
                name = getValue<Token::Identifier>(-1).get();
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
                enumNode->addEntry(name, std::move(enumValue), std::move(endExpr));
            } else {
                auto clonedExpr = enumValue->clone();
                enumNode->addEntry(name, std::move(enumValue), std::move(clonedExpr));
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
    std::unique_ptr<ast::ASTNode> Parser::parseBitfieldEntry() {
        std::unique_ptr<ast::ASTNode> member = nullptr;

        if (sequence(tkn::Literal::Identifier, tkn::Operator::Assign)) {
            const auto variableName = getValue<Token::Identifier>(-2).get();
            member = parseFunctionVariableAssignment(variableName);
        } else if (const auto identifierOffset = parseCompoundAssignment(tkn::Literal::Identifier); identifierOffset.has_value())
            member = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(*identifierOffset).get());
        else if (MATCHES(optional(tkn::Keyword::Unsigned) && sequence(tkn::Literal::Identifier, tkn::Operator::Colon))) {
            auto fieldName = getValue<Token::Identifier>(-2).get();
            member = create<ast::ASTNodeBitfieldField>(fieldName, parseMathematicalExpression());
        } else if (sequence(tkn::Keyword::Signed, tkn::Literal::Identifier, tkn::Operator::Colon)) {
            auto fieldName = getValue<Token::Identifier>(-2).get();
            member = create<ast::ASTNodeBitfieldFieldSigned>(fieldName, parseMathematicalExpression());
        } else if (sequence(tkn::ValueType::Padding, tkn::Operator::Colon))
            member = create<ast::ASTNodeBitfieldField>("$padding$", parseMathematicalExpression());
        else if (peek(tkn::Literal::Identifier) || peek(tkn::ValueType::Any)) {
            std::unique_ptr<ast::ASTNodeTypeDecl> type = nullptr;

            if (sequence(tkn::ValueType::Any)) {
                const auto typeToken = getValue<Token::ValueType>(-1);
                type = create<ast::ASTNodeTypeDecl>(Token::getTypeName(typeToken), create<ast::ASTNodeBuiltinType>(typeToken));
            } else if (sequence(tkn::Literal::Identifier)) {
                const auto originalPosition = m_curr;
                auto name = parseNamespaceResolution();

                if (sequence(tkn::Separator::LeftParenthesis)) {
                    m_curr = originalPosition;
                    member = parseFunctionCall();
                } else {
                    type = getCustomType(name);

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

                std::unique_ptr<ast::ASTNode> size;
                if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
                    size = parseWhileStatement();
                else
                    size = parseMathematicalExpression();

                if (!sequence(tkn::Separator::RightBracket)) {
                    error("Expected ']' at end of array declaration, got {}.", getFormattedToken(0));
                    return nullptr;
                }

                member = create<ast::ASTNodeBitfieldArrayVariableDecl>(fieldName, std::move(type), std::move(size));
            } else if (sequence(tkn::Literal::Identifier)) {
                // (parseType) Identifier;
                if (sequence(tkn::Operator::At)) {
                    error("Placement syntax is invalid within bitfields.");
                    return nullptr;
                }

                auto variableName = getValue<Token::Identifier>(-1).get();

                if (sequence(tkn::Operator::Colon))
                    member = create<ast::ASTNodeBitfieldFieldSizedType>(variableName, std::move(type), parseMathematicalExpression());
                else
                    member = parseMemberVariable(std::move(type), false, false, variableName);
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
    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::parseBitfield() {
        const std::string typeName = getValue<Token::Identifier>(-1).get();

        auto typeDecl = addType(typeName, create<ast::ASTNodeBitfield>());
        typeDecl->setTemplateParameters(this->parseTemplateList());
        const auto bitfieldNode = static_cast<ast::ASTNodeBitfield *>(typeDecl->getType().get());

        if (!sequence(tkn::Separator::LeftBrace)) {
            errorDesc("Expected '{{' after bitfield declaration, got {}.", getFormattedToken(0));
            return nullptr;
        }

        while (!sequence(tkn::Separator::RightBrace)) {
            bitfieldNode->addEntry(this->parseBitfieldEntry());

            // Consume superfluous semicolons
            while (sequence(tkn::Separator::Semicolon))
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
        if (sequence(tkn::Operator::At)) {
            placementOffset = parseMathematicalExpression();

            if (sequence(tkn::Keyword::In))
                placementSection = parseMathematicalExpression();
        } else if (sequence(tkn::Keyword::In)) {
            inVariable = true;
        } else if (sequence(tkn::Keyword::Out)) {
            outVariable = true;
        } else if (sequence(tkn::Operator::Assign)) {
            std::vector<std::unique_ptr<ast::ASTNode>> compounds;

            compounds.push_back(create<ast::ASTNodeVariableDecl>(name, type, std::move(placementOffset), nullptr, inVariable, outVariable));
            compounds.push_back(create<ast::ASTNodeLValueAssignment>(name, parseMathematicalExpression()));

            return create<ast::ASTNodeCompoundStatement>(std::move(compounds));
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

        return create<ast::ASTNodeVariableDecl>(name, type, std::move(placementOffset), std::move(placementSection), inVariable, outVariable);
    }

    // (parseType) Identifier[[(parseMathematicalExpression)]] @ Integer
    std::unique_ptr<ast::ASTNode> Parser::parseArrayVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ast::ASTNode> size;

        if (!sequence(tkn::Separator::RightBracket)) {
            if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!sequence(tkn::Separator::RightBracket)) {
                errorDesc("Expected ']' at end of array declaration, got {}.", getFormattedToken(0));
                return nullptr;
            }
        }

        std::unique_ptr<ast::ASTNode> placementOffset, placementSection;
        if (sequence(tkn::Operator::At)) {
            placementOffset = parseMathematicalExpression();

            if (sequence(tkn::Keyword::In))
                placementSection = parseMathematicalExpression();
        }

        return create<ast::ASTNodeArrayVariableDecl>(name, type, std::move(size), std::move(placementOffset), std::move(placementSection));
    }

    // (parseType) *Identifier : (parseType) @ Integer
    std::unique_ptr<ast::ASTNode> Parser::parsePointerVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        auto sizeType = parseType();

        if (!sequence(tkn::Operator::At)) {
            error("Expected '@' after pointer placement, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto placementOffset = parseMathematicalExpression();

        std::unique_ptr<ast::ASTNode> placementSection;
        if (sequence(tkn::Keyword::In))
            placementSection = parseMathematicalExpression();

        return create<ast::ASTNodePointerVariableDecl>(name, type, std::move(sizeType), std::move(placementOffset), std::move(placementSection));
    }

    // (parseType) *Identifier[[(parseMathematicalExpression)]] : (parseType) @ Integer
    std::unique_ptr<ast::ASTNode> Parser::parsePointerArrayVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ast::ASTNode> size;

        if (!sequence(tkn::Separator::RightBracket)) {
            if (sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

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

        if (!sequence(tkn::Operator::At)) {
            error("Expected '@' after pointer placement, got {}.", getFormattedToken(0));
            return nullptr;
        }

        auto placementOffset = parseMathematicalExpression();

        std::unique_ptr<ast::ASTNode> placementSection;
        if (sequence(tkn::Keyword::In))
            placementSection = parseMathematicalExpression();

        return create<ast::ASTNodePointerVariableDecl>(name, createShared<ast::ASTNodeArrayVariableDecl>("", type, std::move(size)), std::move(sizeType), std::move(placementOffset), std::move(placementSection));
    }

    std::vector<std::shared_ptr<ast::ASTNode>> Parser::parseNamespace() {
        std::vector<std::shared_ptr<ast::ASTNode>> statements;

        if (!sequence(tkn::Literal::Identifier)) {
            error("Expected identifier after 'namespace', got {}.", getFormattedToken(0));
            return { };
        }

        this->m_currNamespace.push_back(this->m_currNamespace.back());

        while (true) {
            this->m_currNamespace.back().push_back(getValue<Token::Identifier>(-1).get());

            if (!sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier))
                break;
        }

        if (!sequence(tkn::Separator::LeftBrace)) {
            error("Expected '{{' at beginning of namespace, got {}.", getFormattedToken(0));
            return { };
        }

        while (!sequence(tkn::Separator::RightBrace)) {
            auto newStatements = parseStatements();
            std::ranges::move(newStatements, std::back_inserter(statements));
        }

        this->m_currNamespace.pop_back();

        return statements;
    }

    std::unique_ptr<ast::ASTNode> Parser::parsePlacement() {
        auto type = parseType();

        if (sequence(tkn::Literal::Identifier, tkn::Separator::LeftBracket))
            return parseArrayVariablePlacement(std::move(type));
        if (sequence(tkn::Literal::Identifier))
            return parseVariablePlacement(std::move(type));
        if (sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Operator::Colon))
            return parsePointerVariablePlacement(std::move(type));
        if (sequence(tkn::Operator::Star, tkn::Literal::Identifier, tkn::Separator::LeftBracket))
            return parsePointerArrayVariablePlacement(std::move(type));

        error("Invalid placement syntax.");
        return nullptr;
    }

    /* Program */

    // <(parseUsingDeclaration)|(parseVariablePlacement)|(parseStruct)>
    std::vector<std::shared_ptr<ast::ASTNode>> Parser::parseStatements() {
        std::shared_ptr<ast::ASTNode> statement;
        bool requiresSemicolon = true;

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

        if (statement && sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket))
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

    std::shared_ptr<ast::ASTNodeTypeDecl> Parser::addType(const std::string &name, std::unique_ptr<ast::ASTNode> &&node, std::optional<std::endian> endian) {
        auto typeName = getNamespacePrefixedNames(name).back();

        if (this->m_types.contains(typeName) && this->m_types.at(typeName)->isForwardDeclared()) {
            this->m_types.at(typeName)->setType(std::move(node));

            return this->m_types.at(typeName);
        }

        if (!this->m_types.contains(typeName)) {
            std::shared_ptr typeDecl = createShared<ast::ASTNodeTypeDecl>(typeName, std::move(node), endian);
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

                    error("Unmatched DOCS IGNORE OFF without previous DOCS IGNORE ON");
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
    CompileResult<std::vector<std::shared_ptr<ast::ASTNode>>> Parser::parse(const std::vector<Token> &tokens) {

        this->m_curr = this->m_startToken = this->m_originalPosition = this->m_partOriginalPosition = tokens.begin();

        this->m_types.clear();
        this->m_currTemplateType.clear();
        this->m_matchedOptionals.clear();
        this->m_processedDocComments.clear();

        this->m_currNamespace.clear();
        this->m_currNamespace.emplace_back();

        auto program = parseTillToken(tkn::Separator::EndOfProgram);

        if (this->m_curr == tokens.end()) {
            for (const auto &type : this->m_types)
                type.second->setCompleted();

            return { program, {} };
        }

        errorDesc("Failed to parse entire input.", "Parsing stopped due to an invalid sequence before the entire input could be parsed. This is most likely a bug.");
        return { {}, this->collectErrors() };
    }

    Location Parser::location() {
        return this->m_curr->location;
    }


}
