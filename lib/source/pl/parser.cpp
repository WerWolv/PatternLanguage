#include "pl/parser.hpp"

#include "pl/ast/ast_node_array_variable_decl.hpp"
#include "pl/ast/ast_node_attribute.hpp"
#include "pl/ast/ast_node_bitfield.hpp"
#include "pl/ast/ast_node_bitfield_field.hpp"
#include "pl/ast/ast_node_builtin_type.hpp"
#include "pl/ast/ast_node_cast.hpp"
#include "pl/ast/ast_node_compound_statement.hpp"
#include "pl/ast/ast_node_conditional_statement.hpp"
#include "pl/ast/ast_node_control_flow_statement.hpp"
#include "pl/ast/ast_node_enum.hpp"
#include "pl/ast/ast_node_function_call.hpp"
#include "pl/ast/ast_node_function_definition.hpp"
#include "pl/ast/ast_node_literal.hpp"
#include "pl/ast/ast_node_lvalue_assignment.hpp"
#include "pl/ast/ast_node_mathematical_expression.hpp"
#include "pl/ast/ast_node_multi_variable_decl.hpp"
#include "pl/ast/ast_node_parameter_pack.hpp"
#include "pl/ast/ast_node_pointer_variable_decl.hpp"
#include "pl/ast/ast_node_rvalue.hpp"
#include "pl/ast/ast_node_rvalue_assignment.hpp"
#include "pl/ast/ast_node_scope_resolution.hpp"
#include "pl/ast/ast_node_struct.hpp"
#include "pl/ast/ast_node_ternary_expression.hpp"
#include "pl/ast/ast_node_type_decl.hpp"
#include "pl/ast/ast_node_type_operator.hpp"
#include "pl/ast/ast_node_union.hpp"
#include "pl/ast/ast_node_variable_decl.hpp"
#include "pl/ast/ast_node_while_statement.hpp"

#include <optional>

#define MATCHES(x) (begin() && resetIfFailed(x))

// Definition syntax:
// [A]          : Either A or no token
// [A|B]        : Either A, B or no token
// <A|B>        : Either A or B
// <A...>       : One or more of A
// A B C        : Sequence of tokens A then B then C
// (parseXXXX)  : Parsing handled by other function
namespace pl {

    /* Mathematical expressions */

    // Identifier([(parseMathematicalExpression)|<(parseMathematicalExpression),...>(parseMathematicalExpression)]
    std::unique_ptr<ASTNode> Parser::parseFunctionCall() {
        std::string functionName = parseNamespaceResolution();

        if (!MATCHES(sequence(tkn::Separator::LeftParenthesis)))
            throwParserError("expected '(' after function name");

        std::vector<std::unique_ptr<ASTNode>> params;

        while (!MATCHES(sequence(tkn::Separator::RightParenthesis))) {
            params.push_back(parseMathematicalExpression());

            if (MATCHES(sequence(tkn::Separator::Comma, tkn::Separator::RightParenthesis)))
                throwParserError("unexpected ',' at end of function parameter list", -1);
            else if (MATCHES(sequence(tkn::Separator::RightParenthesis)))
                break;
            else if (!MATCHES(sequence(tkn::Separator::Comma)))
                throwParserError("missing ',' between parameters", -1);
        }

        return create(new ASTNodeFunctionCall(functionName, std::move(params)));
    }

    std::unique_ptr<ASTNode> Parser::parseStringLiteral() {
        return create(new ASTNodeLiteral(getValue<Token::Literal>(-1)));
    }

    std::string Parser::parseNamespaceResolution() {
        std::string name;

        while (true) {
            name += getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier()))) {
                name += "::";
                continue;
            } else
                break;
        }

        return name;
    }

    std::unique_ptr<ASTNode> Parser::parseScopeResolution() {
        std::string typeName;

        while (true) {
            typeName += getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier()))) {
                if (peek(tkn::Operator::ScopeResolution, 0) && peek(tkn::Literal::Identifier(), 1)) {
                    typeName += "::";
                    continue;
                } else {
                    if (!this->m_types.contains(typeName))
                        throwParserError(fmt::format("cannot access scope of invalid type '{}'", typeName), -1);

                    return create(new ASTNodeScopeResolution(this->m_types[typeName]->clone(), getValue<Token::Identifier>(-1).get()));
                }
            } else
                break;
        }

        throwParserError("failed to parse scope resolution. Expected 'TypeName::Identifier'");
    }

    std::unique_ptr<ASTNode> Parser::parseRValue() {
        ASTNodeRValue::Path path;
        return this->parseRValue(path);
    }

    // <Identifier[.]...>
    std::unique_ptr<ASTNode> Parser::parseRValue(ASTNodeRValue::Path &path) {
        if (peek(tkn::Literal::Identifier(), -1))
            path.push_back(getValue<Token::Identifier>(-1).get());
        else if (peek(tkn::Keyword::Parent, -1))
            path.emplace_back("parent");
        else if (peek(tkn::Keyword::This, -1))
            path.emplace_back("this");

        if (MATCHES(sequence(tkn::Separator::LeftBracket))) {
            path.push_back(parseMathematicalExpression());
            if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                throwParserError("expected closing ']' at end of array indexing");
        }

        if (MATCHES(sequence(tkn::Separator::Dot))) {
            if (MATCHES(oneOf(tkn::Literal::Identifier(), tkn::Keyword::Parent)))
                return this->parseRValue(path);
            else
                throwParserError("expected member name or 'parent' keyword", -1);
        } else
            return create(new ASTNodeRValue(std::move(path)));
    }

    // <Integer|((parseMathematicalExpression))>
    std::unique_ptr<ASTNode> Parser::parseFactor() {
        if (MATCHES(sequence(tkn::Literal::Numeric())))
            return create(new ASTNodeLiteral(getValue<Token::Literal>(-1)));
        else if (peek(tkn::Operator::Plus) || peek(tkn::Operator::Minus) || peek(tkn::Operator::BitNot) || peek(tkn::Operator::BoolNot))
            return this->parseMathematicalExpression();
        else if (MATCHES(sequence(tkn::Separator::LeftParenthesis))) {
            auto node = this->parseMathematicalExpression();
            if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
                throwParserError("expected closing parenthesis");

            return node;
        } else if (MATCHES(sequence(tkn::Literal::Identifier()))) {
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
        } else if (MATCHES(oneOf(tkn::Keyword::Parent, tkn::Keyword::This))) {
            return this->parseRValue();
        } else if (MATCHES(sequence(tkn::Operator::Dollar))) {
            return create(new ASTNodeRValue(pl::moveToVector<ASTNodeRValue::PathSegment>("$")));
        } else if (MATCHES(oneOf(tkn::Operator::AddressOf, tkn::Operator::SizeOf) && sequence(tkn::Separator::LeftParenthesis))) {
            auto op = getValue<Token::Operator>(-2);

            std::unique_ptr<ASTNode> result;

            if (MATCHES(oneOf(tkn::Literal::Identifier(), tkn::Keyword::Parent, tkn::Keyword::This))) {
                result = create(new ASTNodeTypeOperator(op, this->parseRValue()));
            } else if (MATCHES(sequence(tkn::ValueType::Any))) {
                auto type = getValue<Token::ValueType>(-1);

                result = create(new ASTNodeLiteral(u128(Token::getTypeSize(type))));
            } else if (MATCHES(sequence(tkn::Operator::Dollar))) {
                result = create(new ASTNodeTypeOperator(op));
            } else {
                throwParserError("expected rvalue identifier or built-in type");
            }

            if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
                throwParserError("expected closing parenthesis");

            return result;
        } else
            throwParserError("expected value or parenthesis");
    }

    std::unique_ptr<ASTNode> Parser::parseCastExpression() {
        if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any)) {
            auto type        = parseType(true);
            auto builtinType = dynamic_cast<ASTNodeBuiltinType *>(type->getType().get());

            if (builtinType == nullptr)
                throwParserError("invalid type used for pointer size", -1);

            if (!peek(tkn::Separator::LeftParenthesis))
                throwParserError("expected '(' before cast expression", -1);

            auto node = parseFactor();

            return create(new ASTNodeCast(std::move(node), std::move(type)));
        } else return parseFactor();
    }

    // <+|-|!|~> (parseFactor)
    std::unique_ptr<ASTNode> Parser::parseUnaryExpression() {
        if (MATCHES(oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::BoolNot, tkn::Operator::BitNot))) {
            auto op = getValue<Token::Operator>(-1);

            return create(new ASTNodeMathematicalExpression(create(new ASTNodeLiteral(0)), this->parseCastExpression(), op));
        } else if (MATCHES(sequence(tkn::Literal::String()))) {
            return this->parseStringLiteral();
        }

        return this->parseCastExpression();
    }

    // (parseUnaryExpression) <*|/|%> (parseUnaryExpression)
    std::unique_ptr<ASTNode> Parser::parseMultiplicativeExpression() {
        auto node = this->parseUnaryExpression();

        while (MATCHES(oneOf(tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create(new ASTNodeMathematicalExpression(std::move(node), this->parseUnaryExpression(), op));
        }

        return node;
    }

    // (parseMultiplicativeExpression) <+|-> (parseMultiplicativeExpression)
    std::unique_ptr<ASTNode> Parser::parseAdditiveExpression() {
        auto node = this->parseMultiplicativeExpression();

        while (MATCHES(variant(tkn::Operator::Plus, tkn::Operator::Minus))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create(new ASTNodeMathematicalExpression(std::move(node), this->parseMultiplicativeExpression(), op));
        }

        return node;
    }

    // (parseAdditiveExpression) < >>|<< > (parseAdditiveExpression)
    std::unique_ptr<ASTNode> Parser::parseShiftExpression() {
        auto node = this->parseAdditiveExpression();

        while (MATCHES(variant(tkn::Operator::LeftShift, tkn::Operator::RightShift))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create(new ASTNodeMathematicalExpression(std::move(node), this->parseAdditiveExpression(), op));
        }

        return node;
    }

    // (parseShiftExpression) & (parseShiftExpression)
    std::unique_ptr<ASTNode> Parser::parseBinaryAndExpression() {
        auto node = this->parseShiftExpression();

        while (MATCHES(sequence(tkn::Operator::BitAnd))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseShiftExpression(), Token::Operator::BitAnd));
        }

        return node;
    }

    // (parseBinaryAndExpression) ^ (parseBinaryAndExpression)
    std::unique_ptr<ASTNode> Parser::parseBinaryXorExpression() {
        auto node = this->parseBinaryAndExpression();

        while (MATCHES(sequence(tkn::Operator::BitXor))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseBinaryAndExpression(), Token::Operator::BitXor));
        }

        return node;
    }

    // (parseBinaryXorExpression) | (parseBinaryXorExpression)
    std::unique_ptr<ASTNode> Parser::parseBinaryOrExpression() {
        auto node = this->parseBinaryXorExpression();

        while (MATCHES(sequence(tkn::Operator::BitOr))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseBinaryXorExpression(), Token::Operator::BitOr));
        }

        return node;
    }

    // (parseBinaryOrExpression) < >=|<=|>|< > (parseBinaryOrExpression)
    std::unique_ptr<ASTNode> Parser::parseRelationExpression() {
        auto node = this->parseBinaryOrExpression();

        while (MATCHES(sequence(tkn::Operator::BoolGreaterThan) || sequence(tkn::Operator::BoolLessThan) || sequence(tkn::Operator::BoolGreaterThanOrEqual) || sequence(tkn::Operator::BoolLessThanOrEqual))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create(new ASTNodeMathematicalExpression(std::move(node), this->parseBinaryOrExpression(), op));
        }

        return node;
    }

    // (parseRelationExpression) <==|!=> (parseRelationExpression)
    std::unique_ptr<ASTNode> Parser::parseEqualityExpression() {
        auto node = this->parseRelationExpression();

        while (MATCHES(sequence(tkn::Operator::BoolEqual) || sequence(tkn::Operator::BoolNotEqual))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create(new ASTNodeMathematicalExpression(std::move(node), this->parseRelationExpression(), op));
        }

        return node;
    }

    // (parseEqualityExpression) && (parseEqualityExpression)
    std::unique_ptr<ASTNode> Parser::parseBooleanAnd() {
        auto node = this->parseEqualityExpression();

        while (MATCHES(sequence(tkn::Operator::BoolAnd))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseEqualityExpression(), Token::Operator::BoolAnd));
        }

        return node;
    }

    // (parseBooleanAnd) ^^ (parseBooleanAnd)
    std::unique_ptr<ASTNode> Parser::parseBooleanXor() {
        auto node = this->parseBooleanAnd();

        while (MATCHES(sequence(tkn::Operator::BoolXor))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseBooleanAnd(), Token::Operator::BoolXor));
        }

        return node;
    }

    // (parseBooleanXor) || (parseBooleanXor)
    std::unique_ptr<ASTNode> Parser::parseBooleanOr() {
        auto node = this->parseBooleanXor();

        while (MATCHES(sequence(tkn::Operator::BoolOr))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseBooleanXor(), Token::Operator::BoolOr));
        }

        return node;
    }

    // (parseBooleanOr) ? (parseBooleanOr) : (parseBooleanOr)
    std::unique_ptr<ASTNode> Parser::parseTernaryConditional() {
        auto node = this->parseBooleanOr();

        while (MATCHES(sequence(tkn::Operator::TernaryConditional))) {
            auto second = this->parseBooleanOr();

            if (!MATCHES(sequence(tkn::Operator::Colon)))
                throwParserError("expected ':' in ternary expression");

            auto third = this->parseBooleanOr();
            node       = create(new ASTNodeTernaryExpression(std::move(node), std::move(second), std::move(third), Token::Operator::TernaryConditional));
        }

        return node;
    }

    // (parseTernaryConditional)
    std::unique_ptr<ASTNode> Parser::parseMathematicalExpression() {
        return this->parseTernaryConditional();
    }

    // [[ <Identifier[( (parseStringLiteral) )], ...> ]]
    void Parser::parseAttribute(Attributable *currNode) {
        if (currNode == nullptr)
            throwParserError("tried to apply attribute to invalid statement");

        do {
            if (!MATCHES(sequence(tkn::Literal::Identifier())))
                throwParserError("expected attribute expression");

            auto attribute = getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(tkn::Separator::LeftParenthesis, tkn::Literal::String(), tkn::Separator::RightParenthesis))) {
                auto value  = getValue<Token::Literal>(-2);
                auto string = std::get_if<std::string>(&value);

                if (string == nullptr)
                    throwParserError("expected string attribute argument");

                currNode->addAttribute(create(new ASTNodeAttribute(attribute, *string)));
            } else
                currNode->addAttribute(create(new ASTNodeAttribute(attribute)));

        } while (MATCHES(sequence(tkn::Separator::Comma)));

        if (!MATCHES(sequence(tkn::Separator::RightBracket, tkn::Separator::RightBracket)))
            throwParserError("unfinished attribute. Expected ']]'");
    }

    /* Functions */

    std::unique_ptr<ASTNode> Parser::parseFunctionDefinition() {
        const auto &functionName = getValue<Token::Identifier>(-2).get();
        std::vector<std::pair<std::string, std::unique_ptr<ASTNode>>> params;
        std::optional<std::string> parameterPack;

        // Parse parameter list
        bool hasParams        = !peek(tkn::Separator::RightParenthesis);
        u32 unnamedParamCount = 0;
        std::vector<std::unique_ptr<ASTNode>> defaultParameters;

        while (hasParams) {
            if (MATCHES(sequence(tkn::ValueType::Auto, tkn::Separator::Dot, tkn::Separator::Dot, tkn::Separator::Dot, tkn::Literal::Identifier()))) {
                parameterPack = getValue<Token::Identifier>(-1).get();

                if (MATCHES(sequence(tkn::Separator::Comma)))
                    throwParserError("parameter pack can only appear at end of parameter list");

                break;
            } else {
                auto type = parseType(true);

                if (MATCHES(sequence(tkn::Literal::Identifier())))
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
                        throwParserError(fmt::format("default argument missing for parameter {}", params.size()));
                }

                if (!MATCHES(sequence(tkn::Separator::Comma))) {
                    break;
                }
            }
        }

        if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
            throwParserError("expected closing ')' after parameter list");

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            throwParserError("expected opening '{' after function definition");


        // Parse function body
        std::vector<std::unique_ptr<ASTNode>> body;

        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            body.push_back(this->parseFunctionStatement());
        }

        return create(new ASTNodeFunctionDefinition(getNamespacePrefixedNames(functionName).back(), std::move(params), std::move(body), parameterPack, std::move(defaultParameters)));
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionVariableDecl() {
        std::unique_ptr<ASTNode> statement;
        auto type = parseType(true);

        if (MATCHES(sequence(tkn::Literal::Identifier()))) {
            auto identifier = getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(tkn::Separator::LeftBracket) && !peek(tkn::Separator::LeftBracket))) {
                statement = parseMemberArrayVariable(std::move(type));
            } else {
                statement = parseMemberVariable(std::move(type));

                if (MATCHES(sequence(tkn::Operator::Assign))) {
                    auto expression = parseMathematicalExpression();

                    std::vector<std::unique_ptr<ASTNode>> compoundStatement;
                    {
                        compoundStatement.push_back(std::move(statement));
                        compoundStatement.push_back(create(new ASTNodeLValueAssignment(identifier, std::move(expression))));
                    }

                    statement = create(new ASTNodeCompoundStatement(std::move(compoundStatement)));
                }
            }
        } else
            throwParserError("invalid variable declaration");

        return statement;
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionStatement() {
        bool needsSemicolon = true;
        std::unique_ptr<ASTNode> statement;

        if (MATCHES(sequence(tkn::Literal::Identifier(), tkn::Operator::Assign)))
            statement = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (MATCHES(sequence(tkn::Operator::Dollar, tkn::Operator::Assign)))
            statement = parseFunctionVariableAssignment("$");
        else if (MATCHES(oneOf(tkn::Literal::Identifier()) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            statement = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(-3).get());
        else if (MATCHES(oneOf(tkn::Operator::Dollar) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            statement = parseFunctionVariableCompoundAssignment("$");
        else if (MATCHES(oneOf(tkn::Keyword::Return, tkn::Keyword::Break, tkn::Keyword::Continue)))
            statement = parseFunctionControlFlowStatement();
        else if (MATCHES(sequence(tkn::Keyword::If, tkn::Separator::LeftParenthesis))) {
            statement      = parseFunctionConditional();
            needsSemicolon = false;
        } else if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis))) {
            statement      = parseFunctionWhileLoop();
            needsSemicolon = false;
        } else if (MATCHES(sequence(tkn::Keyword::For, tkn::Separator::LeftParenthesis))) {
            statement      = parseFunctionForLoop();
            needsSemicolon = false;
        } else if (MATCHES(sequence(tkn::Literal::Identifier()) && (peek(tkn::Separator::Dot) || peek(tkn::Separator::LeftBracket)))) {
            auto lhs = parseRValue();

            if (!MATCHES(sequence(tkn::Operator::Assign)))
                throwParserError("failed to parse rvalue assignment. Expected '='", 0);

            auto rhs = parseMathematicalExpression();

            statement = create(new ASTNodeRValueAssignment(std::move(lhs), std::move(rhs)));
        } else if (MATCHES(sequence(tkn::Literal::Identifier()))) {
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
        } else
            throwParserError("invalid sequence", 0);

        if (needsSemicolon && !MATCHES(sequence(tkn::Separator::Semicolon)))
            throwParserError("missing ';' at end of expression", -1);

        // Consume superfluous semicolons
        while (needsSemicolon && MATCHES(sequence(tkn::Separator::Semicolon)))
            ;

        return statement;
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionVariableAssignment(const std::string &lvalue) {
        auto rvalue = this->parseMathematicalExpression();

        return create(new ASTNodeLValueAssignment(lvalue, std::move(rvalue)));
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionVariableCompoundAssignment(const std::string &lvalue) {
        const auto &op = getValue<Token::Operator>(-2);

        auto rvalue = this->parseMathematicalExpression();

        return create(new ASTNodeLValueAssignment(lvalue, create(new ASTNodeMathematicalExpression(create(new ASTNodeRValue(pl::moveToVector<ASTNodeRValue::PathSegment>(lvalue))), std::move(rvalue), op))));
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionControlFlowStatement() {
        ControlFlowStatement type;
        if (peek(tkn::Keyword::Return, -1))
            type = ControlFlowStatement::Return;
        else if (peek(tkn::Keyword::Break, -1))
            type = ControlFlowStatement::Break;
        else if (peek(tkn::Keyword::Continue, -1))
            type = ControlFlowStatement::Continue;
        else
            throwParserError("invalid control flow statement. Expected 'return', 'break' or 'continue'");

        if (peek(tkn::Separator::Semicolon))
            return create(new ASTNodeControlFlowStatement(type, nullptr));
        else
            return create(new ASTNodeControlFlowStatement(type, this->parseMathematicalExpression()));
    }

    std::vector<std::unique_ptr<ASTNode>> Parser::parseStatementBody() {
        std::vector<std::unique_ptr<ASTNode>> body;

        if (MATCHES(sequence(tkn::Separator::LeftBrace))) {
            while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
                body.push_back(parseFunctionStatement());
            }
        } else {
            body.push_back(parseFunctionStatement());
        }

        return body;
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionConditional() {
        auto condition = parseMathematicalExpression();
        std::vector<std::unique_ptr<ASTNode>> trueBody, falseBody;

        if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
            throwParserError("expected closing ')' after statement head");

        trueBody = parseStatementBody();

        if (MATCHES(sequence(tkn::Keyword::Else)))
            falseBody = parseStatementBody();

        return create(new ASTNodeConditionalStatement(std::move(condition), std::move(trueBody), std::move(falseBody)));
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionWhileLoop() {
        auto condition = parseMathematicalExpression();
        std::vector<std::unique_ptr<ASTNode>> body;

        if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
            throwParserError("expected closing ')' after statement head");

        body = parseStatementBody();

        return create(new ASTNodeWhileStatement(std::move(condition), std::move(body)));
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionForLoop() {
        auto variable = parseFunctionVariableDecl();

        if (!MATCHES(sequence(tkn::Separator::Comma)))
            throwParserError("expected ',' after for loop variable declaration");

        auto condition = parseMathematicalExpression();

        if (!MATCHES(sequence(tkn::Separator::Comma)))
            throwParserError("expected ',' after for loop condition");

        std::unique_ptr<ASTNode> postExpression = nullptr;
        if (MATCHES(sequence(tkn::Literal::Identifier(), tkn::Operator::Assign)))
            postExpression = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (MATCHES(sequence(tkn::Operator::Dollar, tkn::Operator::Assign)))
            postExpression = parseFunctionVariableAssignment("$");
        else if (MATCHES(oneOf(tkn::Literal::Identifier()) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            postExpression = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(-3).get());
        else if (MATCHES(oneOf(tkn::Operator::Dollar) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            postExpression = parseFunctionVariableCompoundAssignment("$");
        else
            throwParserError("expected variable assignment in for loop post expression");

        std::vector<std::unique_ptr<ASTNode>> body;


        if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
            throwParserError("expected closing ')' after statement head");

        body = parseStatementBody();

        std::vector<std::unique_ptr<ASTNode>> compoundStatement;
        {
            compoundStatement.push_back(std::move(variable));
            compoundStatement.push_back(create(new ASTNodeWhileStatement(std::move(condition), std::move(body), std::move(postExpression))));
        }

        return create(new ASTNodeCompoundStatement(std::move(compoundStatement), true));
    }

    /* Control flow */

    // if ((parseMathematicalExpression)) { (parseMember) }
    std::unique_ptr<ASTNode> Parser::parseConditional() {
        auto condition = parseMathematicalExpression();
        std::vector<std::unique_ptr<ASTNode>> trueBody, falseBody;

        if (MATCHES(sequence(tkn::Separator::RightParenthesis, tkn::Separator::LeftBrace))) {
            while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
                trueBody.push_back(parseMember());
            }
        } else if (MATCHES(sequence(tkn::Separator::RightParenthesis))) {
            trueBody.push_back(parseMember());
        } else
            throwParserError("expected body of conditional statement");

        if (MATCHES(sequence(tkn::Keyword::Else, tkn::Separator::LeftBrace))) {
            while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
                falseBody.push_back(parseMember());
            }
        } else if (MATCHES(sequence(tkn::Keyword::Else))) {
            falseBody.push_back(parseMember());
        }

        return create(new ASTNodeConditionalStatement(std::move(condition), std::move(trueBody), std::move(falseBody)));
    }

    // while ((parseMathematicalExpression))
    std::unique_ptr<ASTNode> Parser::parseWhileStatement() {
        auto condition = parseMathematicalExpression();

        if (!MATCHES(sequence(tkn::Separator::RightParenthesis)))
            throwParserError("expected closing ')' after while head");

        return create(new ASTNodeWhileStatement(std::move(condition), {}));
    }

    /* Type declarations */

    // [be|le] <Identifier|u8|u16|u24|u32|u48|u64|u96|u128|s8|s16|s24|s32|s48|s64|s96|s128|float|double|str>
    std::unique_ptr<ASTNodeTypeDecl> Parser::parseType(bool allowFunctionTypes) {
        std::optional<std::endian> endian;

        if (MATCHES(sequence(tkn::Keyword::LittleEndian)))
            endian = std::endian::little;
        else if (MATCHES(sequence(tkn::Keyword::BigEndian)))
            endian = std::endian::big;

        if (MATCHES(sequence(tkn::Literal::Identifier()))) {    // Custom type
            auto baseTypeName = parseNamespaceResolution();

            for (const auto &typeName : getNamespacePrefixedNames(baseTypeName)) {
                if (this->m_types.contains(typeName))
                    return create(new ASTNodeTypeDecl({}, this->m_types[typeName], endian));
            }

            throwParserError(fmt::format("unknown type '{}'", baseTypeName));
        } else if (MATCHES(sequence(tkn::ValueType::Any))) {    // Builtin type
            auto type = getValue<Token::ValueType>(-1);
            if (!allowFunctionTypes) {
                if (type == Token::ValueType::String)
                    throwParserError("cannot use 'str' in this context. Use a character array instead");
                else if (type == Token::ValueType::Auto)
                    throwParserError("cannot use 'auto' in this context");
            }

            return create(new ASTNodeTypeDecl({}, create(new ASTNodeBuiltinType(type)), endian));
        } else throwParserError("failed to parse type. Expected identifier or builtin type");
    }

    // using Identifier = (parseType)
    std::shared_ptr<ASTNodeTypeDecl> Parser::parseUsingDeclaration() {
        auto name = getValue<Token::Identifier>(-2).get();

        auto type = parseType();

        auto endian = type->getEndian();
        return addType(name, std::move(type), endian);
    }

    // padding[(parseMathematicalExpression)]
    std::unique_ptr<ASTNode> Parser::parsePadding() {
        std::unique_ptr<ASTNode> size;
        if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
            size = parseWhileStatement();
        else
            size = parseMathematicalExpression();

        if (!MATCHES(sequence(tkn::Separator::RightBracket)))
            throwParserError("expected closing ']' at end of array declaration", -1);

        return create(new ASTNodeArrayVariableDecl({}, create(new ASTNodeTypeDecl({}, create(new ASTNodeBuiltinType(Token::ValueType::Padding)))), std::move(size)));
    }

    // (parseType) Identifier
    std::unique_ptr<ASTNode> Parser::parseMemberVariable(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        if (peek(tkn::Separator::Comma)) {

            std::vector<std::unique_ptr<ASTNode>> variables;

            do {
                variables.push_back(create(new ASTNodeVariableDecl(getValue<Token::Identifier>(-1).get(), type)));
            } while (MATCHES(sequence(tkn::Separator::Comma, tkn::Literal::Identifier())));

            return create(new ASTNodeMultiVariableDecl(std::move(variables)));
        } else if (MATCHES(sequence(tkn::Operator::At)))
            return create(new ASTNodeVariableDecl(getValue<Token::Identifier>(-2).get(), type, parseMathematicalExpression()));
        else
            return create(new ASTNodeVariableDecl(getValue<Token::Identifier>(-1).get(), type));
    }

    // (parseType) Identifier[(parseMathematicalExpression)]
    std::unique_ptr<ASTNode> Parser::parseMemberArrayVariable(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ASTNode> size;

        if (!MATCHES(sequence(tkn::Separator::RightBracket))) {
            if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                throwParserError("expected closing ']' at end of array declaration", -1);
        }

        if (MATCHES(sequence(tkn::Operator::At)))
            return create(new ASTNodeArrayVariableDecl(name, type, std::move(size), parseMathematicalExpression()));
        else
            return create(new ASTNodeArrayVariableDecl(name, type, std::move(size)));
    }

    std::unique_ptr<ASTNodeTypeDecl> Parser::parsePointerSizeType() {
        auto sizeType = parseType();

        auto builtinType = dynamic_cast<ASTNodeBuiltinType *>(sizeType->getType().get());

        if (builtinType == nullptr || !Token::isInteger(builtinType->getType()))
            throwParserError("invalid type used for pointer size", -1);

        if (Token::getTypeSize(builtinType->getType()) > 8) {
            throwParserError("pointer size cannot be larger than 64 bits", -1);
        }

        return sizeType;
    }

    // (parseType) *Identifier : (parseType)
    std::unique_ptr<ASTNode> Parser::parseMemberPointerVariable(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();
        auto sizeType = parsePointerSizeType();

        if (MATCHES(sequence(tkn::Operator::At)))
            return create(new ASTNodePointerVariableDecl(name, type, std::move(sizeType), parseMathematicalExpression()));
        else
            return create(new ASTNodePointerVariableDecl(name, type, std::move(sizeType)));
    }

    // (parseType) *Identifier[[(parseMathematicalExpression)]]  : (parseType)
    std::unique_ptr<ASTNode> Parser::parseMemberPointerArrayVariable(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ASTNode> size;

        if (!MATCHES(sequence(tkn::Separator::RightBracket))) {
            if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                throwParserError("expected closing ']' at end of array declaration", -1);
        }

        if (!MATCHES(sequence(tkn::Operator::Colon))) {
            throwParserError("expected type used for pointer size", -1);
        }

        auto sizeType = parsePointerSizeType();
        auto arrayType = create(new ASTNodeArrayVariableDecl("", std::move(type), std::move(size)));

        if (MATCHES(sequence(tkn::Operator::At)))
            return create(new ASTNodePointerVariableDecl(name, std::move(arrayType), std::move(sizeType), parseMathematicalExpression()));
        else
            return create(new ASTNodePointerVariableDecl(name, std::move(arrayType), std::move(sizeType)));
    }

    // [(parsePadding)|(parseMemberVariable)|(parseMemberArrayVariable)|(parseMemberPointerVariable)|(parseMemberArrayPointerVariable)]
    std::unique_ptr<ASTNode> Parser::parseMember() {
        std::unique_ptr<ASTNode> member;

        if (MATCHES(sequence(tkn::Operator::Dollar, tkn::Operator::Assign)))
            member = parseFunctionVariableAssignment("$");
        else if (MATCHES(sequence(tkn::Operator::Dollar) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            member = parseFunctionVariableCompoundAssignment("$");
        else if (MATCHES(sequence(tkn::Literal::Identifier(), tkn::Operator::Assign)))
            member = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (MATCHES(sequence(tkn::Literal::Identifier()) && oneOf(tkn::Operator::Plus, tkn::Operator::Minus, tkn::Operator::Star, tkn::Operator::Slash, tkn::Operator::Percent, tkn::Operator::LeftShift, tkn::Operator::RightShift, tkn::Operator::BitOr, tkn::Operator::BitAnd, tkn::Operator::BitXor) && sequence(tkn::Operator::Assign)))
            member = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(-3).get());
        else if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any) || peek(tkn::Literal::Identifier())) {
            // Some kind of variable definition

            bool isFunction = false;

            if (peek(tkn::Literal::Identifier())) {
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

                if (MATCHES(sequence(tkn::Literal::Identifier(), tkn::Separator::LeftBracket) && sequence<Not>(tkn::Separator::LeftBracket)))
                    member = parseMemberArrayVariable(std::move(type));
                else if (MATCHES(sequence(tkn::Literal::Identifier())))
                    member = parseMemberVariable(std::move(type));
                else if (MATCHES(sequence(tkn::Operator::Star, tkn::Literal::Identifier(), tkn::Operator::Colon)))
                    member = parseMemberPointerVariable(std::move(type));
                else if (MATCHES(sequence(tkn::Operator::Star, tkn::Literal::Identifier(), tkn::Separator::LeftBracket)))
                    member = parseMemberPointerArrayVariable(std::move(type));
                else
                    throwParserError("invalid variable declaration");
            }
        } else if (MATCHES(sequence(tkn::ValueType::Padding, tkn::Separator::LeftBracket)))
            member = parsePadding();
        else if (MATCHES(sequence(tkn::Keyword::If, tkn::Separator::LeftParenthesis)))
            return parseConditional();
        else if (MATCHES(sequence(tkn::Separator::EndOfProgram)))
            throwParserError("unexpected end of program", -2);
        else if (MATCHES(sequence(tkn::Keyword::Break)))
            member = create(new ASTNodeControlFlowStatement(ControlFlowStatement::Break, nullptr));
        else if (MATCHES(sequence(tkn::Keyword::Continue)))
            member = create(new ASTNodeControlFlowStatement(ControlFlowStatement::Continue, nullptr));
        else
            throwParserError("invalid struct member", 0);

        if (MATCHES(sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket)))
            parseAttribute(dynamic_cast<Attributable *>(member.get()));

        if (!MATCHES(sequence(tkn::Separator::Semicolon)))
            throwParserError("missing ';' at end of expression", -1);

        // Consume superfluous semicolons
        while (MATCHES(sequence(tkn::Separator::Semicolon)))
            ;

        return member;
    }

    // struct Identifier { <(parseMember)...> }
    std::shared_ptr<ASTNodeTypeDecl> Parser::parseStruct() {
        const auto &typeName = getValue<Token::Identifier>(-1).get();

        auto typeDecl   = addType(typeName, create(new ASTNodeStruct()));
        auto structNode = static_cast<ASTNodeStruct *>(typeDecl->getType().get());

        if (MATCHES(sequence(tkn::Operator::Colon, tkn::Literal::Identifier()))) {
            // Inheritance

            do {
                auto inheritedTypeName = getValue<Token::Identifier>(-1).get();
                if (!this->m_types.contains(inheritedTypeName))
                    throwParserError(fmt::format("cannot inherit from unknown type '{}'", inheritedTypeName), -1);

                structNode->addInheritance(this->m_types[inheritedTypeName]->clone());
            } while (MATCHES(sequence(tkn::Separator::Comma, tkn::Literal::Identifier())));

        } else if (MATCHES(sequence(tkn::Operator::Colon, tkn::ValueType::Any))) {
            throwParserError("cannot inherit from builtin type");
        }

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            throwParserError("expected '{' after struct definition", -1);

        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            structNode->addMember(parseMember());
        }

        return typeDecl;
    }

    // union Identifier { <(parseMember)...> }
    std::shared_ptr<ASTNodeTypeDecl> Parser::parseUnion() {
        const auto &typeName = getValue<Token::Identifier>(-2).get();

        auto typeDecl  = addType(typeName, create(new ASTNodeUnion()));
        auto unionNode = static_cast<ASTNodeStruct *>(typeDecl->getType().get());

        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            unionNode->addMember(parseMember());
        }

        return typeDecl;
    }

    // enum Identifier : (parseType) { <<Identifier|Identifier = (parseMathematicalExpression)[,]>...> }
    std::shared_ptr<ASTNodeTypeDecl> Parser::parseEnum() {
        auto typeName = getValue<Token::Identifier>(-2).get();

        auto underlyingType = parseType();
        if (underlyingType->getEndian().has_value()) throwParserError("underlying type may not have an endian specification", -2);

        auto typeDecl = addType(typeName, create(new ASTNodeEnum(std::move(underlyingType))));
        auto enumNode = static_cast<ASTNodeEnum *>(typeDecl->getType().get());

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            throwParserError("expected '{' after enum definition", -1);

        std::unique_ptr<ASTNode> lastEntry;
        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            if (MATCHES(sequence(tkn::Literal::Identifier(), tkn::Operator::Assign))) {
                auto name  = getValue<Token::Identifier>(-2).get();
                auto value = parseMathematicalExpression();

                lastEntry = value->clone();
                enumNode->addEntry(name, std::move(value));
            } else if (MATCHES(sequence(tkn::Literal::Identifier()))) {
                std::unique_ptr<ASTNode> valueExpr;
                auto name = getValue<Token::Identifier>(-1).get();
                if (enumNode->getEntries().empty())
                    valueExpr = create(new ASTNodeLiteral(u128(0)));
                else
                    valueExpr = create(new ASTNodeMathematicalExpression(lastEntry->clone(), create(new ASTNodeLiteral(u128(1))), Token::Operator::Plus));

                lastEntry = valueExpr->clone();
                enumNode->addEntry(name, std::move(valueExpr));
            } else if (MATCHES(sequence(tkn::Separator::EndOfProgram)))
                throwParserError("unexpected end of program", -2);
            else
                throwParserError("invalid enum entry", -1);

            if (!MATCHES(sequence(tkn::Separator::Comma))) {
                if (MATCHES(sequence(tkn::Separator::RightBrace)))
                    break;
                else
                    throwParserError("missing ',' between enum entries", -1);
            }
        }

        return typeDecl;
    }


    std::unique_ptr<ASTNode> Parser::parseBitfieldEntry() {
        std::unique_ptr<ASTNode> result;

        if (MATCHES(sequence(tkn::Literal::Identifier(), tkn::Operator::Colon))) {
            auto name = getValue<Token::Identifier>(-2).get();
            result = create(new ASTNodeBitfieldField(name, parseMathematicalExpression()));

            if (MATCHES(sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket)))
                parseAttribute(dynamic_cast<Attributable *>(result.get()));
        } else if (MATCHES(sequence(tkn::ValueType::Padding, tkn::Operator::Colon))) {
            result = create(new ASTNodeBitfieldField("padding", parseMathematicalExpression()));
        } else if (MATCHES(sequence(tkn::Keyword::If, tkn::Separator::LeftParenthesis))) {
            auto condition = parseMathematicalExpression();
            std::vector<std::unique_ptr<ASTNode>> trueBody, falseBody;

            if (MATCHES(sequence(tkn::Separator::RightParenthesis, tkn::Separator::LeftBrace))) {
                while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
                    trueBody.push_back(parseBitfieldEntry());
                }
            } else if (MATCHES(sequence(tkn::Separator::RightParenthesis))) {
                trueBody.push_back(parseBitfieldEntry());
            } else
                throwParserError("expected body of conditional statement");

            if (MATCHES(sequence(tkn::Keyword::Else, tkn::Separator::LeftBrace))) {
                while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
                    falseBody.push_back(parseBitfieldEntry());
                }
            } else if (MATCHES(sequence(tkn::Keyword::Else))) {
                falseBody.push_back(parseBitfieldEntry());
            }

            return create(new ASTNodeConditionalStatement(std::move(condition), std::move(trueBody), std::move(falseBody)));
        } else if (MATCHES(sequence(tkn::Separator::EndOfProgram)))
            throwParserError("unexpected end of program", -2);
        else
            throwParserError("invalid bitfield member", 0);

        if (!MATCHES(sequence(tkn::Separator::Semicolon)))
            throwParserError("missing ';' at end of expression", -1);

        return result;
    }

    // bitfield Identifier { <Identifier : (parseMathematicalExpression)[;]...> }
    std::shared_ptr<ASTNodeTypeDecl> Parser::parseBitfield() {
        std::string typeName = getValue<Token::Identifier>(-2).get();

        auto typeDecl     = addType(typeName, create(new ASTNodeBitfield()));
        auto bitfieldNode = static_cast<ASTNodeBitfield *>(typeDecl->getType().get());

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

        this->m_types.insert({ typeName, create(new ASTNodeTypeDecl(typeName) )});
    }

    // (parseType) Identifier @ Integer
    std::unique_ptr<ASTNode> Parser::parseVariablePlacement(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        bool inVariable  = false;
        bool outVariable = false;

        auto name = getValue<Token::Identifier>(-1).get();

        std::unique_ptr<ASTNode> placementOffset;
        if (MATCHES(sequence(tkn::Operator::At))) {
            placementOffset = parseMathematicalExpression();
        } else if (MATCHES(sequence(tkn::Keyword::In))) {
            inVariable = true;
        } else if (MATCHES(sequence(tkn::Keyword::Out))) {
            outVariable = true;
        }

        if (inVariable || outVariable) {
            bool invalidType = false;
            if (auto builtinType = dynamic_cast<ASTNodeBuiltinType*>(type->getType().get()); builtinType != nullptr) {
                auto valueType = builtinType->getType();
                if (!Token::isInteger(valueType) && !Token::isFloatingPoint(valueType) && valueType != Token::ValueType::Boolean && valueType != Token::ValueType::Character)
                    invalidType = true;
            } else {
                invalidType = true;
            }

            if (invalidType)
                throwParserError("invalid type for In/Out variable. Allowed types are: 'char', 'bool', floating point types or integral types");
        }

        return create(new ASTNodeVariableDecl(name, type, std::move(placementOffset), inVariable, outVariable));
    }

    // (parseType) Identifier[[(parseMathematicalExpression)]] @ Integer
    std::unique_ptr<ASTNode> Parser::parseArrayVariablePlacement(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ASTNode> size;

        if (!MATCHES(sequence(tkn::Separator::RightBracket))) {
            if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                throwParserError("expected closing ']' at end of array declaration", -1);
        }

        if (!MATCHES(sequence(tkn::Operator::At)))
            throwParserError("expected placement instruction", -1);

        auto placementOffset = parseMathematicalExpression();

        return create(new ASTNodeArrayVariableDecl(name, type, std::move(size), std::move(placementOffset)));
    }

    // (parseType) *Identifier : (parseType) @ Integer
    std::unique_ptr<ASTNode> Parser::parsePointerVariablePlacement(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        auto sizeType = parsePointerSizeType();

        if (!MATCHES(sequence(tkn::Operator::At)))
            throwParserError("expected placement instruction", -1);

        auto placementOffset = parseMathematicalExpression();

        return create(new ASTNodePointerVariableDecl(name, type, std::move(sizeType), std::move(placementOffset)));
    }

    // (parseType) *Identifier[[(parseMathematicalExpression)]] : (parseType) @ Integer
    std::unique_ptr<ASTNode> Parser::parsePointerArrayVariablePlacement(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ASTNode> size;

        if (!MATCHES(sequence(tkn::Separator::RightBracket))) {
            if (MATCHES(sequence(tkn::Keyword::While, tkn::Separator::LeftParenthesis)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(tkn::Separator::RightBracket)))
                throwParserError("expected closing ']' at end of array declaration", -1);
        }

        if (!MATCHES(sequence(tkn::Operator::Colon))) {
            throwParserError("expected type used for pointer size", -1);
        }

        auto sizeType = parsePointerSizeType();

        if (!MATCHES(sequence(tkn::Operator::At)))
            throwParserError("expected placement instruction", -1);

        auto placementOffset = parseMathematicalExpression();

        return create(new ASTNodePointerVariableDecl(name, create(new ASTNodeArrayVariableDecl("", std::move(type), std::move(size))), std::move(sizeType), std::move(placementOffset)));
    }

    std::vector<std::shared_ptr<ASTNode>> Parser::parseNamespace() {
        std::vector<std::shared_ptr<ASTNode>> statements;

        if (!MATCHES(sequence(tkn::Literal::Identifier())))
            throwParserError("expected namespace identifier");

        this->m_currNamespace.push_back(this->m_currNamespace.back());

        while (true) {
            this->m_currNamespace.back().push_back(getValue<Token::Identifier>(-1).get());

            if (MATCHES(sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier())))
                continue;
            else
                break;
        }

        if (!MATCHES(sequence(tkn::Separator::LeftBrace)))
            throwParserError("expected '{' at start of namespace");

        while (!MATCHES(sequence(tkn::Separator::RightBrace))) {
            auto newStatements = parseStatements();
            std::move(newStatements.begin(), newStatements.end(), std::back_inserter(statements));
        }

        this->m_currNamespace.pop_back();

        return statements;
    }

    std::unique_ptr<ASTNode> Parser::parsePlacement() {
        auto type = parseType();

        if (MATCHES(sequence(tkn::Literal::Identifier(), tkn::Separator::LeftBracket)))
            return parseArrayVariablePlacement(std::move(type));
        else if (MATCHES(sequence(tkn::Literal::Identifier())))
            return parseVariablePlacement(std::move(type));
        else if (MATCHES(sequence(tkn::Operator::Star, tkn::Literal::Identifier(), tkn::Operator::Colon)))
            return parsePointerVariablePlacement(std::move(type));
        else if (MATCHES(sequence(tkn::Operator::Star, tkn::Literal::Identifier(), tkn::Separator::LeftBracket)))
            return parsePointerArrayVariablePlacement(std::move(type));
        else throwParserError("invalid sequence", 0);
    }

    /* Program */

    // <(parseUsingDeclaration)|(parseVariablePlacement)|(parseStruct)>
    std::vector<std::shared_ptr<ASTNode>> Parser::parseStatements() {
        std::shared_ptr<ASTNode> statement;
        bool requiresSemicolon = true;

        if (MATCHES(sequence(tkn::Keyword::Using, tkn::Literal::Identifier(), tkn::Operator::Assign)))
            statement = parseUsingDeclaration();
        else if (MATCHES(sequence(tkn::Keyword::Using, tkn::Literal::Identifier())))
            parseForwardDeclaration();
        else if (peek(tkn::Keyword::BigEndian) || peek(tkn::Keyword::LittleEndian) || peek(tkn::ValueType::Any))
            statement = parsePlacement();
        else if (peek(tkn::Literal::Identifier()) && !peek(tkn::Operator::Assign, 1) && !peek(tkn::Separator::Dot, 1)  && !peek(tkn::Separator::LeftBracket, 1)) {
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
        else if (MATCHES(sequence(tkn::Keyword::Struct, tkn::Literal::Identifier())))
            statement = parseStruct();
        else if (MATCHES(sequence(tkn::Keyword::Union, tkn::Literal::Identifier(), tkn::Separator::LeftBrace)))
            statement = parseUnion();
        else if (MATCHES(sequence(tkn::Keyword::Enum, tkn::Literal::Identifier(), tkn::Operator::Colon)))
            statement = parseEnum();
        else if (MATCHES(sequence(tkn::Keyword::Bitfield, tkn::Literal::Identifier(), tkn::Separator::LeftBrace)))
            statement = parseBitfield();
        else if (MATCHES(sequence(tkn::Keyword::Function, tkn::Literal::Identifier(), tkn::Separator::LeftParenthesis)))
            statement = parseFunctionDefinition();
        else if (MATCHES(sequence(tkn::Keyword::Namespace)))
            return parseNamespace();
        else {
            statement = parseFunctionStatement();
            requiresSemicolon = false;
        }

        if (statement && MATCHES(sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket)))
            parseAttribute(dynamic_cast<Attributable *>(statement.get()));

        if (requiresSemicolon && !MATCHES(sequence(tkn::Separator::Semicolon)))
            throwParserError("missing ';' at end of expression", -1);

        // Consume superfluous semicolons
        while (MATCHES(sequence(tkn::Separator::Semicolon)))
            ;

        if (!statement)
            return { };

        return pl::moveToVector(std::move(statement));
    }

    std::shared_ptr<ASTNodeTypeDecl> Parser::addType(const std::string &name, std::unique_ptr<ASTNode> &&node, std::optional<std::endian> endian) {
        auto typeName = getNamespacePrefixedNames(name).back();

        if (this->m_types.contains(typeName) && this->m_types.at(typeName)->isForwardDeclared()) {
            this->m_types.at(typeName)->setType(std::move(node));

            return this->m_types.at(typeName);
        } else {
            if (this->m_types.contains(typeName))
                throwParserError(fmt::format("redefinition of type '{}'", typeName));

            std::shared_ptr typeDecl = create(new ASTNodeTypeDecl(typeName, std::move(node), endian));
            this->m_types.insert({ typeName, typeDecl });

            return typeDecl;
        }
    }

    // <(parseNamespace)...> EndOfProgram
    std::optional<std::vector<std::shared_ptr<ASTNode>>> Parser::parse(const std::vector<Token> &tokens) {
        this->m_curr = tokens.begin();

        this->m_types.clear();

        this->m_currNamespace.clear();
        this->m_currNamespace.emplace_back();

        try {
            auto program = parseTillToken(tkn::Separator::EndOfProgram);

            if (this->m_curr != tokens.end())
                throwParserError("program failed to parse completely", -1);

            return program;
        } catch (PatternLanguageError &e) {
            this->m_error = e;

            return std::nullopt;
        }
    }

}
