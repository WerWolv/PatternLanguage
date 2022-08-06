#pragma once

#include <pl/core/errors/error.hpp>

namespace pl::core::ast { class ASTNode; }

namespace pl::core::err {

    namespace {
        class EvaluatorError : public Error<const ast::ASTNode *> {
        public:
            EvaluatorError(u32 errorCode, std::string title) noexcept :
                    Error('E', errorCode, std::move(title)) { }
        };
    }

    const static inline EvaluatorError E0001(1, "Evaluator bug.");
    const static inline EvaluatorError E0002(2, "Math expression error.");
    const static inline EvaluatorError E0003(3, "Variable error.");
    const static inline EvaluatorError E0004(4, "Type error.");
    const static inline EvaluatorError E0005(5, "Placement error.");
    const static inline EvaluatorError E0006(6, "Array index error.");
    const static inline EvaluatorError E0007(7, "Limit error.");
    const static inline EvaluatorError E0008(8, "Attribute error.");
    const static inline EvaluatorError E0009(9, "Function error.");
    const static inline EvaluatorError E0010(10, "Control flow error.");
    const static inline EvaluatorError E0011(11, "Memory error.");
    const static inline EvaluatorError E0012(12, "Built-in function error.");

}