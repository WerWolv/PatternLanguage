#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_parameter_pack.hpp>

#include <pl/patterns/pattern_pointer.hpp>
#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_enum.hpp>
#include <pl/patterns/pattern_bitfield.hpp>

namespace pl::core::ast {

    class ASTNodeRValue : public ASTNode {
    public:
        using PathSegment = std::variant<std::string, std::unique_ptr<ASTNode>>;
        using Path        = std::vector<PathSegment>;

        explicit ASTNodeRValue(Path &&path) : ASTNode(), m_path(std::move(path)) { }

        ASTNodeRValue(const ASTNodeRValue &other) : ASTNode(other) {
            for (auto &part : other.m_path) {
                if (auto stringPart = std::get_if<std::string>(&part); stringPart != nullptr)
                    this->m_path.push_back(*stringPart);
                else if (auto nodePart = std::get_if<std::unique_ptr<ASTNode>>(&part); nodePart != nullptr)
                    this->m_path.push_back((*nodePart)->clone());
            }
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeRValue(*this));
        }

        [[nodiscard]] const Path &getPath() const {
            return this->m_path;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            if (this->getPath().size() == 1) {
                if (auto name = std::get_if<std::string>(&this->getPath().front()); name != nullptr) {
                    if (*name == "$") return std::unique_ptr<ASTNode>(new ASTNodeLiteral(u128(evaluator->dataOffset())));

                    auto parameterPack = evaluator->getScope(0).parameterPack;
                    if (parameterPack && *name == parameterPack->name)
                        return std::unique_ptr<ASTNode>(new ASTNodeParameterPack(std::move(parameterPack->values)));
                }
            }

            ptrn::Pattern *pattern = nullptr;
            {
                auto referencedPattern = std::move(this->createPatterns(evaluator).front());

                pattern = referencedPattern.get();
                evaluator->getScope(0).savedPatterns.push_back(std::move(referencedPattern));
            }

            Token::Literal literal;
            if (dynamic_cast<ptrn::PatternUnsigned *>(pattern) || dynamic_cast<ptrn::PatternEnum *>(pattern)) {
                u128 value = 0;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<ptrn::PatternSigned *>(pattern)) {
                i128 value = 0;
                readVariable(evaluator, value, pattern);
                value   = hlp::signExtend(pattern->getSize() * 8, value);
                literal = value;
            } else if (dynamic_cast<ptrn::PatternFloat *>(pattern)) {
                if (pattern->getSize() == sizeof(u16)) {
                    u16 value = 0;
                    readVariable(evaluator, value, pattern);
                    literal = double(hlp::float16ToFloat32(value));
                } else if (pattern->getSize() == sizeof(float)) {
                    float value = 0;
                    readVariable(evaluator, value, pattern);
                    literal = double(value);
                } else if (pattern->getSize() == sizeof(double)) {
                    double value = 0;
                    readVariable(evaluator, value, pattern);
                    literal = value;
                } else
                    err::E0001.throwError("Invalid floating point type.");
            } else if (dynamic_cast<ptrn::PatternCharacter *>(pattern)) {
                char value = 0;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<ptrn::PatternBoolean *>(pattern)) {
                bool value = false;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<ptrn::PatternString *>(pattern)) {
                std::string value;

                if (pattern->getMemoryLocationType() == ptrn::PatternMemoryType::Stack) {
                    auto &variableValue = evaluator->getStack()[pattern->getOffset()];

                    std::visit(hlp::overloaded {
                           [&](char assignmentValue) { if (assignmentValue != 0x00) value = std::string({ assignmentValue }); },
                           [&](std::string &assignmentValue) { value = assignmentValue; },
                           [&, this](ptrn::Pattern *const &assignmentValue) {
                               if (!dynamic_cast<ptrn::PatternString *>(assignmentValue) && !dynamic_cast<ptrn::PatternCharacter *>(assignmentValue))
                                   err::E0004.throwError(fmt::format("Cannot assign value of type '{}' to variable of type 'string'.", pattern->getTypeName()), {}, this);

                               readVariable(evaluator, value, assignmentValue);
                           },
                           [&, this](auto &&) {
                               err::E0004.throwError(fmt::format("Cannot assign value of type '{}' to variable of type 'string'.", pattern->getTypeName()), {}, this);
                           }
                       }, variableValue);
                } else if (pattern->getMemoryLocationType() == ptrn::PatternMemoryType::Provider) {
                    value.resize(pattern->getSize());
                    evaluator->readData(pattern->getOffset(), value.data(), value.size());
                    value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
                } else if (pattern->getMemoryLocationType() == ptrn::PatternMemoryType::Heap) {
                    value.resize(pattern->getSize());
                    std::memcpy(value.data(), &evaluator->getHeap()[pattern->getOffset()], value.size());
                    value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
                }

                literal = value;
            } else if (auto bitfieldFieldPattern = dynamic_cast<ptrn::PatternBitfieldField *>(pattern)) {
                u64 value = 0;
                readVariable(evaluator, value, pattern);
                literal = u128(hlp::extract(bitfieldFieldPattern->getBitOffset() + (bitfieldFieldPattern->getBitSize() - 1), bitfieldFieldPattern->getBitOffset(), value));
            } else {
                literal = pattern;
            }

            if (auto transformFunc = pattern->getTransformFunction(); transformFunc.has_value()) {
                auto result = transformFunc->func(evaluator, { std::move(literal) });

                if (!result.has_value())
                    err::E0009.throwError("Transform function did not return a value.", "Try adding a 'return <value>;' statement in all code paths.", this);
                literal = std::move(result.value());
            }

            return std::unique_ptr<ASTNode>(new ASTNodeLiteral(std::move(literal)));
        }

        [[nodiscard]] std::vector<std::unique_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            std::vector<std::shared_ptr<ptrn::Pattern>> searchScope;
            std::unique_ptr<ptrn::Pattern> currPattern;
            i32 scopeIndex = 0;


            if (!evaluator->isGlobalScope()) {
                const auto &globalScope = evaluator->getGlobalScope().scope;
                std::copy(globalScope->begin(), globalScope->end(), std::back_inserter(searchScope));
            }

            {
                auto currScope = evaluator->getScope(scopeIndex).scope;
                std::copy(currScope->begin(), currScope->end(), std::back_inserter(searchScope));
            }

            for (const auto &part : this->getPath()) {

                if (part.index() == 0) {
                    // Variable access
                    auto name = std::get<std::string>(part);

                    if (name == "parent") {
                        scopeIndex--;

                        if (static_cast<size_t>(std::abs(scopeIndex)) >= evaluator->getScopeCount())
                            err::E0003.throwError("Cannot access parent of global scope.", {}, this);

                        searchScope     = *evaluator->getScope(scopeIndex).scope;
                        auto currParent = evaluator->getScope(scopeIndex).parent;

                        if (currParent == nullptr) {
                            currPattern = nullptr;
                        } else {
                            currPattern = currParent->clone();
                        }

                        continue;
                    } else if (name == "this") {
                        searchScope = *evaluator->getScope(scopeIndex).scope;

                        auto currParent = evaluator->getScope(0).parent;

                        if (currParent == nullptr)
                            err::E0003.throwError("Cannot use 'this' outside of nested type.", "Try using it inside of a struct, union or bitfield.", this);

                        currPattern = currParent->clone();
                        continue;
                    } else {
                        bool found = false;
                        for (auto iter = searchScope.crbegin(); iter != searchScope.crend(); ++iter) {
                            if ((*iter)->getVariableName() == name) {
                                currPattern = (*iter)->clone();
                                found       = true;
                                break;
                            }
                        }

                        if (name == "$")
                            err::E0003.throwError("Invalid use of '$' operator in rvalue.", {}, this);

                        if (!found) {
                            err::E0003.throwError(fmt::format("No variable named '{}' found.", name), {}, this);
                        }
                    }
                } else {
                    // Array indexing
                    auto node  = std::get<std::unique_ptr<ASTNode>>(part)->evaluate(evaluator);
                    auto index = dynamic_cast<ASTNodeLiteral *>(node.get());

                    std::visit(hlp::overloaded {
                        [this](const std::string &) { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this); },
                        [this](ptrn::Pattern *pattern) {err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this); },
                        [&, this](auto &&index) {
                            if (auto dynamicArrayPattern = dynamic_cast<ptrn::PatternArrayDynamic *>(currPattern.get())) {
                                if (static_cast<u128>(index) >= searchScope.size() || static_cast<i128>(index) < 0)
                                    err::E0006.throwError(fmt::format("Cannot out of bounds index '{}'.", index), {}, this);

                                currPattern = searchScope[index]->clone();
                            } else if (auto staticArrayPattern = dynamic_cast<ptrn::PatternArrayStatic *>(currPattern.get())) {
                                if (static_cast<u128>(index) >= staticArrayPattern->getEntryCount() || static_cast<i128>(index) < 0)
                                    err::E0006.throwError(fmt::format("Cannot out of bounds index '{}'.", index), {}, this);

                                auto newPattern = searchScope.front()->clone();
                                newPattern->setOffset(staticArrayPattern->getOffset() + index * staticArrayPattern->getTemplate()->getSize());
                                currPattern = std::move(newPattern);
                           }
                        }
                    }, index->getValue());
                }

                if (currPattern == nullptr)
                    break;

                if (auto pointerPattern = dynamic_cast<ptrn::PatternPointer *>(currPattern.get())) {
                    currPattern = pointerPattern->getPointedAtPattern()->clone();
                }

                ptrn::Pattern *indexPattern;
                if (currPattern->getMemoryLocationType() == ptrn::PatternMemoryType::Stack) {
                    auto stackLiteral = evaluator->getStack()[currPattern->getOffset()];
                    if (auto stackPattern = std::get_if<ptrn::Pattern *>(&stackLiteral); stackPattern != nullptr)
                        indexPattern = *stackPattern;
                    else
                        return hlp::moveToVector<std::unique_ptr<ptrn::Pattern>>(std::move(currPattern));
                } else {
                    indexPattern = currPattern.get();
                }

                if (auto structPattern = dynamic_cast<ptrn::PatternStruct *>(indexPattern))
                    searchScope = structPattern->getMembers();
                else if (auto unionPattern = dynamic_cast<ptrn::PatternUnion *>(indexPattern))
                    searchScope = unionPattern->getMembers();
                else if (auto bitfieldPattern = dynamic_cast<ptrn::PatternBitfield *>(indexPattern))
                    searchScope = bitfieldPattern->getFields();
                else if (auto dynamicArrayPattern = dynamic_cast<ptrn::PatternArrayDynamic *>(indexPattern))
                    searchScope = dynamicArrayPattern->getEntries();
                else if (auto staticArrayPattern = dynamic_cast<ptrn::PatternArrayStatic *>(indexPattern))
                    searchScope = { staticArrayPattern->getTemplate() };
            }

            if (currPattern == nullptr)
                err::E0003.throwError("Cannot reference global scope.", {}, this);

            return hlp::moveToVector<std::unique_ptr<ptrn::Pattern>>(std::move(currPattern));
        }

    private:
        Path m_path;

        void readVariable(Evaluator *evaluator, auto &value, ptrn::Pattern *variablePattern) const {
            constexpr bool isString = std::same_as<std::remove_cvref_t<decltype(value)>, std::string>;

            if (variablePattern->getMemoryLocationType() == ptrn::PatternMemoryType::Stack) {
                auto &literal = evaluator->getStack()[variablePattern->getOffset()];

                std::visit(hlp::overloaded {
                    [&](std::string &assignmentValue) {
                       if constexpr (isString) value = assignmentValue;
                    },
                    [&](ptrn::Pattern *assignmentValue) { readVariable(evaluator, value, assignmentValue); },
                    [&](auto &&assignmentValue) { value = assignmentValue; }
                }, literal);
            } else if (variablePattern->getMemoryLocationType() == ptrn::PatternMemoryType::Heap) {
                auto &heap = evaluator->getHeap();
                auto offset = variablePattern->getOffset();
                if constexpr (!isString) {
                    if (offset < heap.size())
                        std::memcpy(&value, &heap[offset], variablePattern->getSize());
                    else
                        value = 0;
                } else {
                    if (offset < heap.size()) {
                        value.resize(variablePattern->getSize());
                        std::memcpy(value.data(), &heap[offset], variablePattern->getSize());
                    }
                    else {
                        value = "";
                    }
                }

            } else {
                if constexpr (isString) {
                    value.resize(variablePattern->getSize());
                    evaluator->readData(variablePattern->getOffset(), value.data(), value.size());
                    value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
                } else {
                    evaluator->readData(variablePattern->getOffset(), &value, variablePattern->getSize());
                }
            }

            if constexpr (!isString)
                value = hlp::changeEndianess(value, variablePattern->getSize(), variablePattern->getEndian());
        }
    };

}