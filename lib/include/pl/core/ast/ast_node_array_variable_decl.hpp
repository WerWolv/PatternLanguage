#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>
#include <pl/core/ast/ast_node_while_statement.hpp>

#include <pl/patterns/pattern_padding.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_wide_string.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>

namespace pl::core::ast {

    class ASTNodeArrayVariableDecl : public ASTNode,
                                     public Attributable {
    public:
        ASTNodeArrayVariableDecl(std::string name, std::shared_ptr<ASTNodeTypeDecl> type, std::unique_ptr<ASTNode> &&size, std::unique_ptr<ASTNode> &&placementOffset = {}, std::unique_ptr<ASTNode> &&placementSection = {}, bool constant = false)
            : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_size(std::move(size)), m_placementOffset(std::move(placementOffset)), m_placementSection(std::move(placementSection)), m_constant(constant) { }

        ASTNodeArrayVariableDecl(const ASTNodeArrayVariableDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            if (other.m_type->isForwardDeclared())
                this->m_type = other.m_type;
            else
                this->m_type = std::shared_ptr<ASTNodeTypeDecl>(static_cast<ASTNodeTypeDecl*>(other.m_type->clone().release()));

            if (other.m_size != nullptr)
                this->m_size = other.m_size->clone();

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();

            if (other.m_placementSection != nullptr)
                this->m_placementSection = other.m_placementSection->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeArrayVariableDecl(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto startOffset = evaluator->dataOffset();

            auto scopeGuard = SCOPE_GUARD {
                evaluator->popSectionId();
            };

            if (this->m_placementSection != nullptr) {
                const auto node = this->m_placementSection->evaluate(evaluator);
                const auto id = dynamic_cast<ASTNodeLiteral *>(node.get());
                if (id == nullptr)
                    err::E0010.throwError("Cannot use void expression as section identifier.", {}, this);

                evaluator->pushSectionId(id->getValue().toUnsigned());
            } else {
                scopeGuard.release();
            }

            if (this->m_placementOffset != nullptr) {
                auto evaluatedPlacement = this->m_placementOffset->evaluate(evaluator);
                auto offset             = dynamic_cast<ASTNodeLiteral *>(evaluatedPlacement.get());
                if (offset == nullptr)
                    err::E0010.throwError("Cannot use void expression as placement offset.", {}, this);

                evaluator->dataOffset() = std::visit(wolv::util::overloaded {
                    [this](const std::string &) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this); },
                    [this](ptrn::Pattern *) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this); },
                    [](auto &&offset) -> u64 { return offset; }
                }, offset->getValue());
            }

            auto type = this->m_type->evaluate(evaluator);

            std::shared_ptr<ptrn::Pattern> pattern;
            if (dynamic_cast<ASTNodeBuiltinType *>(type.get()))
                pattern = createStaticArray(evaluator);
            else if (auto attributable = dynamic_cast<Attributable *>(type.get())) {
                bool isStaticType = attributable->hasAttribute("static", false);

                if (isStaticType)
                    pattern = createStaticArray(evaluator);
                else
                    pattern = createDynamicArray(evaluator);
            } else {
                err::E0001.throwError("Invalid type used in array variable declaration.", { }, this);
            }

            pattern->setSection(evaluator->getSectionId());

            applyVariableAttributes(evaluator, this, pattern);

            if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
                evaluator->dataOffset() = startOffset;
            }

            if (evaluator->getSectionId() == ptrn::Pattern::PatternLocalSectionId) {
                evaluator->dataOffset() = startOffset;
                this->execute(evaluator);
                return { };
            } else {
                return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
            }
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            if (this->m_size == nullptr)
                err::E0004.throwError("Function arrays cannot be unsized.", {}, this);

            auto sizeNode = this->m_size->evaluate(evaluator);
            auto sizeLiteral = dynamic_cast<ASTNodeLiteral*>(sizeNode.get());
            if (sizeLiteral == nullptr)
                err::E0004.throwError("Function arrays require a fixed size.", {}, this);

            auto entryCount = std::visit(wolv::util::overloaded {
                [this](const std::string &) -> i128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this); },
                [this](ptrn::Pattern *pattern) -> i128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this); },
                [](auto &&size) -> i128 { return size; }
            }, sizeLiteral->getValue());

            if (this->m_placementOffset != nullptr) {
                const auto placementNode = this->m_placementOffset->evaluate(evaluator);
                const auto offsetLiteral = dynamic_cast<ASTNodeLiteral *>(placementNode.get());
                if (offsetLiteral == nullptr)
                    err::E0002.throwError("Void expression used in placement expression.", { }, this);


                u64 section = 0;
                if (this->m_placementSection != nullptr) {
                    const auto sectionNode = this->m_placementSection->evaluate(evaluator);
                    const auto sectionLiteral = dynamic_cast<ASTNodeLiteral *>(sectionNode.get());
                    if (sectionLiteral == nullptr)
                        err::E0002.throwError("Cannot use void expression as section identifier.", {}, this);

                    section = sectionLiteral->getValue().toUnsigned();
                }

                evaluator->createArrayVariable(this->m_name, this->m_type.get(), entryCount, section, this->m_constant);
                evaluator->setVariableAddress(this->getName(), offsetLiteral->getValue().toUnsigned(), section);
            } else {
                evaluator->createArrayVariable(this->m_name, this->m_type.get(), entryCount, ptrn::Pattern::HeapSectionId, this->m_constant);
            }

            return std::nullopt;
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

        [[nodiscard]] const std::unique_ptr<ASTNode> &getPlacementOffset() const {
            return this->m_placementOffset;
        }

        [[nodiscard]] bool isConstant() const {
            return this->m_constant;
        }

    private:
        std::string m_name;
        std::shared_ptr<ASTNodeTypeDecl> m_type;
        std::unique_ptr<ASTNode> m_size;
        std::unique_ptr<ASTNode> m_placementOffset, m_placementSection;
        bool m_constant;

        std::unique_ptr<ptrn::Pattern> createStaticArray(Evaluator *evaluator) const {
            u64 startOffset = evaluator->dataOffset();

            auto templatePatterns = this->m_type->createPatterns(evaluator);
            if (templatePatterns.empty())
                err::E0005.throwError("'auto' can only be used with parameters.", { }, this);

            auto &templatePattern = templatePatterns.front();

            templatePattern->setSection(evaluator->getSectionId());

            evaluator->dataOffset() = startOffset;

            i128 entryCount = 0;

            if (this->m_size != nullptr) {
                auto sizeNode = this->m_size->evaluate(evaluator);

                if (auto literal = dynamic_cast<ASTNodeLiteral *>(sizeNode.get()); literal != nullptr) {
                    entryCount = std::visit(wolv::util::overloaded {
                        [this](const std::string &) -> i128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this); },
                        [this](ptrn::Pattern *pattern) -> i128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this); },
                        [](auto &&size) -> i128 { return size; }
                    }, literal->getValue());
                } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement *>(sizeNode.get())) {
                    while (whileStatement->evaluateCondition(evaluator)) {
                        if (templatePattern->getSection() == ptrn::Pattern::MainSectionId)
                            if ((evaluator->dataOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                                err::E0004.throwError("Array expanded past end of the data before termination condition was met.", { }, this);

                        evaluator->handleAbort();
                        entryCount++;
                        evaluator->dataOffset() += templatePattern->getSize();
                    }
                }

                if (entryCount < 0)
                    err::E0004.throwError("Array size cannot be negative.", { }, this);
            } else {
                std::vector<u8> buffer(templatePattern->getSize());
                while (true) {
                    if (templatePattern->getSection() == ptrn::Pattern::MainSectionId)
                        if ((evaluator->dataOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                            err::E0004.throwError("Array expanded past end of the data before a null-entry was found.", "Try using a while-sized array instead to limit the size of the array.", this);

                    evaluator->readData(evaluator->dataOffset(), buffer.data(), buffer.size(), templatePattern->getSection());
                    evaluator->dataOffset() += buffer.size();

                    entryCount++;

                    bool reachedEnd = true;
                    for (u8 &byte : buffer) {
                        if (byte != 0x00) {
                            reachedEnd = false;
                            break;
                        }
                    }

                    if (reachedEnd) break;
                    evaluator->handleAbort();
                }
            }

            std::unique_ptr<ptrn::Pattern> outputPattern;
            if (dynamic_cast<ptrn::PatternPadding *>(templatePattern.get())) {
                outputPattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternPadding(evaluator, startOffset, 0));
            } else if (dynamic_cast<ptrn::PatternCharacter *>(templatePattern.get())) {
                outputPattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternString(evaluator, startOffset, 0));
            } else if (dynamic_cast<ptrn::PatternWideCharacter *>(templatePattern.get())) {
                outputPattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternWideString(evaluator, startOffset, 0));
            } else {
                auto arrayPattern = std::make_unique<ptrn::PatternArrayStatic>(evaluator, startOffset, 0);
                arrayPattern->setEntries(templatePattern->clone(), entryCount);
                arrayPattern->setSection(templatePattern->getSection());
                outputPattern = std::move(arrayPattern);
            }

            outputPattern->setVariableName(this->m_name);
            if (templatePattern->hasOverriddenEndian())
                outputPattern->setEndian(templatePattern->getEndian());
            outputPattern->setTypeName(templatePattern->getTypeName());
            outputPattern->setSize(templatePattern->getSize() * entryCount);
            outputPattern->setSection(templatePattern->getSection());

            evaluator->dataOffset() = startOffset + outputPattern->getSize();

            if (outputPattern->getSection() == ptrn::Pattern::MainSectionId)
                if ((evaluator->dataOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                    err::E0004.throwError("Array expanded past end of the data.", { }, this);

            return outputPattern;
        }

        std::unique_ptr<ptrn::Pattern> createDynamicArray(Evaluator *evaluator) const {
            auto startArrayIndex = evaluator->getCurrentArrayIndex();
            ON_SCOPE_EXIT {
                if (startArrayIndex.has_value())
                    evaluator->setCurrentArrayIndex(*startArrayIndex);
                else
                    evaluator->clearCurrentArrayIndex();
            };

            auto arrayPattern = std::make_unique<ptrn::PatternArrayDynamic>(evaluator, evaluator->dataOffset(), 0);
            arrayPattern->setVariableName(this->m_name);
            arrayPattern->setSection(evaluator->getSectionId());

            std::vector<std::shared_ptr<ptrn::Pattern>> entries;

            size_t size    = 0;
            u64 entryIndex = 0;

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

            auto discardEntries = [&](u32 count) {
                for (u32 i = 0; i < count; i++) {
                    entries.pop_back();
                    entryIndex--;
                }
            };

            if (this->m_size != nullptr) {
                auto sizeNode = this->m_size->evaluate(evaluator);

                if (auto literal = dynamic_cast<ASTNodeLiteral *>(sizeNode.get()); literal != nullptr) {
                    auto entryCount = std::visit(wolv::util::overloaded {
                        [this](const std::string &) -> u128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this); },
                        [this](ptrn::Pattern *pattern) -> u128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this); },
                        [](auto &&size) -> u128 { return size; }
                    }, literal->getValue());

                    auto limit = evaluator->getArrayLimit();
                    if (entryCount > limit)
                        err::E0007.throwError(fmt::format("Array grew past set limit of {}", limit), "If this is intended, try increasing the limit using '#pragma array_limit <new_limit>'.", this);

                    for (u64 i = 0; i < entryCount; i++) {
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                        evaluator->setCurrentArrayIndex(i);

                        auto patterns = this->m_type->createPatterns(evaluator);
                        size_t patternCount = patterns.size();

                        if (arrayPattern->getSection() == ptrn::Pattern::MainSectionId)
                            if ((evaluator->dataOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                                err::E0004.throwError("Array expanded past end of the data.", fmt::format("Entry {} exceeded data by {} bytes.", i, evaluator->dataOffset() - evaluator->getDataSize()), this);

                        if (!patterns.empty())
                            addEntries(std::move(patterns));

                        auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                        if (ctrlFlow == ControlFlowStatement::Break || ctrlFlow == ControlFlowStatement::Return)
                            break;
                        else if (ctrlFlow == ControlFlowStatement::Continue) {

                            discardEntries(patternCount);
                            continue;
                        }
                    }
                } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement *>(sizeNode.get())) {
                    while (whileStatement->evaluateCondition(evaluator)) {
                        auto limit = evaluator->getArrayLimit();
                        if (entryIndex > limit)
                            err::E0007.throwError(fmt::format("Array grew past set limit of {}", limit), "If this is intended, try increasing the limit using '#pragma array_limit <new_limit>'.", this);

                        evaluator->setCurrentArrayIndex(entryIndex);

                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                        auto patterns       = this->m_type->createPatterns(evaluator);
                        size_t patternCount = patterns.size();

                        if (arrayPattern->getSection() == ptrn::Pattern::MainSectionId)
                            if ((evaluator->dataOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                                err::E0004.throwError("Array expanded past end of the data before termination condition was met.", { }, this);

                        if (!patterns.empty())
                            addEntries(std::move(patterns));


                        auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                        if (ctrlFlow == ControlFlowStatement::Break)
                            break;
                        else if (ctrlFlow == ControlFlowStatement::Continue) {
                            discardEntries(patternCount);
                            continue;
                        }
                    }
                }
            } else {
                while (true) {
                    bool reachedEnd = true;
                    auto limit      = evaluator->getArrayLimit();
                    if (entryIndex > limit)
                        err::E0007.throwError(fmt::format("Array grew past set limit of {}", limit), "If this is intended, try increasing the limit using '#pragma array_limit <new_limit>'.", this);

                    evaluator->setCurrentArrayIndex(entryIndex);

                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                    auto patterns = this->m_type->createPatterns(evaluator);

                    for (auto &pattern : patterns) {
                        std::vector<u8> buffer(pattern->getSize());

                        if (arrayPattern->getSection() == ptrn::Pattern::MainSectionId)
                            if ((evaluator->dataOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                                err::E0004.throwError("Array expanded past end of the data before a null-entry was found.", "Try using a while-sized array instead to limit the size of the array.", this);

                        const auto patternSize = pattern->getSize();
                        evaluator->readData(evaluator->dataOffset() - patternSize, buffer.data(), buffer.size(), pattern->getSection());

                        addEntries(hlp::moveToVector(std::move(pattern)));

                        auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                        if (ctrlFlow == ControlFlowStatement::None)
                            break;

                        reachedEnd = true;
                        for (u8 &byte : buffer) {
                            if (byte != 0x00) {
                                reachedEnd = false;
                                break;
                            }
                        }

                        if (reachedEnd) break;
                    }

                    auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                    if (ctrlFlow == ControlFlowStatement::Break)
                        break;
                    else if (ctrlFlow == ControlFlowStatement::Continue) {
                        discardEntries(1);
                        continue;
                    }

                    if (reachedEnd) break;
                }
            }


            if (arrayPattern->getEntryCount() > 0)
                arrayPattern->setTypeName(arrayPattern->getEntry(0)->getTypeName());

            arrayPattern->setEntries(std::move(entries));
            arrayPattern->setSize(size);

            return arrayPattern;
        }
    };

}