#include <pl/core/ast/ast_node_import_statement.hpp>

#include <pl/core/evaluator.hpp>

#include <utility>

namespace pl::core::ast {


    std::unique_ptr<ASTNode> ASTNodeImport::evaluate(pl::core::Evaluator *evaluator) const {
        // execute the imported statements

        evaluator->updateRuntime(this);

        for (const auto &item: this->m_importedStatements) {
            (void)item.evaluate(evaluator);
        }

        return nullptr;
    }

}