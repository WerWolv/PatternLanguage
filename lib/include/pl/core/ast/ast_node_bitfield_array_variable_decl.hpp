#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_bitfield.hpp>
#include <pl/core/ast/ast_node_bitfield_field.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_while_statement.hpp>

#include <pl/patterns/pattern_array_dynamic.hpp>

namespace pl::core::ast {

    class ASTNodeBitfieldArrayVariableDecl : public ASTNode,
                                             public Attributable {
    public:
        ASTNodeBitfieldArrayVariableDecl(std::string name, std::shared_ptr<ASTNodeTypeDecl> type, std::unique_ptr<ASTNode> &&size)
            : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_size(std::move(size)) { }

        ASTNodeBitfieldArrayVariableDecl(const ASTNodeBitfieldArrayVariableDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            if (other.m_type->isForwardDeclared())
                this->m_type = other.m_type;
            else
                this->m_type = std::shared_ptr<ASTNodeTypeDecl>(static_cast<ASTNodeTypeDecl*>(other.m_type->clone().release()));

            if (other.m_size != nullptr)
                this->m_size = other.m_size->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBitfieldArrayVariableDecl(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto startOffset = evaluator->dataOffset();

            auto type = this->m_type->evaluate(evaluator);

            std::shared_ptr<ptrn::Pattern> pattern;
            if (dynamic_cast<ASTNodeBitfield *>(type.get()) != nullptr
                || dynamic_cast<ASTNodeBitfieldField *>(type.get()) != nullptr) {
                pattern = createArray(evaluator);
            } else {
                err::E0001.throwError("Bitfield arrays may only contain bitwise fields.", { }, this);
            }

            applyVariableAttributes(evaluator, this, pattern);

            if (evaluator->getSectionId() == ptrn::Pattern::PatternLocalSectionId) {
                evaluator->dataOffset() = startOffset;
                this->execute(evaluator);
                return { };
            } else {
                return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
            }
        }

        [[nodiscard]] const std::string &getName() const {
            return this->m_name;
        }

        [[nodiscard]] const std::shared_ptr<ASTNodeTypeDecl> &getType() const {
            return this->m_type;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getSize() const {
            return this->m_size;
        }

    private:
        std::string m_name;
        std::shared_ptr<ASTNodeTypeDecl> m_type;
        std::unique_ptr<ASTNode> m_size;

        std::unique_ptr<ptrn::Pattern> createArray(Evaluator *evaluator) const {
            auto startArrayIndex = evaluator->getCurrentArrayIndex();
            ON_SCOPE_EXIT {
                if (startArrayIndex.has_value())
                    evaluator->setCurrentArrayIndex(*startArrayIndex);
                else
                    evaluator->clearCurrentArrayIndex();
            };

            auto arrayPattern = std::make_unique<ptrn::PatternBitfieldArray>(evaluator, evaluator->dataOffset(), evaluator->getBitfieldBitOffset(), 0);
            arrayPattern->setVariableName(this->m_name);
            arrayPattern->setSection(evaluator->getSectionId());

            std::vector<std::shared_ptr<ptrn::Pattern>> entries;

            size_t size = 0;
            u128 entryIndex = 0;

            auto addEntries = [&](std::vector<std::shared_ptr<ptrn::Pattern>> &&patterns) {
                for (auto &pattern : patterns) {
                    pattern->setVariableName(fmt::format("[{}]", entryIndex));
                    pattern->setEndian(arrayPattern->getEndian());
                    if (pattern->getSection() == ptrn::Pattern::MainSectionId)
                        pattern->setSection(arrayPattern->getSection());

                    size += pattern->getSize();
                    entryIndex++;

                    entries.push_back(std::move(pattern));

                    evaluator->handleAbort();
                }
            };

            if (this->m_size != nullptr) {
                auto sizeNode = this->m_size->evaluate(evaluator);
                std::variant<u128, ASTNodeWhileStatement *> boundsCondition;

                if (auto literalNode = dynamic_cast<ASTNodeLiteral *>(sizeNode.get()); literalNode != nullptr) {
                    boundsCondition = std::visit(wolv::util::overloaded {
                        [this](const std::string &) -> u128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this); },
                        [this](ptrn::Pattern *pattern) -> u128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this); },
                        [](auto &&size) -> u128 { return size; }
                    }, literalNode->getValue());
                } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement *>(sizeNode.get()); whileStatement != nullptr) {
                    boundsCondition = whileStatement;
                } else {
                    err::E0001.throwError(fmt::format("Unexpected type of bitfield array size node."), {}, this);
                }

                auto limit = evaluator->getArrayLimit();
                auto checkLimit = [&](auto count) {
                    if (count > limit)
                        err::E0007.throwError(fmt::format("Bitfield array grew past set limit of {}", limit), "If this is intended, try increasing the limit using '#pragma array_limit <new_limit>'.", this);
                };

                if (std::holds_alternative<u128>(boundsCondition))
                    checkLimit(std::get<u128>(boundsCondition));

                u128 dataIndex = 0;

                auto checkCondition = [&]() {
                    if (std::holds_alternative<u128>(boundsCondition))
                        return dataIndex < std::get<u128>(boundsCondition);

                    checkLimit(entryIndex);
                    return std::get<ASTNodeWhileStatement *>(boundsCondition)->evaluateCondition(evaluator);
                };

                while (checkCondition()) {
                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                    evaluator->setCurrentArrayIndex(entryIndex);

                    auto patterns = this->m_type->createPatterns(evaluator);
                    size_t patternCount = patterns.size();

                    if (arrayPattern->getSection() == ptrn::Pattern::MainSectionId)
                        if ((evaluator->dataOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                            err::E0004.throwError("Bitfield array expanded past end of the data.", fmt::format("Entry {} exceeded data by {} bytes.", dataIndex, evaluator->dataOffset() - evaluator->getDataSize()), this);

                    auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                    if (ctrlFlow == ControlFlowStatement::Continue) {
                        entryIndex -= patternCount;
                        continue;
                    }

                    if (!patterns.empty())
                        addEntries(std::move(patterns));

                    if (ctrlFlow == ControlFlowStatement::Break || ctrlFlow == ControlFlowStatement::Return)
                        break;

                    dataIndex++;
                }
            } else {
                err::E0001.throwError(fmt::format("Bitfield array was created with no size."), {}, this);
            }

            if (!entries.empty()) {
                auto lastField = std::find_if(entries.rbegin(), entries.rend(), [](auto& pattern) {
                    return dynamic_cast<ptrn::PatternBitfieldMember *>(pattern.get()) != nullptr;
                });

                if (lastField != entries.rend()) {
                    auto &lastBitfieldMember = static_cast<ptrn::PatternBitfieldMember &>(*lastField->get());
                    auto totalBitSize = (lastBitfieldMember.getTotalBitOffset() + lastBitfieldMember.getBitSize()) - arrayPattern->getTotalBitOffset();
                    arrayPattern->setBitSize(totalBitSize);

                    for (auto &pattern : entries) {
                        if (auto bitfieldMember = dynamic_cast<ptrn::PatternBitfieldMember*>(pattern.get()); bitfieldMember != nullptr)
                            bitfieldMember->setParentBitfield(arrayPattern.get());
                    }

                    arrayPattern->setEntries(std::move(entries));
                }
            }

            if (arrayPattern->getEntryCount() > 0)
                arrayPattern->setTypeName(arrayPattern->getEntry(0)->getTypeName());

            return arrayPattern;
        }
    };

}