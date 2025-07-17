#include <pl/core/ast/ast_node_array_variable_decl.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_while_statement.hpp>

#include <pl/patterns/pattern_padding.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_wide_string.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>

namespace pl::core::ast {

    ASTNodeArrayVariableDecl::ASTNodeArrayVariableDecl(std::string name, std::shared_ptr<ASTNodeTypeDecl> type, std::unique_ptr<ASTNode> &&size, std::unique_ptr<ASTNode> &&placementOffset, std::unique_ptr<ASTNode> &&placementSection, bool constant)
        :m_name(std::move(name)), m_type(std::move(type)), m_size(std::move(size)), m_placementOffset(std::move(placementOffset)), m_placementSection(std::move(placementSection)), m_constant(constant) {
        }

    ASTNodeArrayVariableDecl::ASTNodeArrayVariableDecl(const ASTNodeArrayVariableDecl &other) : ASTNode(other), Attributable(other) {
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

        this->m_constant = other.m_constant;
    }

    void ASTNodeArrayVariableDecl::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto startOffset = evaluator->getBitwiseReadOffset();

        auto scopeGuard = SCOPE_GUARD {
            evaluator->popSectionId();
        };

        if (this->m_placementSection != nullptr) {
            const auto node = this->m_placementSection->evaluate(evaluator);
            const auto id = dynamic_cast<ASTNodeLiteral *>(node.get());
            if (id == nullptr)
                err::E0010.throwError("Cannot use void expression as section identifier.", {}, this->getLocation());

            evaluator->pushSectionId(u64(id->getValue().toUnsigned()));
        } else {
            scopeGuard.release();
        }

        if (this->m_placementOffset != nullptr) {
            auto evaluatedPlacement = this->m_placementOffset->evaluate(evaluator);
            auto offset             = dynamic_cast<ASTNodeLiteral *>(evaluatedPlacement.get());
            if (offset == nullptr)
                err::E0010.throwError("Cannot use void expression as placement offset.", {}, this->getLocation());

            evaluator->setReadOffset(std::visit(wolv::util::overloaded {
                    [this](const std::string &) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this->getLocation()); },
                    [this](const std::shared_ptr<ptrn::Pattern>&) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this->getLocation()); },
                    [](auto &&offset) -> u64 { return u64(offset); }
            }, offset->getValue()));
        }

        if (evaluator->getSectionId() == ptrn::Pattern::PatternLocalSectionId || evaluator->getSectionId() == ptrn::Pattern::HeapSectionId) {
            evaluator->setBitwiseReadOffset(startOffset);
            this->execute(evaluator);
        } else {
            auto type = this->m_type->evaluate(evaluator);

            auto &pattern = resultPatterns.emplace_back();
            if (auto builtinType = dynamic_cast<ASTNodeBuiltinType *>(type.get()); builtinType != nullptr && builtinType->getType() != Token::ValueType::CustomType)
                createStaticArray(evaluator, pattern);
            else {
                bool isStaticType = false;
                if (auto attributable = dynamic_cast<Attributable *>(type.get()))
                    isStaticType = attributable->hasAttribute("static", false);

                if (isStaticType)
                    createStaticArray(evaluator, pattern);
                else
                    createDynamicArray(evaluator, pattern);
            }

            pattern->setSection(evaluator->getSectionId());

            applyVariableAttributes(evaluator, this, pattern);

            if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
                evaluator->setBitwiseReadOffset(startOffset);
            }

            if (this->m_placementSection != nullptr && !evaluator->isGlobalScope()) {
                evaluator->addPattern(std::move(pattern));
                resultPatterns.pop_back();
            }
        }
    }

    ASTNode::FunctionResult ASTNodeArrayVariableDecl::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        if (this->m_size == nullptr)
            err::E0004.throwError("Function arrays cannot be unsized.", {}, this->getLocation());

        auto sizeNode = this->m_size->evaluate(evaluator);
        auto sizeLiteral = dynamic_cast<ASTNodeLiteral*>(sizeNode.get());
        if (sizeLiteral == nullptr)
            err::E0004.throwError("Function arrays require a fixed size.", {}, this->getLocation());

        auto entryCount = std::visit(wolv::util::overloaded {
                [this](const std::string &) -> i128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this->getLocation()); },
                [this](const std::shared_ptr<ptrn::Pattern> &pattern) -> i128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this->getLocation()); },
                [](auto &&size) -> i128 { return i128(size); }
        }, sizeLiteral->getValue());

        u64 section = 0;
        if (this->m_placementSection != nullptr) {
            const auto sectionNode = this->m_placementSection->evaluate(evaluator);
            const auto sectionLiteral = dynamic_cast<ASTNodeLiteral *>(sectionNode.get());
            if (sectionLiteral == nullptr)
                err::E0002.throwError("Cannot use void expression as section identifier.", {}, this->getLocation());

            section = u64(sectionLiteral->getValue().toUnsigned());
        } else {
            section = evaluator->getSectionId();
        }

        evaluator->createArrayVariable(this->m_name, this->m_type.get(), size_t(entryCount), section, this->m_constant);

        if (this->m_placementOffset != nullptr) {
            const auto placementNode = this->m_placementOffset->evaluate(evaluator);
            const auto offsetLiteral = dynamic_cast<ASTNodeLiteral *>(placementNode.get());
            if (offsetLiteral == nullptr)
                err::E0002.throwError("Void expression used in placement expression.", { }, this->getLocation());

            evaluator->setVariableAddress(this->getName(), u64(offsetLiteral->getValue().toUnsigned()), section);
        }

        return std::nullopt;
    }


    void ASTNodeArrayVariableDecl::createStaticArray(Evaluator *evaluator, std::shared_ptr<ptrn::Pattern> &outputPattern) const {
        evaluator->alignToByte();
        auto startOffset = evaluator->getReadOffset();

        std::vector<std::shared_ptr<ptrn::Pattern>> templatePatterns;
        this->m_type->createPatterns(evaluator, templatePatterns);
        if (templatePatterns.empty())
            err::E0005.throwError("'auto' can only be used with parameters.", { }, this->getLocation());

        auto &templatePattern = templatePatterns.front();

        templatePattern->setSection(evaluator->getSectionId());

        evaluator->setReadOffset(startOffset);

        i128 entryCount = 0;

        if (this->m_size != nullptr) {
            auto sizeNode = this->m_size->evaluate(evaluator);

            if (auto literal = dynamic_cast<ASTNodeLiteral *>(sizeNode.get()); literal != nullptr) {
                entryCount = std::visit(wolv::util::overloaded {
                        [this](const std::string &) -> i128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this->getLocation()); },
                        [this](const std::shared_ptr<ptrn::Pattern> &pattern) -> i128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this->getLocation()); },
                        [](auto &&size) -> i128 { return i128(size); }
                }, literal->getValue());
            } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement *>(sizeNode.get())) {
                while (whileStatement->evaluateCondition(evaluator)) {
                    if (templatePattern->getSection() == ptrn::Pattern::MainSectionId)
                        if ((evaluator->getReadOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                            err::E0004.throwError("Array expanded past end of the data before termination condition was met.", { }, this->getLocation());

                    evaluator->handleAbort();
                    entryCount++;
                    evaluator->getReadOffsetAndIncrement(templatePattern->getSize());
                }
            }

            if (entryCount < 0)
                err::E0004.throwError("Array size cannot be negative.", { }, this->getLocation());
        } else {
            std::vector<u8> buffer(templatePattern->getSize());
            while (true) {
                if (templatePattern->getSection() == ptrn::Pattern::MainSectionId)
                    if ((evaluator->getReadOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                        err::E0004.throwError("Array expanded past end of the data before a null-entry was found.", "Try using a while-sized array instead to limit the size of the array.", this->getLocation());

                evaluator->readData(evaluator->getReadOffset(), buffer.data(), buffer.size(), templatePattern->getSection());
                evaluator->getReadOffsetAndIncrement(buffer.size());

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

        if (dynamic_cast<ptrn::PatternPadding *>(templatePattern.get())) {
            outputPattern = std::make_unique<ptrn::PatternPadding>(evaluator, startOffset, 0, getLocation().line);
        } else if (dynamic_cast<ptrn::PatternCharacter *>(templatePattern.get())) {
            outputPattern = std::make_unique<ptrn::PatternString>(evaluator, startOffset, 0, getLocation().line);
        } else if (dynamic_cast<ptrn::PatternWideCharacter *>(templatePattern.get())) {
            outputPattern = std::make_unique<ptrn::PatternWideString>(evaluator, startOffset, 0, getLocation().line);
        } else {
            auto arrayPattern = std::make_unique<ptrn::PatternArrayStatic>(evaluator, startOffset, 0, getLocation().line);
            arrayPattern->setEntries(templatePattern->clone(), size_t(entryCount));
            arrayPattern->setSection(templatePattern->getSection());
            outputPattern = std::move(arrayPattern);
        }

        outputPattern->setVariableName(this->m_name, this->getLocation());
        if (templatePattern->hasOverriddenEndian())
            outputPattern->setEndian(templatePattern->getEndian());
        outputPattern->setTypeName(templatePattern->getTypeName());
        outputPattern->setSize(size_t(templatePattern->getSize() * entryCount));
        if (evaluator->isReadOrderReversed())
            outputPattern->setAbsoluteOffset(evaluator->getReadOffset());
        outputPattern->setSection(templatePattern->getSection());

        evaluator->setReadOffset(startOffset + outputPattern->getSize());

        if (outputPattern->getSection() == ptrn::Pattern::MainSectionId)
            if ((evaluator->getReadOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                err::E0004.throwError("Array expanded past end of the data.", { }, this->getLocation());
    }

    void ASTNodeArrayVariableDecl::createDynamicArray(Evaluator *evaluator, std::shared_ptr<ptrn::Pattern> &resultPattern) const {
        auto startArrayIndex = evaluator->getCurrentArrayIndex();
        ON_SCOPE_EXIT {
        if (startArrayIndex.has_value())
            evaluator->setCurrentArrayIndex(*startArrayIndex);
        else
            evaluator->clearCurrentArrayIndex();
        };

        evaluator->alignToByte();
        auto arrayPattern = std::make_unique<ptrn::PatternArrayDynamic>(evaluator, evaluator->getReadOffset(), 0, getLocation().line);
        arrayPattern->setVariableName(this->m_name, this->getLocation());
        arrayPattern->setSection(evaluator->getSectionId());

        std::vector<std::shared_ptr<ptrn::Pattern>> entries;

        size_t size    = 0;
        u64 entryIndex = 0;

        ON_SCOPE_EXIT {
            if (arrayPattern->getEntryCount() > 0)
                arrayPattern->setTypeName(arrayPattern->getEntry(0)->getTypeName());

            arrayPattern->setEntries(entries);
            arrayPattern->setSize(size);

            resultPattern = std::move(arrayPattern);
        };

        auto addEntries = [&](std::vector<std::shared_ptr<ptrn::Pattern>> &&patterns) {
            for (auto &pattern : patterns) {
                pattern->setArrayIndex(entryIndex);
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
                        [this](const std::string &) -> u128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this->getLocation()); },
                        [this](const std::shared_ptr<ptrn::Pattern> &pattern) -> u128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this->getLocation()); },
                        [](auto &&size) -> u128 { return u128(size); }
                }, literal->getValue());

                auto limit = evaluator->getArrayLimit();
                if (entryCount > limit)
                    err::E0007.throwError(fmt::format("Array grew past set limit of {}", limit), "If this is intended, try increasing the limit using '#pragma array_limit <new_limit>'.", this->getLocation());

                for (u64 i = 0; i < entryCount; i++) {
                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                    evaluator->setCurrentArrayIndex(i);

                    std::vector<std::shared_ptr<ptrn::Pattern>> patterns;
                    this->m_type->createPatterns(evaluator, patterns);
                    size_t patternCount = patterns.size();

                    if (arrayPattern->getSection() == ptrn::Pattern::MainSectionId)
                        if ((evaluator->getReadOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                            err::E0004.throwError("Array expanded past end of the data.", fmt::format("Entry {} exceeded data by {} bytes.", i, evaluator->getReadOffset() - evaluator->getDataSize()), this->getLocation());

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
                        err::E0007.throwError(fmt::format("Array grew past set limit of {}", limit), "If this is intended, try increasing the limit using '#pragma array_limit <new_limit>'.", this->getLocation());

                    evaluator->setCurrentArrayIndex(entryIndex);

                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                    std::vector<std::shared_ptr<ptrn::Pattern>> patterns;
                    this->m_type->createPatterns(evaluator, patterns);
                    size_t patternCount = patterns.size();

                    if (arrayPattern->getSection() == ptrn::Pattern::MainSectionId)
                        if ((evaluator->getReadOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                            err::E0004.throwError("Array expanded past end of the data before termination condition was met.", { }, this->getLocation());

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
                    err::E0007.throwError(fmt::format("Array grew past set limit of {}", limit), "If this is intended, try increasing the limit using '#pragma array_limit <new_limit>'.", this->getLocation());

                evaluator->setCurrentArrayIndex(entryIndex);

                evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

                std::vector<std::shared_ptr<ptrn::Pattern>> patterns;
                this->m_type->createPatterns(evaluator, patterns);

                for (auto &pattern : patterns) {
                    std::vector<u8> buffer(pattern->getSize());

                    if (arrayPattern->getSection() == ptrn::Pattern::MainSectionId)
                        if ((evaluator->getReadOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                            err::E0004.throwError("Array expanded past end of the data before a null-entry was found.", "Try using a while-sized array instead to limit the size of the array.", this->getLocation());

                    const auto patternSize = pattern->getSize();
                    evaluator->readData(evaluator->getReadOffset() - patternSize, buffer.data(), buffer.size(), pattern->getSection());

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
    }
}