#pragma once

#include <pl/ast/ast_node.hpp>
#include <pl/ast/ast_node_literal.hpp>
#include <pl/ast/ast_node_parameter_pack.hpp>

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

namespace pl {

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

            Pattern *pattern = nullptr;
            {
                auto referencedPattern = std::move(this->createPatterns(evaluator).front());

                pattern = referencedPattern.get();
                evaluator->getScope(0).savedPatterns.push_back(std::move(referencedPattern));
            }

            Token::Literal literal;
            if (dynamic_cast<PatternUnsigned *>(pattern)) {
                u128 value = 0;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<PatternSigned *>(pattern)) {
                i128 value = 0;
                readVariable(evaluator, value, pattern);
                value   = pl::signExtend(pattern->getSize() * 8, value);
                literal = value;
            } else if (dynamic_cast<PatternFloat *>(pattern)) {
                if (pattern->getSize() == sizeof(u16)) {
                    u16 value = 0;
                    readVariable(evaluator, value, pattern);
                    literal = double(pl::float16ToFloat32(value));
                } else if (pattern->getSize() == sizeof(float)) {
                    float value = 0;
                    readVariable(evaluator, value, pattern);
                    literal = double(value);
                } else if (pattern->getSize() == sizeof(double)) {
                    double value = 0;
                    readVariable(evaluator, value, pattern);
                    literal = value;
                } else LogConsole::abortEvaluation("invalid floating point type access", this);
            } else if (dynamic_cast<PatternCharacter *>(pattern)) {
                char value = 0;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<PatternBoolean *>(pattern)) {
                bool value = false;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<PatternString *>(pattern)) {
                std::string value;

                if (pattern->getMemoryLocationType() == PatternMemoryType::Stack) {
                    auto &variableValue = evaluator->getStack()[pattern->getOffset()];

                    std::visit(overloaded {
                           [&](char assignmentValue) { if (assignmentValue != 0x00) value = std::string({ assignmentValue }); },
                           [&](std::string &assignmentValue) { value = assignmentValue; },
                           [&, this](Pattern *const &assignmentValue) {
                               if (!dynamic_cast<PatternString *>(assignmentValue) && !dynamic_cast<PatternCharacter *>(assignmentValue))
                                   LogConsole::abortEvaluation(fmt::format("cannot assign '{}' to string", pattern->getTypeName()), this);

                               readVariable(evaluator, value, assignmentValue);
                           },
                           [&, this](auto &&) { LogConsole::abortEvaluation(fmt::format("cannot assign '{}' to string", pattern->getTypeName()), this); }
                       }, variableValue);
                } else if (pattern->getMemoryLocationType() == PatternMemoryType::Provider) {
                    value.resize(pattern->getSize());
                    evaluator->readData(pattern->getOffset(), value.data(), value.size());
                    value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
                } else if (pattern->getMemoryLocationType() == PatternMemoryType::Heap) {
                    value.resize(pattern->getSize());
                    std::memcpy(value.data(), evaluator->getHeap().data(), value.size());
                    value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
                }

                literal = value;
            } else if (auto bitfieldFieldPattern = dynamic_cast<PatternBitfieldField *>(pattern)) {
                u64 value = 0;
                readVariable(evaluator, value, pattern);
                literal = u128(pl::extract(bitfieldFieldPattern->getBitOffset() + (bitfieldFieldPattern->getBitSize() - 1), bitfieldFieldPattern->getBitOffset(), value));
            } else {
                literal = pattern;
            }

            if (auto transformFunc = pattern->getTransformFunction(); transformFunc.has_value()) {
                auto result = transformFunc->func(evaluator, { std::move(literal) });

                if (!result.has_value())
                    LogConsole::abortEvaluation("transform function did not return a value", this);
                literal = std::move(result.value());
            }

            return std::unique_ptr<ASTNode>(new ASTNodeLiteral(std::move(literal)));
        }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {
            std::vector<std::shared_ptr<Pattern>> searchScope;
            std::unique_ptr<Pattern> currPattern;
            i32 scopeIndex = 0;


            if (!evaluator->isGlobalScope()) {
                auto globalScope = evaluator->getGlobalScope().scope;
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
                            LogConsole::abortEvaluation("cannot access parent of global scope", this);

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
                            LogConsole::abortEvaluation("invalid use of 'this' outside of struct-like type", this);

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
                            LogConsole::abortEvaluation("invalid use of placeholder operator in rvalue");

                        if (!found) {
                            LogConsole::abortEvaluation(fmt::format("no variable named '{}' found", name), this);
                        }
                    }
                } else {
                    // Array indexing
                    auto node  = std::get<std::unique_ptr<ASTNode>>(part)->evaluate(evaluator);
                    auto index = dynamic_cast<ASTNodeLiteral *>(node.get());

                    std::visit(overloaded {
                        [this](const std::string &) { LogConsole::abortEvaluation("cannot use string to index array", this); },
                        [this](Pattern *) { LogConsole::abortEvaluation("cannot use custom type to index array", this); },
                        [&, this](auto &&index) {
                           if (auto dynamicArrayPattern = dynamic_cast<PatternArrayDynamic *>(currPattern.get())) {
                               if (static_cast<u128>(index) >= searchScope.size() || static_cast<i128>(index) < 0)
                                   LogConsole::abortEvaluation("array index out of bounds", this);

                               currPattern = searchScope[index]->clone();
                           } else if (auto staticArrayPattern = dynamic_cast<PatternArrayStatic *>(currPattern.get())) {
                               if (static_cast<u128>(index) >= staticArrayPattern->getEntryCount() || static_cast<i128>(index) < 0)
                                   LogConsole::abortEvaluation("array index out of bounds", this);

                               auto newPattern = searchScope.front()->clone();
                               newPattern->setOffset(staticArrayPattern->getOffset() + index * staticArrayPattern->getTemplate()->getSize());
                               currPattern = std::move(newPattern);
                           }
                        }
                    }, index->getValue());
                }

                if (currPattern == nullptr)
                    break;

                if (auto pointerPattern = dynamic_cast<PatternPointer *>(currPattern.get())) {
                    currPattern = pointerPattern->getPointedAtPattern()->clone();
                }

                Pattern *indexPattern;
                if (currPattern->getMemoryLocationType() == PatternMemoryType::Stack) {
                    auto stackLiteral = evaluator->getStack()[currPattern->getOffset()];
                    if (auto stackPattern = std::get_if<Pattern *>(&stackLiteral); stackPattern != nullptr)
                        indexPattern = *stackPattern;
                    else
                        return pl::moveToVector<std::unique_ptr<Pattern>>(std::move(currPattern));
                } else {
                    indexPattern = currPattern.get();
                }

                if (auto structPattern = dynamic_cast<PatternStruct *>(indexPattern))
                    searchScope = structPattern->getMembers();
                else if (auto unionPattern = dynamic_cast<PatternUnion *>(indexPattern))
                    searchScope = unionPattern->getMembers();
                else if (auto bitfieldPattern = dynamic_cast<PatternBitfield *>(indexPattern))
                    searchScope = bitfieldPattern->getFields();
                else if (auto dynamicArrayPattern = dynamic_cast<PatternArrayDynamic *>(indexPattern))
                    searchScope = dynamicArrayPattern->getEntries();
                else if (auto staticArrayPattern = dynamic_cast<PatternArrayStatic *>(indexPattern))
                    searchScope = { staticArrayPattern->getTemplate() };
            }

            if (currPattern == nullptr)
                LogConsole::abortEvaluation("cannot reference global scope", this);

            return pl::moveToVector<std::unique_ptr<Pattern>>(std::move(currPattern));
        }

    private:
        Path m_path;

        void readVariable(Evaluator *evaluator, auto &value, Pattern *variablePattern) const {
            constexpr bool isString = std::same_as<std::remove_cvref_t<decltype(value)>, std::string>;

            if (variablePattern->getMemoryLocationType() == PatternMemoryType::Stack) {
                auto &literal = evaluator->getStack()[variablePattern->getOffset()];

                std::visit(overloaded {
                    [&](std::string &assignmentValue) {
                       if constexpr (isString) value = assignmentValue;
                    },
                    [&](Pattern *assignmentValue) { readVariable(evaluator, value, assignmentValue); },
                    [&](auto &&assignmentValue) { value = assignmentValue; }
                }, literal);
            } else if (variablePattern->getMemoryLocationType() == PatternMemoryType::Heap) {
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
                value = pl::changeEndianess(value, variablePattern->getSize(), variablePattern->getEndian());
        }
    };

}