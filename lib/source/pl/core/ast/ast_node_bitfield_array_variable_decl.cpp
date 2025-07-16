#include <pl/core/ast/ast_node_bitfield_array_variable_decl.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace pl::core::ast {

    ASTNodeBitfieldArrayVariableDecl::ASTNodeBitfieldArrayVariableDecl(std::string name, std::shared_ptr<ASTNodeTypeDecl> type, std::unique_ptr<ASTNode> &&size)
    : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_size(std::move(size)) { }

    ASTNodeBitfieldArrayVariableDecl::ASTNodeBitfieldArrayVariableDecl(const ASTNodeBitfieldArrayVariableDecl &other) : ASTNode(other), Attributable(other) {
        this->m_name = other.m_name;
        if (other.m_type->isForwardDeclared())
            this->m_type = other.m_type;
        else
            this->m_type = std::shared_ptr<ASTNodeTypeDecl>(static_cast<ASTNodeTypeDecl*>(other.m_type->clone().release()));

        if (other.m_size != nullptr)
            this->m_size = other.m_size->clone();
    }

    void ASTNodeBitfieldArrayVariableDecl::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto startOffset = evaluator->getBitwiseReadOffset();

        auto type = this->m_type->evaluate(evaluator);

        auto &pattern = resultPatterns.emplace_back();
        if (dynamic_cast<ASTNodeBitfield *>(type.get()) != nullptr
            || dynamic_cast<ASTNodeBitfieldField *>(type.get()) != nullptr) {
            createArray(evaluator, pattern);
        } else {
            err::E0001.throwError("Bitfield arrays may only contain bitwise fields.", { }, this->getLocation());
        }

        applyVariableAttributes(evaluator, this, pattern);

        if (evaluator->getSectionId() == ptrn::Pattern::PatternLocalSectionId) {
            evaluator->setBitwiseReadOffset(startOffset);
            this->execute(evaluator);
            resultPatterns.pop_back();
        }
    }

    void ASTNodeBitfieldArrayVariableDecl::createArray(Evaluator *evaluator, std::shared_ptr<ptrn::Pattern> &resultPattern) const {
        auto startArrayIndex = evaluator->getCurrentArrayIndex();
        ON_SCOPE_EXIT {
        if (startArrayIndex.has_value())
            evaluator->setCurrentArrayIndex(*startArrayIndex);
        else
            evaluator->clearCurrentArrayIndex();
        };

        auto position = evaluator->getBitwiseReadOffset();
        auto arrayPattern = std::make_shared<ptrn::PatternBitfieldArray>(evaluator, position.byteOffset, position.bitOffset, 0, getLocation().line);
        arrayPattern->setVariableName(this->m_name, this->getLocation());
        arrayPattern->setSection(evaluator->getSectionId());
        arrayPattern->setReversed(evaluator->isReadOrderReversed());

        std::vector<std::shared_ptr<ptrn::Pattern>> entries;

        size_t size = 0;
        u128 entryIndex = 0;

        auto addEntries = [&](std::vector<std::shared_ptr<ptrn::Pattern>> &&patterns) {
            for (auto &pattern : patterns) {
                pattern->setVariableName(fmt::format("[{}]", entryIndex), pattern->getVariableLocation());
                pattern->setEndian(arrayPattern->getEndian());
                if (pattern->getSection() == ptrn::Pattern::MainSectionId)
                    pattern->setSection(arrayPattern->getSection());

                size += pattern->getSize();
                entryIndex++;

                entries.push_back(std::move(pattern));

                evaluator->handleAbort();
            }
        };

        if (this->m_size == nullptr)
            err::E0001.throwError(fmt::format("Bitfield array was created with no size."), {}, this->getLocation());

        auto sizeNode = this->m_size->evaluate(evaluator);
        std::variant<u128, ASTNodeWhileStatement *> boundsCondition;

        if (auto literalNode = dynamic_cast<ASTNodeLiteral *>(sizeNode.get()); literalNode != nullptr) {
            boundsCondition = std::visit(wolv::util::overloaded {
                    [this](const std::string &) -> u128 { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this->getLocation()); },
                    [this](const std::shared_ptr<ptrn::Pattern> &pattern) -> u128 {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this->getLocation()); },
                    [](auto &&size) -> u128 { return u128(size); }
            }, literalNode->getValue());
        } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement *>(sizeNode.get()); whileStatement != nullptr) {
            boundsCondition = whileStatement;
        } else {
            err::E0001.throwError(fmt::format("Unexpected type of bitfield array size node."), {}, this->getLocation());
        }

        auto limit = evaluator->getArrayLimit();
        auto checkLimit = [&](auto count) {
            if (count > limit)
                err::E0007.throwError(fmt::format("Bitfield array grew past set limit of {}", limit), "If this is intended, try increasing the limit using '#pragma array_limit <new_limit>'.", this->getLocation());
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

        auto initialPosition = evaluator->getBitwiseReadOffset();

        ON_SCOPE_EXIT {
            auto endPosition = evaluator->getBitwiseReadOffset();
            auto startOffset = (initialPosition.byteOffset * 8) + initialPosition.bitOffset;
            auto endOffset = (endPosition.byteOffset * 8) + endPosition.bitOffset;

            if (startOffset < endOffset) {
                arrayPattern->setBitSize(endOffset - startOffset);
            } else {
                arrayPattern->setAbsoluteOffset(endPosition.byteOffset);
                arrayPattern->setBitOffset(endPosition.bitOffset);
                arrayPattern->setBitSize(startOffset - endOffset);
            }

            for (auto &pattern : entries) {
                if (auto bitfieldMember = dynamic_cast<ptrn::PatternBitfieldMember*>(pattern.get()); bitfieldMember != nullptr)
                    bitfieldMember->setParent(arrayPattern->reference());
            }

            arrayPattern->setEntries(entries);

            if (arrayPattern->getEntryCount() > 0)
                arrayPattern->setTypeName(arrayPattern->getEntry(0)->getTypeName());

            resultPattern = std::move(arrayPattern);
        };

        while (checkCondition()) {
            evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

            evaluator->setCurrentArrayIndex(u64(entryIndex));

            std::vector<std::shared_ptr<ptrn::Pattern>> patterns;
            this->m_type->createPatterns(evaluator, patterns);

            if (arrayPattern->getSection() == ptrn::Pattern::MainSectionId)
                if ((evaluator->getReadOffset() - evaluator->getDataBaseAddress()) > (evaluator->getDataSize() + 1))
                    err::E0004.throwError("Bitfield array expanded past end of the data.", fmt::format("Entry {} exceeded data by {} bytes.", dataIndex, evaluator->getReadOffset() - evaluator->getDataSize()), this->getLocation());

            auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
            evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);

            dataIndex++;

            if (ctrlFlow == ControlFlowStatement::Continue) {
                continue;
            }

            if (!patterns.empty())
                addEntries(std::move(patterns));

            if (ctrlFlow == ControlFlowStatement::Break || ctrlFlow == ControlFlowStatement::Return)
                break;
        }
    }

}