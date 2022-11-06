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
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_wide_string.hpp>
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
            if (dynamic_cast<ptrn::PatternUnsigned *>(pattern) != nullptr) {
                u128 value = 0;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<ptrn::PatternSigned *>(pattern) != nullptr) {
                i128 value = 0;
                readVariable(evaluator, value, pattern);
                value   = hlp::signExtend(pattern->getSize() * 8, value);
                literal = value;
            } else if (dynamic_cast<ptrn::PatternFloat *>(pattern) != nullptr) {
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
            } else if (dynamic_cast<ptrn::PatternCharacter *>(pattern) != nullptr) {
                char value = 0;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<ptrn::PatternBoolean *>(pattern) != nullptr) {
                bool value = false;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<ptrn::PatternString *>(pattern) != nullptr) {
                std::string value;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (auto bitfieldFieldPattern = dynamic_cast<ptrn::PatternBitfieldField *>(pattern); bitfieldFieldPattern != nullptr) {
                u64 value = 0;
                readVariable(evaluator, value, pattern);
                literal = u128(hlp::extract(bitfieldFieldPattern->getBitOffset() + (bitfieldFieldPattern->getBitSize() - 1), bitfieldFieldPattern->getBitOffset(), value));
            } else {
                literal = pattern;
            }

            if (auto& transformFunc = pattern->getTransformFunction(); transformFunc != nullptr) {
                auto result = transformFunc->func(evaluator, { std::move(literal) });

                if (!result.has_value())
                    err::E0009.throwError("Transform function did not return a value.", "Try adding a 'return <value>;' statement in all code paths.", this);
                literal = std::move(result.value());
            }

            return std::unique_ptr<ASTNode>(new ASTNodeLiteral(std::move(literal)));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            std::vector<std::shared_ptr<ptrn::Pattern>> searchScope;
            std::shared_ptr<ptrn::Pattern> currPattern;
            i32 scopeIndex = 0;

            if (!evaluator->isGlobalScope()) {
                const auto &globalScope = evaluator->getGlobalScope().scope;
                std::copy(globalScope->begin(), globalScope->end(), std::back_inserter(searchScope));
            }

            {
                const auto &currScope = evaluator->getScope(0);
                std::copy(currScope.scope->begin(), currScope.scope->end(), std::back_inserter(searchScope));
            }

            {
                const auto &templateParameters = evaluator->getTemplateParameters();
                std::copy(templateParameters.begin(), templateParameters.end(), std::back_inserter(searchScope));
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
                            currPattern = currParent;
                        }

                        continue;
                    } else if (name == "this") {
                        searchScope = *evaluator->getScope(scopeIndex).scope;

                        auto currParent = evaluator->getScope(0).parent;

                        if (currParent == nullptr)
                            err::E0003.throwError("Cannot use 'this' outside of nested type.", "Try using it inside of a struct, union or bitfield.", this);

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
                            [this](ptrn::Pattern *pattern) { err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this); },
                            [&, this](auto &&index) {
                                auto pattern = currPattern.get();
                                if (dynamic_cast<ptrn::PatternArrayDynamic *>(pattern) != nullptr) {
                                    if (static_cast<u128>(index) >= searchScope.size() || static_cast<i128>(index) < 0)
                                        err::E0006.throwError(fmt::format("Cannot access out of bounds index '{}'.", index), {}, this);

                                    currPattern = std::move(searchScope[index]);
                                } else if (auto staticArrayPattern = dynamic_cast<ptrn::PatternArrayStatic *>(pattern); staticArrayPattern != nullptr) {
                                    if (static_cast<u128>(index) >= staticArrayPattern->getEntryCount() || static_cast<i128>(index) < 0)
                                        err::E0006.throwError(fmt::format("Cannot access out of bounds index '{}'.", index), {}, this);

                                    auto newPattern = searchScope.front();
                                    newPattern->setOffset(staticArrayPattern->getOffset() + index * staticArrayPattern->getTemplate()->getSize());
                                    currPattern = std::move(newPattern);
                                } else if (auto stringPattern = dynamic_cast<ptrn::PatternString *>(pattern); stringPattern != nullptr) {
                                    if (static_cast<u128>(index) >= (stringPattern->getSize() / sizeof(char)) || static_cast<i128>(index) < 0)
                                        err::E0006.throwError(fmt::format("Cannot access out of bounds index '{}'.", index), {}, this);

                                    currPattern = std::make_unique<ptrn::PatternCharacter>(evaluator, stringPattern->getOffset() + index * sizeof(char));
                                } else if (auto wideStringPattern = dynamic_cast<ptrn::PatternWideString *>(pattern); wideStringPattern != nullptr) {
                                    if (static_cast<u128>(index) >= (wideStringPattern->getSize() / sizeof(char16_t)) || static_cast<i128>(index) < 0)
                                        err::E0006.throwError(fmt::format("Cannot access out of bounds index '{}'.", index), {}, this);

                                    currPattern = std::make_unique<ptrn::PatternWideCharacter>(evaluator, wideStringPattern->getOffset() + index * sizeof(char16_t));
                                } else {
                                    err::E0006.throwError(fmt::format("Cannot access non-array type '{}'.", pattern->getTypeName()), {}, this);
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

                if (auto structPattern = dynamic_cast<ptrn::PatternStruct *>(indexPattern))
                    searchScope = structPattern->getMembers();
                else if (auto unionPattern = dynamic_cast<ptrn::PatternUnion *>(indexPattern))
                    searchScope = unionPattern->getMembers();
                else if (auto bitfieldPattern = dynamic_cast<ptrn::PatternBitfield *>(indexPattern))
                    searchScope = bitfieldPattern->getFields();
                else if (auto dynamicArrayPattern = dynamic_cast<ptrn::PatternArrayDynamic *>(indexPattern))
                    searchScope = dynamicArrayPattern->getEntries();
                else if (auto staticArrayPattern = dynamic_cast<ptrn::PatternArrayStatic *>(indexPattern))
                    searchScope = { staticArrayPattern->getTemplate()->clone() };
            }

            if (currPattern == nullptr)
                err::E0003.throwError("Cannot reference global scope.", {}, this);
            else
                return hlp::moveToVector(std::move(currPattern));
        }

    private:
        Path m_path;

        void readVariable(Evaluator *evaluator, auto &value, ptrn::Pattern *variablePattern) const {
            constexpr bool isString = std::same_as<std::remove_cvref_t<decltype(value)>, std::string>;

            if constexpr (isString) {
                value.resize(variablePattern->getSize());
                evaluator->readData(variablePattern->getOffset(), value.data(), value.size(), variablePattern->getSection());
                value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
            } else {
                evaluator->readData(variablePattern->getOffset(), &value, variablePattern->getSize(), variablePattern->getSection());
            }

            if constexpr (!isString)
                value = hlp::changeEndianess(value, variablePattern->getSize(), variablePattern->getEndian());
        }
    };

}