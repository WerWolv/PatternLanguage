#include <pl/core/ast/visitors/ast_stringify.hpp>

#include <pl/core/ast/ast_node.hpp>
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
#include <pl/core/ast/ast_node_match_statement.hpp>
#include <pl/core/ast/ast_node_mathematical_expression.hpp>
#include <pl/core/ast/ast_node_multi_variable_decl.hpp>
#include <pl/core/ast/ast_node_parameter_pack.hpp>
#include <pl/core/ast/ast_node_pointer_variable_decl.hpp>
#include <pl/core/ast/ast_node_rvalue.hpp>
#include <pl/core/ast/ast_node_rvalue_assignment.hpp>
#include <pl/core/ast/ast_node_scope_resolution.hpp>
#include <pl/core/ast/ast_node_struct.hpp>
#include <pl/core/ast/ast_node_template_parameter.hpp>
#include <pl/core/ast/ast_node_ternary_expression.hpp>
#include <pl/core/ast/ast_node_try_catch_statement.hpp>
#include <pl/core/ast/ast_node_type_appilication.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_type_operator.hpp>
#include <pl/core/ast/ast_node_union.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_while_statement.hpp>

#include <pl/core/token.hpp>

#include <wolv/utils/core.hpp>
#include <fmt/format.h>

namespace pl::core::ast {

    namespace {

        constexpr int PrecNone    = 0;
        constexpr int PrecTernary = 1;
        constexpr int PrecUnary   = 13;
        constexpr int PrecAtom    = 100;

        int getOperatorPrecedence(Token::Operator op) {
            using Op = Token::Operator;
            switch (op) {
                case Op::Star:
                case Op::Slash:
                case Op::Percent:                          return 12;
                case Op::Plus:
                case Op::Minus:                            return 11;
                case Op::LeftShift:
                case Op::RightShift:                       return 10;
                case Op::BitAnd:                           return 9;
                case Op::BitXor:                           return 8;
                case Op::BitOr:                            return 7;
                case Op::BoolLessThan:
                case Op::BoolGreaterThan:
                case Op::BoolLessThanOrEqual:
                case Op::BoolGreaterThanOrEqual:           return 6;
                case Op::BoolEqual:
                case Op::BoolNotEqual:                     return 5;
                case Op::BoolAnd:                          return 4;
                case Op::BoolXor:                          return 3;
                case Op::BoolOr:                           return 2;
                default:                                   return PrecTernary;
            }
        }

        bool isZeroIntegerLiteral(const ASTNode &node) {
            // The parser's unary encoding (see Parser::parseUnaryExpression) always uses a signed
            // zero literal. Only that exact shape may be folded back into '-x'; a user-written
            // "0u - x" has to keep its form because re-parsing '-x' would change its signedness.
            const auto *literal = dynamic_cast<const ASTNodeLiteral *>(&node);
            if (literal == nullptr)
                return false;

            const auto &value = literal->getValue();
            if (const auto *signedValue = std::get_if<i128>(&value); signedValue != nullptr)
                return *signedValue == 0;
            return false;
        }

        bool isUnaryMathExpression(const ASTNodeMathematicalExpression &node) {
            // The parser represents unary +, -, !, ~ as "0 <op> expr"
            using Op = Token::Operator;
            const auto op = node.getOperator();
            if (op != Op::Plus && op != Op::Minus && op != Op::BoolNot && op != Op::BitNot)
                return false;

            return node.getLeftOperand() != nullptr && isZeroIntegerLiteral(*node.getLeftOperand());
        }

        bool isAngleAmbiguousOperator(Token::Operator op) {
            // The lexer emits '<' and '>' as single tokens; '>>', '>=' and friends are composed by the
            // parser. In template argument context a '>' is never consumed as an operator because it
            // closes the argument list, so '>', '>=' and '>>' can never appear unparenthesized.
            // '<' and '<=' are just as bad: once the relation level consumes them, its operator loop
            // then eats the closing '>' as another comparison. '<<' is the exception — both '<' are
            // consumed as a single shift operator before that can happen, so it parses bare.
            using Op = Token::Operator;
            return op == Op::BoolLessThan || op == Op::BoolGreaterThan
                || op == Op::BoolLessThanOrEqual || op == Op::BoolGreaterThanOrEqual
                || op == Op::RightShift;
        }

        std::string stringifyTypeEndian(const ASTNodeTypeApplication &typeApp) {
            auto endian = typeApp.getEndian();

            if (!endian.has_value())
                return "";
            else if (endian == std::endian::little)
                return "le ";
            else if (endian == std::endian::big)
                return "be ";
            else
                return "";
        }

        std::string resolveTemplateParamName(int index, const ASTStringifier::TemplateParameters *parentParams) {
            // The parser indexes template parameters by type parameters only (see Parser::getCustomType),
            // so lookups must skip over non-type (auto) parameters.
            if (parentParams == nullptr || index < 0)
                return "???";

            int typeIndex = 0;
            for (const auto &param : *parentParams) {
                if (!param->isType())
                    continue;
                if (typeIndex == index)
                    return std::string(param->getName().get());
                typeIndex++;
            }

            return "???";
        }

    }

    std::string ASTStringifier::stringifyExpression(const ASTNode *node, int parenDepth, const TemplateParameters *parentParams) {
        return stringify(node, { parenDepth, PrecNone, false, parentParams });
    }

    std::string ASTStringifier::stringifyTypeName(const ASTNode *type, const TemplateParameters *parentParams) {
        return stringify(type, { 0, PrecNone, true, parentParams });
    }

    std::string ASTStringifier::stringify(const ASTNode *node, const Context &context) {
        if (node == nullptr)
            return "???";

        // Each nested node is printed with its own context, so the state needs to be
        // saved and restored around the dispatch
        const auto savedContext = m_context;
        const auto savedResult = m_result;
        const auto savedPrecedence = m_precedence;
        const auto savedParenthesized = m_parenthesized;

        m_context = context;
        m_result = "???";
        m_precedence = PrecAtom;
        m_parenthesized = false;

        // accept() isn't const but visitors never modify the nodes they're given
        const_cast<ASTNode *>(node)->accept(*this);

        auto result = std::move(m_result);
        if (m_parenthesized)
            result = fmt::format("({})", result);

        m_context = savedContext;
        m_result = std::move(savedResult);
        m_precedence = savedPrecedence;
        m_parenthesized = savedParenthesized;

        return result;
    }

    void ASTStringifier::beginNode(int precedence, bool angleAmbiguous) {
        m_precedence = precedence;
        m_parenthesized = precedence < m_context.minPrecedence
            || (m_context.parenDepth == 0 && angleAmbiguous);
    }

    ASTStringifier::Context ASTStringifier::childContext(int minPrecedence, int depthOffset) const {
        return {
            m_context.parenDepth + (m_parenthesized ? 1 : 0) + depthOffset,
            minPrecedence,
            false,
            m_context.parentParams
        };
    }

    void ASTStringifier::visit(ASTNode &node) {
        // Nodes that can't appear in an expression or type name have no printable form
        wolv::util::unused(node);

        m_result = "???";
        m_precedence = PrecAtom;
    }

    void ASTStringifier::visit(ASTNodeLiteral &node) {
        beginNode(PrecAtom);

        const auto &value = node.getValue();
        if (value.isString())
            m_result = fmt::format("\"{}\"", value.toString());
        else if (value.isCharacter())
            m_result = fmt::format("'{}'", value.toString(true));
        else {
            m_result = value.toString(true);
            // Unsigned literals keep their suffix so they don't turn back into signed ones on re-parse
            if (value.isUnsigned() && m_keepUnsignedSuffix)
                m_result += "u";
        }
    }

    void ASTStringifier::visit(ASTNodeRValue &node) {
        beginNode(PrecAtom);

        std::string result;
        for (const auto &segment : node.getPath()) {
            if (std::holds_alternative<std::string>(segment)) {
                const auto &name = std::get<std::string>(segment);
                if (!result.empty())
                    result += ".";
                result += name;
            } else if (const auto *expression = std::get_if<std::unique_ptr<ASTNode>>(&segment); expression != nullptr && *expression != nullptr) {
                result += fmt::format("[{}]", stringify(expression->get(), childContext(PrecNone, 1)));
            }
        }

        m_result = result.empty() ? "???" : result;
    }

    void ASTStringifier::visit(ASTNodeMathematicalExpression &node) {
        const auto op = node.getOperator();

        if (isUnaryMathExpression(node)) {
            // Unary operators are right-associative: "- -x" reparses as -(-x) without parens.
            beginNode(PrecUnary);
            m_result = fmt::format("{}{}", Token::getOperatorName(op),
                stringify(node.getRightOperand().get(), childContext(PrecUnary)));
            return;
        }

        const auto precedence = getOperatorPrecedence(op);
        beginNode(precedence, isAngleAmbiguousOperator(op));

        m_result = fmt::format("{} {} {}",
            stringify(node.getLeftOperand().get(), childContext(precedence)),
            Token::getOperatorName(op),
            stringify(node.getRightOperand().get(), childContext(precedence + 1)));
    }

    void ASTStringifier::visit(ASTNodeBuiltinType &node) {
        beginNode(PrecAtom);

        m_result = Token::getTypeName(node.getType());
    }

    void ASTStringifier::visit(ASTNodeTypeApplication &node) {
        beginNode(PrecAtom);

        const std::string endian = stringifyTypeEndian(node);
        const auto *type = node.getType().get();

        std::string typeName;
        if (type == nullptr) {
            typeName = resolveTemplateParamName(node.getTemplateParameterIndex(), m_context.parentParams);
        } else {
            typeName = stringify(type, { 0, PrecNone, true, m_context.parentParams });
        }

        const auto &templateArguments = node.getTemplateArguments();
        if (templateArguments.empty()) {
            m_result = endian + typeName;
            return;
        }

        std::string arguments;
        for (const auto &argument : templateArguments) {
            if (!arguments.empty())
                arguments += ", ";
            // Template arguments are always printed in template argument context
            arguments += stringify(argument.get(), { 0, PrecNone, false, m_context.parentParams });
        }

        m_result = fmt::format("{}{}<{}>", endian, typeName, arguments);
    }

    void ASTStringifier::visit(ASTNodeFunctionCall &node) {
        beginNode(PrecAtom);

        std::string result = node.getFunctionName() + "(";
        const auto &params = node.getParams();
        for (size_t i = 0; i < params.size(); i++) {
            if (i > 0)
                result += ", ";
            result += stringify(params[i].get(), childContext(PrecNone, 1));
        }

        m_result = result + ")";
    }

    void ASTStringifier::visit(ASTNodeTernaryExpression &node) {
        beginNode(PrecTernary);

        // The parser consumes ternaries left-associatively in a loop, so any nested ternary in
        // any operand position must keep its parentheses to round-trip.
        m_result = fmt::format("{} ? {} : {}",
            stringify(node.getFirstOperand().get(), childContext(PrecTernary + 1)),
            stringify(node.getSecondOperand().get(), childContext(PrecTernary + 1)),
            stringify(node.getThirdOperand().get(), childContext(PrecTernary + 1)));
    }

    void ASTStringifier::visit(ASTNodeCast &node) {
        beginNode(PrecAtom);

        const auto typeString = stringify(node.getType().get(), { 0, PrecNone, true, m_context.parentParams });

        if (node.isReinterpret()) {
            // `expr as T`: the value binds at factor level, so only atoms stay unparenthesized
            m_result = fmt::format("{} as {}", stringify(node.getValue().get(), childContext(PrecAtom)), typeString);
        } else {
            // C-style cast T(expr): the value sits inside the cast's own parentheses
            m_result = fmt::format("{}({})", typeString, stringify(node.getValue().get(), childContext(PrecNone, 1)));
        }
    }

    void ASTStringifier::visit(ASTNodeTypeOperator &node) {
        beginNode(PrecAtom);

        const auto opName = Token::getOperatorName(node.getOperator());
        if (node.getExpression() != nullptr)
            m_result = fmt::format("{}({})", opName, stringify(node.getExpression().get(), childContext(PrecNone, 1)));
        else
            m_result = fmt::format("{}($)", opName);
    }

    void ASTStringifier::visit(ASTNodeScopeResolution &node) {
        beginNode(PrecAtom);

        m_result = fmt::format("{}::{}", node.getType()->getName(), node.getName());
    }

    void ASTStringifier::visit(ASTNodeTypeDecl &node) {
        beginNode(PrecAtom);

        // Type declarations only have a name when they're the target of a type name,
        // everywhere else they can't be printed
        if (m_context.typeName)
            m_result = node.getName();
        else
            m_result = "???";
    }

    // Node types that can't appear in an expression or type name delegate to the generic handler
    void ASTStringifier::visit(ASTNodeArrayVariableDecl &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeAttribute &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeBitfield &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeBitfieldArrayVariableDecl &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeBitfieldField &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeBitfieldFieldSigned &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeBitfieldFieldSizedType &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeCompoundStatement &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeConditionalStatement &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeControlFlowStatement &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeEnum &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeFunctionDefinition &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeImportedType &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeLValueAssignment &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeMatchStatement &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeMultiVariableDecl &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeParameterPack &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodePointerVariableDecl &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeRValueAssignment &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeStruct &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeTemplateParameter &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeTryCatchStatement &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeUnion &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeVariableDecl &node) { visit(static_cast<ASTNode &>(node)); }
    void ASTStringifier::visit(ASTNodeWhileStatement &node) { visit(static_cast<ASTNode &>(node)); }

}
