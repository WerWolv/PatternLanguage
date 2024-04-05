#include <pl/core/ast/ast_node.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace pl::core::ast {

    const Location& ASTNode::getLocation() const {
        return m_location;
    }

    void ASTNode::setLocation(const Location& location) {
        this->m_location = location;
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNode::evaluate(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        return this->clone();
    }

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNode::createPatterns(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        return {};
    }

    ASTNode::FunctionResult ASTNode::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        err::E0001.throwError("Cannot execute non-functional statement.", "This is a evaluator bug!", this->getLocation());
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