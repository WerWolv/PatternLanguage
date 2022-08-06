#include "pl/evaluator.hpp"
#include "pl/patterns/pattern.hpp"

#include "pl/ast/ast_node.hpp"
#include "pl/ast/ast_node_type_decl.hpp"
#include "pl/ast/ast_node_variable_decl.hpp"
#include "pl/ast/ast_node_function_call.hpp"
#include "pl/ast/ast_node_function_definition.hpp"

#include "pl/patterns/pattern_unsigned.hpp"
#include "pl/patterns/pattern_signed.hpp"
#include "pl/patterns/pattern_float.hpp"
#include "pl/patterns/pattern_boolean.hpp"
#include "pl/patterns/pattern_character.hpp"
#include "pl/patterns/pattern_string.hpp"
#include "pl/patterns/pattern_wide_string.hpp"
#include "pl/patterns/pattern_enum.hpp"
#include "pl/patterns/pattern_array_dynamic.hpp"
#include "pl/patterns/pattern_array_static.hpp"
#include "pl/patterns/pattern_struct.hpp"
#include "pl/patterns/pattern_union.hpp"
#include "pl/patterns/pattern_bitfield.hpp"
#include "pl/patterns/pattern_pointer.hpp"

namespace pl {

    void Evaluator::createParameterPack(const std::string &name, const std::vector<Token::Literal> &values) {
        this->getScope(0).parameterPack = ParameterPack {
            name,
            values
        };
    }

    void Evaluator::createArrayVariable(const std::string &name, pl::ASTNode *type, size_t entryCount) {
        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return;

        auto &variables = *this->getScope(0).scope;
        for (auto &variable : variables) {
            if (variable->getVariableName() == name) {
                LogConsole::abortEvaluation(fmt::format("variable with name '{}' already exists", name));
            }
        }

        auto startOffset = this->dataOffset();
        auto typePatterns = type->createPatterns(this);
        auto typePattern = std::move(typePatterns.front());

        this->dataOffset() = startOffset;

        auto pattern = new PatternArrayStatic(this, 0, typePattern->getSize() * entryCount);
        pattern->setEntries(std::move(typePattern), entryCount);
        pattern->setMemoryLocationType(PatternMemoryType::Heap);

        auto &heap = this->getHeap();
        pattern->setOffset(heap.size());
        heap.resize(heap.size() + pattern->getSize());

        pattern->setVariableName(name);

        variables.push_back(std::move(std::unique_ptr<Pattern>(pattern)));
    }

    void Evaluator::createVariable(const std::string &name, ASTNode *type, const std::optional<Token::Literal> &value, bool outVariable) {
        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return;

        auto &variables = *this->getScope(0).scope;
        for (auto &variable : variables) {
            if (variable->getVariableName() == name) {
                LogConsole::abortEvaluation(fmt::format("variable with name '{}' already exists", name));
            }
        }

        bool heapType = false;
        auto startOffset = this->dataOffset();

        std::unique_ptr<Pattern> pattern;
        auto typePattern = type->createPatterns(this);

        this->dataOffset() = startOffset;

        if (typePattern.empty()) {
            // Handle auto variables
            if (!value.has_value())
                LogConsole::abortEvaluation("cannot determine type of auto variable", type);

            if (std::get_if<u128>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternUnsigned(this, 0, sizeof(u128)));
            else if (std::get_if<i128>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternSigned(this, 0, sizeof(i128)));
            else if (std::get_if<double>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternFloat(this, 0, sizeof(double)));
            else if (std::get_if<bool>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternBoolean(this, 0));
            else if (std::get_if<char>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternCharacter(this, 0));
            else if (std::get_if<std::string>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternString(this, 0, 1));
            else if (auto patternValue = std::get_if<Pattern *>(&value.value()); patternValue != nullptr) {
                pattern       = (*patternValue)->clone();
                heapType = true;
            } else
                LogConsole::abortEvaluation("cannot determine type of auto variable", type);
        } else {
            pattern = std::move(typePattern.front());

            if (dynamic_cast<PatternStruct*>(pattern.get()) ||
                dynamic_cast<PatternUnion*>(pattern.get()) ||
                dynamic_cast<PatternArrayDynamic*>(pattern.get()) ||
                dynamic_cast<PatternArrayStatic*>(pattern.get()) ||
                dynamic_cast<PatternBitfield*>(pattern.get()) ||
                dynamic_cast<PatternPointer*>(pattern.get()))
                heapType = true;
        }

        pattern->setVariableName(name);

        if (!heapType) {
            pattern->setOffset(this->getStack().size());
            pattern->setMemoryLocationType(PatternMemoryType::Stack);
            this->getStack().emplace_back();
        } else {
            pattern->setMemoryLocationType(PatternMemoryType::Heap);

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

    void Evaluator::setVariable(pl::Pattern *pattern, const Token::Literal &value) {
        Token::Literal castedLiteral = std::visit(overloaded {
            [&](double &value) -> Token::Literal {
                if (dynamic_cast<PatternUnsigned *>(pattern))
                    return u128(value) & bitmask(pattern->getSize() * 8);
                else if (dynamic_cast<PatternSigned *>(pattern))
                    return i128(value) & bitmask(pattern->getSize() * 8);
                else if (dynamic_cast<PatternFloat *>(pattern))
                    return pattern->getSize() == sizeof(float) ? double(float(value)) : value;
                else
                    LogConsole::abortEvaluation(fmt::format("cannot cast type 'double' to type '{}'", pattern->getTypeName()));
            },
            [&](const std::string &value) -> Token::Literal {
               if (dynamic_cast<PatternString *>(pattern))
                   return value;
               else
                   LogConsole::abortEvaluation(fmt::format("cannot cast type 'string' to type '{}'", pattern->getTypeName()));
            },
            [&](Pattern *value) -> Token::Literal {
               if (value->getTypeName() == pattern->getTypeName())
                   return value;
               else
                   LogConsole::abortEvaluation(fmt::format("cannot cast type '{}' to type '{}'", value->getTypeName(), pattern->getTypeName()));
            },
            [&](auto &&value) -> Token::Literal {
                auto numBits = pattern->getSize() * 8;

               if (dynamic_cast<PatternUnsigned *>(pattern) || dynamic_cast<PatternEnum *>(pattern))
                   return u128(value) & bitmask(numBits);
               else if (dynamic_cast<PatternSigned *>(pattern))
                   return pl::signExtend(numBits, u128(value) & bitmask(numBits));
               else if (dynamic_cast<PatternCharacter *>(pattern))
                   return char(value);
               else if (dynamic_cast<PatternBoolean *>(pattern))
                   return bool(value);
               else if (dynamic_cast<PatternFloat *>(pattern))
                   return pattern->getSize() == sizeof(float) ? double(float(value)) : value;
               else
                   LogConsole::abortEvaluation(fmt::format("cannot cast integer literal to type '{}'", pattern->getTypeName()));
            }
        }, value);

        castedLiteral = std::visit(overloaded {
           [](Pattern *value) -> Token::Literal { return value; },
           [](std::string &value) -> Token::Literal { return value; },
           [&pattern](auto &&value) -> Token::Literal {
               return pl::changeEndianess(value, pattern->getSize(), pattern->getEndian());
           }
        }, castedLiteral);

        if (pattern->getMemoryLocationType() == PatternMemoryType::Stack)
            this->getStack()[pattern->getOffset()] = castedLiteral;
        else if (pattern->getMemoryLocationType() == PatternMemoryType::Heap) {
            auto &heap = this->getHeap();
            auto offset = pattern->getOffset();
            if (!pattern->isHeapAddressValid()) {
                pattern->setOffset(heap.size());
                heap.resize(heap.size() + pattern->getSize());
            }

            if (offset > heap.size())
                LogConsole::abortEvaluation(fmt::format("attempted to access invalid heap address 0x{:X}", offset));

            std::visit(overloaded {
                [this, &pattern, &heap](Pattern *value) {
                    if (pattern->getTypeName() != value->getTypeName())
                        LogConsole::abortEvaluation(fmt::format("cannot assign value of type {} to variable {} of type {}", value->getTypeName(), value->getVariableName(), pattern->getTypeName()));
                    else if (pattern->getSize() != value->getSize())
                        LogConsole::abortEvaluation("cannot assign value to variable of different size");

                    if (value->getMemoryLocationType() == PatternMemoryType::Provider)
                        this->readData(value->getOffset(), &heap[pattern->getOffset()], pattern->getSize());
                    else if (value->getMemoryLocationType() == PatternMemoryType::Heap)
                        std::memcpy(&heap[pattern->getOffset()], &heap[value->getOffset()], pattern->getSize());
                },
                [](std::string &value) { LogConsole::abortEvaluation("cannot assign string type to heap variable"); },
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

        std::unique_ptr<Pattern> pattern = nullptr;

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
                        LogConsole::abortEvaluation(fmt::format("cannot modify global variable '{}' which has been placed in memory", name));

                    pattern = variable->clone();
                    break;
                }
            }
        }

        if (pattern == nullptr)
            LogConsole::abortEvaluation(fmt::format("no variable with name '{}' found", name));

        if (!pattern->isLocal()) return;

        this->setVariable(pattern.get(), value);
    }

    void Evaluator::pushScope(Pattern *parent, std::vector<std::shared_ptr<Pattern>> &scope) {
        if (this->m_scopes.size() > this->getEvaluationDepth())
            LogConsole::abortEvaluation(fmt::format("evaluation depth exceeded set limit of {}", this->getEvaluationDepth()));

        this->handleAbort();

        this->m_scopes.push_back({ parent, &scope, { }, { }, this->m_heap.size() });
    }

    void Evaluator::popScope() {
        if (this->m_scopes.empty())
            return;

        this->m_heap.resize(this->m_scopes.back().heapStartSize);

        this->m_scopes.pop_back();
    }

    std::optional<std::vector<std::shared_ptr<Pattern>>> Evaluator::evaluate(const std::vector<std::shared_ptr<ASTNode>> &ast) {
        this->m_stack.clear();
        this->m_customFunctions.clear();
        this->m_scopes.clear();
        this->m_mainResult.reset();
        this->m_aborted = false;
        this->m_colorIndex = 0;

        if (this->m_allowDangerousFunctions == DangerousFunctionPermission::Deny)
            this->m_allowDangerousFunctions = DangerousFunctionPermission::Ask;

        PL_ON_SCOPE_EXIT {
            this->m_envVariables.clear();
        };

        this->dataOffset()       = 0x00;
        this->m_currPatternCount = 0;

        this->m_customFunctionDefinitions.clear();

        std::vector<std::shared_ptr<Pattern>> patterns;

        try {
            this->setCurrentControlFlowStatement(ControlFlowStatement::None);
            pushScope(nullptr, patterns);
            PL_ON_SCOPE_EXIT {
                popScope();
            };

            for (auto &node : ast) {
                if (dynamic_cast<ASTNodeTypeDecl *>(node.get())) {
                    ;    // Don't create patterns from type declarations
                } else if (dynamic_cast<ASTNodeFunctionCall *>(node.get())) {
                    (void)node->evaluate(this);
                } else if (dynamic_cast<ASTNodeFunctionDefinition *>(node.get())) {
                    this->m_customFunctionDefinitions.push_back(node->evaluate(this));
                } else if (auto varDeclNode = dynamic_cast<ASTNodeVariableDecl *>(node.get())) {
                    for (auto &pattern : node->createPatterns(this)) {
                        if (varDeclNode->getPlacementOffset() == nullptr) {
                            auto type = varDeclNode->getType()->evaluate(this);

                            auto &name = pattern->getVariableName();
                            this->createVariable(name, type.get(), std::nullopt, varDeclNode->isOutVariable());

                            if (varDeclNode->isInVariable() && this->m_inVariables.contains(name))
                                this->setVariable(name, this->m_inVariables[name]);
                        } else {
                            patterns.push_back(std::move(pattern));
                        }
                    }
                } else {
                    auto newPatterns = node->createPatterns(this);
                    std::move(newPatterns.begin(), newPatterns.end(), std::back_inserter(patterns));
                }
            }

            if (this->m_customFunctions.contains("main")) {
                auto mainFunction = this->m_customFunctions["main"];

                if (mainFunction.parameterCount.max > 0)
                    LogConsole::abortEvaluation("main function may not accept any arguments");

                this->m_mainResult = mainFunction.func(this, {});
            }
        } catch (err::Error &e) {

            this->getConsole().log(LogConsole::Level::Error, e.what());

            patterns.clear();

            this->m_currPatternCount = 0;

            return std::nullopt;
        }

        // Remove global local variables
        std::erase_if(patterns, [](const std::shared_ptr<Pattern> &pattern) {
            return pattern->isLocal();
        });

        return patterns;
    }

    void Evaluator::patternCreated() {
        if (this->m_currPatternCount > this->m_patternLimit)
            LogConsole::abortEvaluation(fmt::format("exceeded maximum number of patterns: {}", this->m_patternLimit));
        this->m_currPatternCount++;
    }

    void Evaluator::patternDestroyed() {
        this->m_currPatternCount--;
    }

}