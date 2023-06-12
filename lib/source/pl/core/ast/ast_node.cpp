#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    [[nodiscard]] u32 ASTNode::getLine() const { return this->m_line; }
    [[nodiscard]] u32 ASTNode::getColumn() const { return this->m_column; }

    [[maybe_unused]] void ASTNode::setSourceLocation(u32 line, u32 column) {
        this->m_line = line;
        this->m_column = column;
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNode::evaluate(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        return this->clone();
    }

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNode::createPatterns(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        return {};
    }

    ASTNode::FunctionResult ASTNode::execute(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        err::E0001.throwError("Cannot execute non-functional statement.", "This is a evaluator bug!", this);
    }

    void ASTNode::setDocComment(const std::string &comment) {
        this->m_docComment = comment;
    }

    [[nodiscard]] const std::string &ASTNode::getDocComment() const {
        return this->m_docComment;
    }

    void ASTNode::setShouldDocument(bool shouldDocument) {
        this->m_document = shouldDocument;
    }

    [[nodiscard]] bool ASTNode::shouldDocument() const {
        return this->m_document;
    }

}