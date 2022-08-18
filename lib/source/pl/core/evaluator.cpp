#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_function_call.hpp>
#include <pl/core/ast/ast_node_function_definition.hpp>

#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_wide_string.hpp>
#include <pl/patterns/pattern_enum.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_bitfield.hpp>
#include <pl/patterns/pattern_pointer.hpp>

namespace pl::core {

    void Evaluator::createParameterPack(const std::string &name, const std::vector<Token::Literal> &values) {
        this->getScope(0).parameterPack = ParameterPack {
            name,
            values
        };
    }

    void Evaluator::createArrayVariable(const std::string &name, ast::ASTNode *type, size_t entryCount) {
        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return;

        auto &variables = *this->getScope(0).scope;
        for (auto &variable : variables) {
            if (variable->getVariableName() == name) {
                err::E0003.throwError(fmt::format("Variable with name '{}' already exists in this scope.", name), {}, type);
            }
        }

        auto startOffset = this->dataOffset();
        auto typePatterns = type->createPatterns(this);
        auto typePattern = std::move(typePatterns.front());

        this->dataOffset() = startOffset;

        auto pattern = new ptrn::PatternArrayStatic(this, 0, typePattern->getSize() * entryCount);
        pattern->setEntries(std::move(typePattern), entryCount);
        pattern->setMemoryLocationType(ptrn::PatternMemoryType::Heap);

        auto &heap = this->getHeap();
        pattern->setOffset(heap.size());
        heap.resize(heap.size() + pattern->getSize());

        pattern->setVariableName(name);

        variables.push_back(std::unique_ptr<ptrn::Pattern>(pattern));
    }

    void Evaluator::createVariable(const std::string &name, ast::ASTNode *type, const std::optional<Token::Literal> &value, bool outVariable) {
        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return;

        auto &variables = *this->getScope(0).scope;
        for (auto &variable : variables) {
            if (variable->getVariableName() == name) {
                err::E0003.throwError(fmt::format("Variable with name '{}' already exists in this scope.", name), {}, type);
            }
        }

        bool heapType = false;
        auto startOffset = this->dataOffset();

        std::unique_ptr<ptrn::Pattern> pattern;
        auto typePattern = type->createPatterns(this);

        this->dataOffset() = startOffset;

        if (typePattern.empty()) {
            // Handle auto variables
            if (!value.has_value())
                err::E0003.throwError("Cannot determine type of 'auto' variable.", "Try initializing it directly with a literal.", type);

            if (std::get_if<u128>(&value.value()) != nullptr)
                pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternUnsigned(this, 0, sizeof(u128)));
            else if (std::get_if<i128>(&value.value()) != nullptr)
                pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternSigned(this, 0, sizeof(i128)));
            else if (std::get_if<double>(&value.value()) != nullptr)
                pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternFloat(this, 0, sizeof(double)));
            else if (std::get_if<bool>(&value.value()) != nullptr)
                pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternBoolean(this, 0));
            else if (std::get_if<char>(&value.value()) != nullptr)
                pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternCharacter(this, 0));
            else if (std::get_if<std::string>(&value.value()) != nullptr)
                pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternString(this, 0, 1));
            else if (auto patternValue = std::get_if<ptrn::Pattern *>(&value.value()); patternValue != nullptr) {
                pattern       = (*patternValue)->clone();
                heapType = true;
            } else
                err::E0003.throwError("Cannot determine type of 'auto' variable.", "Try initializing it directly with a literal.", type);
        } else {
            pattern = std::move(typePattern.front());

            if (dynamic_cast<ptrn::PatternStruct*>(pattern.get()) ||
                dynamic_cast<ptrn::PatternUnion*>(pattern.get()) ||
                dynamic_cast<ptrn::PatternArrayDynamic*>(pattern.get()) ||
                dynamic_cast<ptrn::PatternArrayStatic*>(pattern.get()) ||
                dynamic_cast<ptrn::PatternBitfield*>(pattern.get()) ||
                dynamic_cast<ptrn::PatternPointer*>(pattern.get()))
                heapType = true;
        }

        pattern->setVariableName(name);

        if (!heapType) {
            pattern->setOffset(this->getStack().size());
            pattern->setMemoryLocationType(ptrn::PatternMemoryType::Stack);
            this->getStack().emplace_back();
        } else {
            pattern->setMemoryLocationType(ptrn::PatternMemoryType::Heap);

            auto &heap = this->getHeap();
            if (!pattern->isHeapAddressValid()) {
                pattern->setOffset(heap.size());
                heap.resize(heap.size() + pattern->getSize());
            }
        }

        if (outVariable)
            this->m_outVariables[name] = pattern->getOffset();

        variables.push_back(std::move(pattern));
    }

    void Evaluator::setVariable(ptrn::Pattern *pattern, const Token::Literal &value) {
        Token::Literal castedLiteral = std::visit(hlp::overloaded {
            [&](double &value) -> Token::Literal {
                if (dynamic_cast<ptrn::PatternUnsigned *>(pattern))
                    return u128(value) & hlp::bitmask(pattern->getSize() * 8);
                else if (dynamic_cast<ptrn::PatternSigned *>(pattern))
                    return i128(value) & hlp::bitmask(pattern->getSize() * 8);
                else if (dynamic_cast<ptrn::PatternFloat *>(pattern))
                    return pattern->getSize() == sizeof(float) ? double(float(value)) : value;
                else
                    err::E0004.throwError(fmt::format("Cannot cast value of type 'floating point' to type '{}'.", pattern->getTypeName()));
            },
            [&](const std::string &value) -> Token::Literal {
               if (dynamic_cast<ptrn::PatternString *>(pattern))
                   return value;
               else
                   err::E0004.throwError(fmt::format("Cannot cast value of type 'string' to type '{}'.", pattern->getTypeName()));
            },
            [&](ptrn::Pattern *value) -> Token::Literal {
               if (value->getTypeName() == pattern->getTypeName())
                   return value;
               else
                   err::E0004.throwError(fmt::format("Cannot cast value of type '{}' to type '{}'.", value->getTypeName(), pattern->getTypeName()));
            },
            [&](auto &&value) -> Token::Literal {
                auto numBits = pattern->getSize() * 8;

               if (dynamic_cast<ptrn::PatternUnsigned *>(pattern) || dynamic_cast<ptrn::PatternEnum *>(pattern))
                   return u128(value) & hlp::bitmask(numBits);
               else if (dynamic_cast<ptrn::PatternSigned *>(pattern))
                   return hlp::signExtend(numBits, u128(value) & hlp::bitmask(numBits));
               else if (dynamic_cast<ptrn::PatternCharacter *>(pattern))
                   return char(value);
               else if (dynamic_cast<ptrn::PatternBoolean *>(pattern))
                   return bool(value);
               else if (dynamic_cast<ptrn::PatternFloat *>(pattern))
                   return pattern->getSize() == sizeof(float) ? double(float(value)) : value;
               else
                   err::E0004.throwError(fmt::format("Cannot cast value of type 'integer' to type '{}'.", pattern->getTypeName()));
            }
        }, value);

        castedLiteral = std::visit(hlp::overloaded {
           [](ptrn::Pattern *value) -> Token::Literal { return value; },
           [](std::string &value) -> Token::Literal { return value; },
           [&pattern](auto &&value) -> Token::Literal {
               return hlp::changeEndianess(value, pattern->getSize(), pattern->getEndian());
           }
        }, castedLiteral);

        if (pattern->getMemoryLocationType() == ptrn::PatternMemoryType::Stack)
            this->getStack()[pattern->getOffset()] = castedLiteral;
        else if (pattern->getMemoryLocationType() == ptrn::PatternMemoryType::Heap) {
            auto &heap = this->getHeap();
            auto offset = pattern->getOffset();
            if (!pattern->isHeapAddressValid()) {
                pattern->setOffset(heap.size());
                heap.resize(heap.size() + pattern->getSize());
            }

            if (offset > heap.size())
                err::E0011.throwError(fmt::format("Cannot access out of bounds heap address 0x{:08X}.", offset));

            std::visit(hlp::overloaded {
                [this, &pattern, &heap](ptrn::Pattern *value) {
                    if (pattern->getTypeName() != value->getTypeName())
                        err::E0004.throwError(fmt::format("Cannot assign value of type {} to variable {} of type {}", value->getTypeName(), value->getVariableName(), pattern->getTypeName()));
                    else if (pattern->getSize() != value->getSize())
                        err::E0004.throwError(fmt::format("Cannot assign value of type {} to variable {} of type {} with a different size.", value->getTypeName(), value->getVariableName(), pattern->getTypeName()), "This can happen when using dynamically sized types as variable types.");

                    if (value->getMemoryLocationType() == ptrn::PatternMemoryType::Provider)
                        this->readData(value->getOffset(), &heap[pattern->getOffset()], pattern->getSize());
                    else if (value->getMemoryLocationType() == ptrn::PatternMemoryType::Heap)
                        std::memcpy(&heap[pattern->getOffset()], &heap[value->getOffset()], pattern->getSize());
                },
                [](std::string &) { err::E0001.throwError("Cannot place string variable on the heap."); },
                [&pattern, &heap](auto &&value) {
                    std::memcpy(&heap[pattern->getOffset()], &value, pattern->getSize());
                }
            }, castedLiteral);
        }
    }

    void Evaluator::setVariable(const std::string &name, const Token::Literal &value) {
        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return;

        std::unique_ptr<ptrn::Pattern> pattern = nullptr;

        {
            auto &variables = *this->getScope(0).scope;
            for (auto &variable : variables) {
                if (variable->getVariableName() == name) {
                    pattern = variable->clone();
                    break;
                }
            }
        }

        if (pattern == nullptr) {
            auto &variables = *this->getGlobalScope().scope;
            for (auto &variable : variables) {
                if (variable->getVariableName() == name) {
                    if (!variable->isLocal())
                        err::E0011.throwError(fmt::format("Cannot modify global variable '{}' as it has been placed in memory.", name));

                    pattern = variable->clone();
                    break;
                }
            }
        }

        if (pattern == nullptr)
            err::E0003.throwError(fmt::format("Cannot find variable '{}' in this scope.", name));

        if (!pattern->isLocal()) return;

        this->setVariable(pattern.get(), value);
    }

    void Evaluator::pushScope(ptrn::Pattern *parent, std::vector<std::shared_ptr<ptrn::Pattern>> &scope) {
        if (this->m_scopes.size() > this->getEvaluationDepth())
            err::E0007.throwError(fmt::format("Evaluation depth exceeded set limit of '{}'.", this->getEvaluationDepth()), "If this is intended, try increasing the limit using '#pragma eval_depth <new_limit>'.");

        this->handleAbort();

        this->m_scopes.push_back({ parent, &scope, std::nullopt, { }, this->m_heap.size() });
    }

    void Evaluator::popScope() {
        if (this->m_scopes.empty())
            return;

        this->m_heap.resize(this->m_scopes.back().heapStartSize);

        this->m_scopes.pop_back();
    }

    bool Evaluator::evaluate(const std::string &sourceCode, const std::vector<std::shared_ptr<ast::ASTNode>> &ast) {
        this->m_stack.clear();
        this->m_customFunctions.clear();
        this->m_scopes.clear();
        this->m_mainResult.reset();
        this->m_aborted = false;
        this->m_colorIndex = 0;
        this->m_patterns.clear();

        if (this->m_allowDangerousFunctions == DangerousFunctionPermission::Deny)
            this->m_allowDangerousFunctions = DangerousFunctionPermission::Ask;

        PL_ON_SCOPE_EXIT {
            this->m_envVariables.clear();
        };

        this->dataOffset()       = 0x00;
        this->m_currPatternCount = 0;

        this->m_customFunctionDefinitions.clear();

        try {
            this->setCurrentControlFlowStatement(ControlFlowStatement::None);
            pushScope(nullptr, this->m_patterns);

            for (auto &node : ast) {
                if (dynamic_cast<ast::ASTNodeTypeDecl *>(node.get())) {
                    ;    // Don't create patterns from type declarations
                } else if (dynamic_cast<ast::ASTNodeFunctionCall *>(node.get())) {
                    (void)node->evaluate(this);
                } else if (dynamic_cast<ast::ASTNodeFunctionDefinition *>(node.get())) {
                    this->m_customFunctionDefinitions.push_back(node->evaluate(this));
                } else if (auto varDeclNode = dynamic_cast<ast::ASTNodeVariableDecl *>(node.get())) {
                    for (auto &pattern : node->createPatterns(this)) {
                        if (varDeclNode->getPlacementOffset() == nullptr) {
                            auto type = varDeclNode->getType()->evaluate(this);

                            auto &name = pattern->getVariableName();
                            this->createVariable(name, type.get(), std::nullopt, varDeclNode->isOutVariable());

                            if (varDeclNode->isInVariable() && this->m_inVariables.contains(name))
                                this->setVariable(name, this->m_inVariables[name]);
                        } else {
                            this->m_patterns.push_back(std::move(pattern));
                        }
                    }
                } else {
                    auto newPatterns = node->createPatterns(this);
                    std::move(newPatterns.begin(), newPatterns.end(), std::back_inserter(this->m_patterns));
                }
            }

            if (this->m_customFunctions.contains("main")) {
                auto mainFunction = this->m_customFunctions["main"];

                if (mainFunction.parameterCount.max > 0)
                    err::E0009.throwError("Entry point function 'main' may not have any parameters.");

                this->m_mainResult = mainFunction.func(this, {});
            }
        } catch (err::EvaluatorError::Exception &e) {

            auto node = e.getUserData();

            const auto line = node == nullptr ? 0 : node->getLine();
            const auto column = node == nullptr ? 0 : node->getColumn();

            this->getConsole().setHardError(err::PatternLanguageError(e.format(sourceCode, line, column), line, column));

            this->m_patterns.clear();

            this->m_currPatternCount = 0;

            return false;
        }

        // Remove global local variables
        std::erase_if(this->m_patterns, [](const std::shared_ptr<ptrn::Pattern> &pattern) {
            return pattern->isLocal();
        });

        return true;
    }

    void Evaluator::patternCreated() {
        if (this->m_currPatternCount > this->m_patternLimit)
            err::E0007.throwError(fmt::format("Pattern count exceeded set limit of '{}'.", this->getPatternLimit()), "If this is intended, try increasing the limit using '#pragma pattern_limit <new_limit>'.");
        this->m_currPatternCount++;
    }

    void Evaluator::patternDestroyed() {
        this->m_currPatternCount--;
    }

}