#include <pl/core/ast/ast_node_rvalue.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_parameter_pack.hpp>

#include <pl/patterns/pattern_pointer.hpp>
#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_wide_string.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_bitfield.hpp>
#include <pl/patterns/pattern_padding.hpp>

namespace pl::core::ast {

    ASTNodeRValue::ASTNodeRValue(Path &&path) : m_path(std::move(path)) { }

    ASTNodeRValue::ASTNodeRValue(const ASTNodeRValue &other) : ASTNode(other) {
        for (auto &part : other.m_path) {
            if (auto stringPart = std::get_if<std::string>(&part); stringPart != nullptr)
                this->m_path.emplace_back(*stringPart);
            else if (auto nodePart = std::get_if<std::unique_ptr<ASTNode>>(&part); nodePart != nullptr)
                this->m_path.emplace_back((*nodePart)->clone());
        }
    }

    static void readVariable(Evaluator *evaluator, auto &value, ptrn::Pattern *variablePattern) {
        constexpr bool isString = std::same_as<std::remove_cvref_t<decltype(value)>, std::string>;

        if constexpr (isString) {
            value.resize(variablePattern->getSize());
            evaluator->readData(variablePattern->getOffset(), value.data(), value.size(), variablePattern->getSection());
        } else {
            evaluator->readData(variablePattern->getOffset(), &value, variablePattern->getSize(), variablePattern->getSection());
        }

        if constexpr (!isString)
            value = hlp::changeEndianess(value, variablePattern->getSize(), variablePattern->getEndian());
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeRValue::evaluate(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        if (this->getPath().size() == 1) {
            if (auto name = std::get_if<std::string>(&this->getPath().front()); name != nullptr) {
                if (*name == "$") return std::make_unique<ASTNodeLiteral>(u128(evaluator->getReadOffset()));
                else if (*name == "null") return std::make_unique<ASTNodeLiteral>(
                    std::make_shared<ptrn::PatternPadding>(evaluator, 0, 0, getLocation().line));

                auto parameterPack = evaluator->getScope(0).parameterPack;
                if (parameterPack && *name == parameterPack->name)
                    return std::make_unique<ASTNodeParameterPack>(std::move(parameterPack->values));
            }
        } else if (this->getPath().size() == 2) {
            if (auto name = std::get_if<std::string>(this->getPath().data()); name != nullptr) {
                if (*name == "$") {
                    if (auto arraySegment = std::get_if<std::unique_ptr<ASTNode>>(&this->getPath()[1]); arraySegment != nullptr) {
                        auto offsetNode = (*arraySegment)->evaluate(evaluator);
                        auto offsetLiteral = dynamic_cast<ASTNodeLiteral*>(offsetNode.get());
                        if (offsetLiteral != nullptr) {
                            auto offset = u64(offsetLiteral->getValue().toUnsigned());

                            u8 byte = 0x00;
                            evaluator->readData(offset, &byte, 1, ptrn::Pattern::MainSectionId);
                            return std::make_unique<ASTNodeLiteral>(u128(byte));
                        }
                    }
                }
            }
        }

        std::shared_ptr<ptrn::Pattern> pattern;
        {
            std::vector<std::shared_ptr<ptrn::Pattern>> referencedPatterns;
            this->createPatterns(evaluator, referencedPatterns);

            pattern = std::move(referencedPatterns.front());
        }

        Token::Literal literal;
        if (dynamic_cast<ptrn::PatternUnsigned *>(pattern.get()) != nullptr) {
            u128 value = 0;
            readVariable(evaluator, value, pattern.get());
            literal = value;
        } else if (dynamic_cast<ptrn::PatternSigned *>(pattern.get()) != nullptr) {
            i128 value = 0;
            readVariable(evaluator, value, pattern.get());
            value   = hlp::signExtend(pattern->getSize() * 8, value);
            literal = value;
        } else if (dynamic_cast<ptrn::PatternFloat *>(pattern.get()) != nullptr) {
            if (pattern->getSize() == sizeof(u16)) {
                u16 value = 0;
                readVariable(evaluator, value, pattern.get());
                literal = double(hlp::float16ToFloat32(value));
            } else if (pattern->getSize() == sizeof(float)) {
                float value = 0;
                readVariable(evaluator, value, pattern.get());
                literal = double(value);
            } else if (pattern->getSize() == sizeof(double)) {
                double value = 0;
                readVariable(evaluator, value, pattern.get());
                literal = value;
            } else
                err::E0001.throwError("Invalid floating point type.");
        } else if (dynamic_cast<ptrn::PatternCharacter *>(pattern.get()) != nullptr) {
            char value = 0;
            readVariable(evaluator, value, pattern.get());
            literal = value;
        } else if (dynamic_cast<ptrn::PatternBoolean *>(pattern.get()) != nullptr) {
            bool value = false;
            readVariable(evaluator, value, pattern.get());
            literal = value;
        } else if (dynamic_cast<ptrn::PatternString *>(pattern.get()) != nullptr) {
            std::string value;
            readVariable(evaluator, value, pattern.get());
            literal = value;
        } else if (auto bitfieldFieldPatternBoolean = dynamic_cast<ptrn::PatternBitfieldFieldBoolean *>(pattern.get()); bitfieldFieldPatternBoolean != nullptr) {
            literal = bool(bitfieldFieldPatternBoolean->readValue());
        } else if (auto bitfieldFieldPatternSigned = dynamic_cast<ptrn::PatternBitfieldFieldSigned *>(pattern.get()); bitfieldFieldPatternSigned != nullptr) {
            literal = hlp::signExtend(bitfieldFieldPatternSigned->getBitSize(), i128(bitfieldFieldPatternSigned->readValue()));
        } else if (auto bitfieldFieldPatternEnum = dynamic_cast<ptrn::PatternBitfieldFieldEnum *>(pattern.get()); bitfieldFieldPatternEnum != nullptr) {
            literal = pattern;
        } else if (auto bitfieldFieldPattern = dynamic_cast<ptrn::PatternBitfieldField *>(pattern.get()); bitfieldFieldPattern != nullptr) {
            literal = bitfieldFieldPattern->readValue();
        } else {
            literal = pattern;
        }

        if (auto transformFunc = evaluator->findFunction(pattern->getTransformFunction()); transformFunc.has_value()) {
            auto oldPatternName = pattern->getVariableName();
            auto result = transformFunc->func(evaluator, { std::move(literal) });
            pattern->setVariableName(oldPatternName, pattern->getVariableLocation());

            if (!result.has_value())
                err::E0009.throwError("Transform function did not return a value.", "Try adding a 'return <value>;' statement in all code paths.", this->getLocation());
            literal = std::move(result.value());
        }

        return std::unique_ptr<ASTNode>(new ASTNodeLiteral(std::move(literal)));
    }

    void ASTNodeRValue::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        std::vector<std::shared_ptr<ptrn::Pattern>> searchScope;
        std::shared_ptr<ptrn::Pattern> currPattern;
        i32 scopeIndex = 0;
        bool iterable = true;

        if (!evaluator->isGlobalScope()) {
            const auto &globalScope = evaluator->getGlobalScope().scope;
            std::copy(globalScope->begin(), globalScope->end(), std::back_inserter(searchScope));
        }

        {
            const auto &templateParameters = evaluator->getTemplateParameters();
            std::copy(templateParameters.begin(), templateParameters.end(), std::back_inserter(searchScope));
        }

        {
            const auto &currScope = evaluator->getScope(0);
            std::copy(currScope.scope->begin(), currScope.scope->end(), std::back_inserter(searchScope));
        }

        for (const auto &part : this->getPath()) {

            if (!iterable)
                err::E0001.throwError("Member access of a non-iterable type.", "Try using a struct-like object or an array instead.", this->getLocation());

            if (part.index() == 0) {
                // Variable access
                auto name = std::get<std::string>(part);

                if (name == "parent") {
                    do {
                        scopeIndex--;

                        if (static_cast<size_t>(std::abs(scopeIndex)) >= evaluator->getScopeCount())
                            err::E0003.throwError("Cannot access parent of global scope.", {}, this->getLocation());
                    } while (evaluator->getScope(scopeIndex).parent == nullptr);

                    searchScope     = *evaluator->getScope(scopeIndex).scope;
                    auto currParent = evaluator->getScope(scopeIndex).parent;

                    if (currParent == nullptr) {
                        currPattern = nullptr;
                    } else {
                        currPattern = currParent;
                    }

                    continue;
                } else if (name == "this") {
                    searchScope = *evaluator->getScope(scopeIndex).scope;

                    auto currParent = evaluator->getScope(0).parent;

                    if (currParent == nullptr)
                        err::E0003.throwError("Cannot use 'this' outside of nested type.", "Try using it inside of a struct, union or bitfield.", this->getLocation());

                    currPattern = currParent;
                    continue;
                } else {
                    bool found = false;
                    for (auto iter = searchScope.crbegin(); iter != searchScope.crend(); ++iter) {
                        if ((*iter)->getVariableName() == name) {
                            currPattern = *iter;
                            found       = true;
                            break;
                        }
                    }

                    if (name == "$")
                        err::E0003.throwError("Invalid use of '$' operator in rvalue.", {}, this->getLocation());
                    else if (name == "null")
                        err::E0003.throwError("Invalid use of 'null' keyword in rvalue.", {}, this->getLocation());

                    if (!found)
                        err::E0003.throwError(fmt::format("No variable named '{}' found.", name), {}, this->getLocation());
                }
            } else {
                // Array indexing
                auto node  = std::get<std::unique_ptr<ASTNode>>(part)->evaluate(evaluator);
                auto index = dynamic_cast<ASTNodeLiteral *>(node.get());
                if (index == nullptr)
                    err::E0010.throwError("Cannot use void expression as array index.", {}, this->getLocation());

                std::visit(wolv::util::overloaded {
                                   [this](const std::string &) { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this->getLocation()); },
                                   [this](const std::shared_ptr<ptrn::Pattern> &pattern) { err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this->getLocation()); },
                                   [&, this](auto &&index) {
                                       auto pattern = currPattern.get();
                                       if (auto indexablePattern = dynamic_cast<ptrn::IIndexable *>(pattern); indexablePattern != nullptr) {
                                           if (size_t(index) >= indexablePattern->getEntryCount())
                                               core::err::E0006.throwError("Index out of bounds.", fmt::format("Tried to access index {} in array of size {}.", size_t(index), indexablePattern->getEntryCount()), this->getLocation());
                                           currPattern = indexablePattern->getEntry(size_t(index));
                                       } else {
                                           err::E0006.throwError(fmt::format("Cannot access non-array type '{}'.", pattern->getTypeName()), {}, this->getLocation());
                                       }
                                   }
                           },
                           index->getValue()
                );
            }

            if (currPattern == nullptr)
                break;


            if (auto pointerPattern = dynamic_cast<ptrn::PatternPointer *>(currPattern.get()))
                currPattern = pointerPattern->getPointedAtPattern();

            auto indexPattern = currPattern.get();

            if (auto iterablePattern = dynamic_cast<ptrn::IIterable *>(indexPattern); iterablePattern != nullptr)
                searchScope = iterablePattern->getEntries();
            else
                iterable = false;

        }

        if (currPattern == nullptr)
            err::E0003.throwError("Cannot reference global scope.", {}, this->getLocation());
        else
            resultPatterns = hlp::moveToVector(std::move(currPattern));
    }

}