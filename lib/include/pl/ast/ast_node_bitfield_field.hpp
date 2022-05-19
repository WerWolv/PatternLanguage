#pragma once

#include <pl/ast/ast_node.hpp>
#include <pl/ast/ast_node_attribute.hpp>

#include <pl/patterns/pattern_bitfield.hpp>

namespace pl {

    class ASTNodeBitfieldField : public ASTNode,
                                 public Attributable {
    public:
        ASTNodeBitfieldField(std::string name, std::unique_ptr<ASTNode> &&size)
            : ASTNode(), m_name(std::move(name)), m_size(std::move(size)) { }

        ASTNodeBitfieldField(const ASTNodeBitfieldField &other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            this->m_size = other.m_size->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBitfieldField(*this));
        }

        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getSize() const { return this->m_size; }

        [[nodiscard]] bool isPadding() const { return this->getName() == "padding"; }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {
            auto literal = this->m_size->evaluate(evaluator);

            u8 bitSize = std::visit(overloaded {
                                        [this](const std::string &) -> u8 { LogConsole::abortEvaluation("bitfield field size cannot be a string", this); },
                                        [this](Pattern *) -> u8 { LogConsole::abortEvaluation("bitfield field size cannot be a custom type", this); },
                                        [](auto &&offset) -> u8 { return static_cast<u8>(offset); } },
                dynamic_cast<ASTNodeLiteral *>(literal.get())->getValue());

            auto pattern = std::make_unique<PatternBitfieldField>(evaluator, evaluator->dataOffset(), 0, bitSize);
            pattern->setPadding(this->isPadding());
            pattern->setVariableName(this->getName());

            return moveToVector<std::unique_ptr<Pattern>>({ std::move(pattern) });
        }

    private:
        std::string m_name;
        std::unique_ptr<ASTNode> m_size;
    };

}