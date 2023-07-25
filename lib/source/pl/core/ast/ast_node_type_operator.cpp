#include <pl/core/ast/ast_node_type_operator.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>

namespace pl::core::ast {

    ASTNodeTypeOperator::ASTNodeTypeOperator(Token::Operator op, std::unique_ptr<ASTNode> &&expression) : m_op(op), m_expression(std::move(expression)) { }
    ASTNodeTypeOperator::ASTNodeTypeOperator(Token::Operator op) : m_op(op), m_providerOperation(true) { }

    ASTNodeTypeOperator::ASTNodeTypeOperator(const ASTNodeTypeOperator &other) : ASTNode(other) {
        this->m_op = other.m_op;
        this->m_providerOperation = other.m_providerOperation;

        if (other.m_expression != nullptr)
            this->m_expression = other.m_expression->clone();
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeTypeOperator::evaluate(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        Token::Literal result;
        if (this->m_providerOperation) {
            switch (this->getOperator()) {
                case Token::Operator::AddressOf:
                    result = u128(evaluator->getDataBaseAddress());
                    break;
                case Token::Operator::SizeOf:
                    result = u128(evaluator->getDataSize());
                    break;
                default:
                    err::E0001.throwError("Invalid type operation.", {}, this);
            }
        } else {
            auto offset = evaluator->getBitwiseReadOffset();
            evaluator->pushSectionId(ptrn::Pattern::InstantiationSectionId);
            ON_SCOPE_EXIT {
                evaluator->setBitwiseReadOffset(offset);
                evaluator->popSectionId();
            };

            auto patterns = this->m_expression->createPatterns(evaluator);
            if (patterns.empty())
                err::E0005.throwError("'auto' can only be used with parameters.", { }, this);

            auto &pattern = patterns.front();

            switch (this->getOperator()) {
                case Token::Operator::AddressOf:
                    result = u128(pattern->getOffset());
                    break;
                case Token::Operator::SizeOf:
                    result = u128(pattern->getSize());
                    break;
                case Token::Operator::TypeNameOf: {
                    if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(this->m_expression.get()); typeDecl != nullptr) {
                        ASTNodeTypeDecl *node;
                        while (true) {
                            node = dynamic_cast<ASTNodeTypeDecl*>(typeDecl->getType().get());
                            if (node != nullptr)
                                typeDecl = node;
                            else
                                break;
                        }

                        result = typeDecl->getName();
                    } else {
                        result = pattern->getTypeName();
                    }

                    break;
                }
                default:
                    err::E0001.throwError("Invalid type operation.", {}, this);
            }
        }

        return std::unique_ptr<ASTNode>(new ASTNodeLiteral(result));
    }

}