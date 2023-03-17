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
#include <pl/patterns/pattern_padding.hpp>

namespace pl::core::ast {

    class ASTNodeRValue : public ASTNode {
    public:
        using PathSegment = std::variant<std::string, std::unique_ptr<ASTNode>>;
        using Path        = std::vector<PathSegment>;

        explicit ASTNodeRValue(Path &&path) : ASTNode(), m_path(std::move(path)) { }

        ASTNodeRValue(const ASTNodeRValue &other) : ASTNode(other) {
            for (auto &part : other.m_path) {
                if (auto stringPart = std::get_if<std::string>(&part); stringPart != nullptr)
                    this->m_path.emplace_back(*stringPart);
                else if (auto nodePart = std::get_if<std::unique_ptr<ASTNode>>(&part); nodePart != nullptr)
                    this->m_path.emplace_back((*nodePart)->clone());
            }
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeRValue(*this));
        }

        [[nodiscard]] const Path &getPath() const {
            return this->m_path;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            if (this->getPath().size() == 1) {
                if (auto name = std::get_if<std::string>(&this->getPath().front()); name != nullptr) {
                    if (*name == "$") return std::unique_ptr<ASTNode>(new ASTNodeLiteral(u128(evaluator->dataOffset())));
                    else if (*name == "null") return std::unique_ptr<ASTNode>(new ASTNodeLiteral(new ptrn::PatternPadding(evaluator, 0, 0)));

                    auto parameterPack = evaluator->getScope(0).parameterPack;
                    if (parameterPack && *name == parameterPack->name)
                        return std::unique_ptr<ASTNode>(new ASTNodeParameterPack(std::move(parameterPack->values)));
                }
            } else if (this->getPath().size() == 2) {
                if (auto name = std::get_if<std::string>(&this->getPath()[0]); name != nullptr) {
                    if (*name == "$") {
                        if (auto arraySegment = std::get_if<std::unique_ptr<ASTNode>>(&this->getPath()[1]); arraySegment != nullptr) {
                            auto offsetNode = (*arraySegment)->evaluate(evaluator);
                            auto offsetLiteral = dynamic_cast<ASTNodeLiteral*>(offsetNode.get());
                            if (offsetLiteral != nullptr) {
                                auto offset = offsetLiteral->getValue().toUnsigned();

                                u8 byte = 0x00;
                                evaluator->readData(offset, &byte, 1, ptrn::Pattern::MainSectionId);
                                return std::unique_ptr<ASTNode>(new ASTNodeLiteral(u128(byte)));
                            }
                        }
                    }
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
                literal = u128(bitfieldFieldPattern->readValue());
            } else {
                literal = pattern;
            }

            if (auto transformFunc = evaluator->findFunction(pattern->getTransformFunction()); transformFunc.has_value()) {
                auto result = transformFunc->func(evaluator, { std::move(literal) });

                if (!result.has_value())
                    err::E0009.throwError("Transform function did not return a value.", "Try adding a 'return <value>;' statement in all code paths.", this);
                literal = std::move(result.value());
            }

            return std::unique_ptr<ASTNode>(new ASTNodeLiteral(std::move(literal)));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            std::vector<std::shared_ptr<ptrn::Pattern>> searchScope;
            std::shared_ptr<ptrn::Pattern> currPattern;
            i32 scopeIndex = 0;
            bool indexable = true;

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

                if (!indexable)
                    err::E0001.throwError("Member access of a non-iterable type.", "Try using a struct-like object or an array instead.", this);

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
                        else if (name == "null")
                            err::E0003.throwError("Invalid use of 'null' keyword in rvalue.", {}, this);

                        if (!found)
                            err::E0003.throwError(fmt::format("No variable named '{}' found.", name), {}, this);
                    }
                } else {
                    // Array indexing
                    auto node  = std::get<std::unique_ptr<ASTNode>>(part)->evaluate(evaluator);
                    auto index = dynamic_cast<ASTNodeLiteral *>(node.get());
                    if (index == nullptr)
                        err::E0010.throwError("Cannot use void expression as array index.", {}, this);

                    std::visit(wolv::util::overloaded {
                            [this](const std::string &) { err::E0006.throwError("Cannot use string to index array.", "Try using an integral type instead.", this); },
                            [this](ptrn::Pattern *pattern) { err::E0006.throwError(fmt::format("Cannot use custom type '{}' to index array.", pattern->getTypeName()), "Try using an integral type instead.", this); },
                            [&, this](auto &&index) {
                                auto pattern = currPattern.get();
                                if (auto *iteratable = dynamic_cast<ptrn::Iteratable *>(pattern); iteratable != nullptr) {
                                    if (size_t(index) >= iteratable->getEntryCount())
                                        core::err::E0006.throwError("Index out of bounds.", fmt::format("Tried to access index {} in array of size {}.", index, iteratable->getEntryCount()), this);
                                    currPattern = iteratable->getEntry(index);
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

                if (auto iteratable = dynamic_cast<ptrn::Iteratable *>(indexPattern); iteratable != nullptr)
                    searchScope = iteratable->getEntries();
                else
                    indexable = false;

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
            } else {
                evaluator->readData(variablePattern->getOffset(), &value, variablePattern->getSize(), variablePattern->getSection());
            }

            if constexpr (!isString)
                value = hlp::changeEndianess(value, variablePattern->getSize(), variablePattern->getEndian());
        }
    };

}