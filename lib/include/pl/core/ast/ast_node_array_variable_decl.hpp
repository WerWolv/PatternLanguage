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
        ASTNodeArrayVariableDecl(std::string name, std::shared_ptr<ASTNodeTypeDecl> type, std::unique_ptr<ASTNode> &&size, std::unique_ptr<ASTNode> &&placementOffset = {})
            : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_size(std::move(size)), m_placementOffset(std::move(placementOffset)) { }

        ASTNodeArrayVariableDecl(const ASTNodeArrayVariableDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type;
            if (other.m_size != nullptr)
                this->m_size = other.m_size->clone();
            else
                this->m_size = nullptr;

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();
            else
                this->m_placementOffset = nullptr;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeArrayVariableDecl(*this));
        }

        [[nodiscard]] std::vector<std::unique_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            auto startOffset = evaluator->dataOffset();

            if (this->m_placementOffset != nullptr) {
                auto evaluatedPlacement = this->m_placementOffset->evaluate(evaluator);
                auto offset             = dynamic_cast<ASTNodeLiteral *>(evaluatedPlacement.get());

                evaluator->dataOffset() = std::visit(hlp::overloaded {
                    [this](const std::string &) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this); },
                    [this](ptrn::Pattern *) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this); },
                    [](auto &&offset) -> u64 { return offset; }
                }, offset->getValue());
            }

            auto type = this->m_type->evaluate(evaluator);

            std::unique_ptr<ptrn::Pattern> pattern;
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

            applyVariableAttributes(evaluator, this, pattern.get());

            if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
                evaluator->dataOffset() = startOffset;
            }

            return hlp::moveToVector(std::move(pattern));
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            auto sizeNode = this->m_size->evaluate(evaluator);
            auto sizeLiteral = dynamic_cast<ASTNodeLiteral*>(sizeNode.get());

            if (sizeLiteral != nullptr) {
                auto entryCount = std::visit(hlp::overloaded {
                    [this](const std::string &) -> i128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this); },
                    [this](ptrn::Pattern *pattern) -> i128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this); },
                    [](auto &&size) -> i128 { return size; }
                }, sizeLiteral->getValue());

                evaluator->createArrayVariable(this->m_name, this->m_type.get(), entryCount);
            }

            return std::nullopt;
        }

    private:
        std::string m_name;
        std::shared_ptr<ASTNodeTypeDecl> m_type;
        std::unique_ptr<ASTNode> m_size;
        std::unique_ptr<ASTNode> m_placementOffset;

        std::unique_ptr<ptrn::Pattern> createStaticArray(Evaluator *evaluator) const {
            u64 startOffset = evaluator->dataOffset();

            auto templatePatterns = this->m_type->createPatterns(evaluator);
            auto &templatePattern = templatePatterns.front();

            evaluator->dataOffset() = startOffset;

            i128 entryCount = 0;

            if (this->m_size != nullptr) {
                auto sizeNode = this->m_size->evaluate(evaluator);

                if (auto literal = dynamic_cast<ASTNodeLiteral *>(sizeNode.get())) {
                    entryCount = std::visit(hlp::overloaded {
                        [this](const std::string &) -> i128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this); },
                        [this](ptrn::Pattern *pattern) -> i128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this); },
                        [](auto &&size) -> i128 { return size; }
                    }, literal->getValue());
                } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement *>(sizeNode.get())) {
                    while (whileStatement->evaluateCondition(evaluator)) {
                        entryCount++;
                        evaluator->dataOffset() += templatePattern->getSize();
                        evaluator->handleAbort();
                    }
                }

                if (entryCount < 0)
                    err::E0004.throwError("Array size cannot be negative.", { }, this);
            } else {
                std::vector<u8> buffer(templatePattern->getSize());
                while (true) {
                    if (evaluator->dataOffset() > evaluator->getDataSize() - buffer.size())
                        err::E0004.throwError("Array expanded past end of the data before a null-entry was found.", "Try using a while-sized array instead to limit the size of the array.", this);

                    evaluator->readData(evaluator->dataOffset(), buffer.data(), buffer.size());
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
                outputPattern = std::move(arrayPattern);
            }

            outputPattern->setVariableName(this->m_name);
            outputPattern->setEndian(templatePattern->getEndian());
            outputPattern->setTypeName(templatePattern->getTypeName());
            outputPattern->setSize(templatePattern->getSize() * entryCount);

            evaluator->dataOffset() = startOffset + outputPattern->getSize();

            return outputPattern;
        }

        std::unique_ptr<ptrn::Pattern> createDynamicArray(Evaluator *evaluator) const {
            auto arrayPattern = std::make_unique<ptrn::PatternArrayDynamic>(evaluator, evaluator->dataOffset(), 0);
            arrayPattern->setVariableName(this->m_name);

            std::vector<std::shared_ptr<ptrn::Pattern>> entries;

            size_t size    = 0;
            u64 entryIndex = 0;

            auto addEntries = [&](std::vector<std::unique_ptr<ptrn::Pattern>> &&patterns) {
                for (auto &pattern : patterns) {
                    pattern->setVariableName(fmt::format("[{}]", entryIndex));
                    pattern->setEndian(arrayPattern->getEndian());

                    size += pattern->getSize();
                    entryIndex++;

                    if (evaluator->dataOffset() > evaluator->getDataSize() - pattern->getSize())
                        err::E0004.throwError("Array expanded past end of the data before termination condition was met.", { }, this);

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

                if (auto literal = dynamic_cast<ASTNodeLiteral *>(sizeNode.get())) {
                    auto entryCount = std::visit(hlp::overloaded {
                        [this](const std::string &) -> u128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this); },
                        [this](ptrn::Pattern *pattern) -> u128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this); },
                        [](auto &&size) -> u128 { return size; }
                    }, literal->getValue());

                    auto limit = evaluator->getArrayLimit();
                    if (entryCount > limit)
                        err::E0007.throwError(fmt::format("Array grew past set limit of {}", limit), "If this is intended, try increasing the limit using '#pragma array_limit <new_limit>'.", this);

                    for (u64 i = 0; i < entryCount; i++) {
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                        auto patterns       = this->m_type->createPatterns(evaluator);
                        size_t patternCount = patterns.size();

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
                } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement *>(sizeNode.get())) {
                    while (whileStatement->evaluateCondition(evaluator)) {
                        auto limit = evaluator->getArrayLimit();
                        if (entryIndex > limit)
                            err::E0007.throwError(fmt::format("Array grew past set limit of {}", limit), "If this is intended, try increasing the limit using '#pragma array_limit <new_limit>'.", this);

                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                        auto patterns       = this->m_type->createPatterns(evaluator);
                        size_t patternCount = patterns.size();

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

                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                    auto patterns = this->m_type->createPatterns(evaluator);

                    for (auto &pattern : patterns) {
                        std::vector<u8> buffer(pattern->getSize());

                        if (evaluator->dataOffset() > evaluator->getDataSize() - buffer.size()) {
                            err::E0004.throwError("Array expanded past end of the data before a null-entry was found.", "Try using a while-sized array instead to limit the size of the array.", this);
                        }

                        const auto patternSize = pattern->getSize();
                        addEntries(hlp::moveToVector(std::move(pattern)));

                        auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                        if (ctrlFlow == ControlFlowStatement::None)
                            break;

                        evaluator->readData(evaluator->dataOffset() - patternSize, buffer.data(), buffer.size());
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


            if (auto &arrayEntries = arrayPattern->getEntries(); !arrayEntries.empty())
                arrayPattern->setTypeName(arrayEntries.front()->getTypeName());

            arrayPattern->setEntries(std::move(entries));
            arrayPattern->setSize(size);

            return arrayPattern;
        }
    };

}