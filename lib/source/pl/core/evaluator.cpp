#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_array_variable_decl.hpp>
#include <pl/core/ast/ast_node_function_call.hpp>
#include <pl/core/ast/ast_node_function_definition.hpp>
#include <pl/core/ast/ast_node_compound_statement.hpp>

#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_string.hpp>

namespace pl::core {

    std::map<std::string, Token::Literal> Evaluator::getOutVariables() const {
        std::map<std::string, Token::Literal> result;

        for (const auto &[name, pattern] : this->m_outVariables) {
            result.insert({ name, pattern->getValue() });
        }

        return result;
    }

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

        auto pattern = new ptrn::PatternArrayDynamic(this, 0, typePattern->getSize() * entryCount);

        typePattern->setLocal(true);
        std::vector<std::shared_ptr<ptrn::Pattern>> entries;
        for (size_t i = 0; i < entryCount; i++) {
            auto entryPattern = typePattern->clone();

            auto &heap = this->getHeap();
            entryPattern->setOffset(u64(heap.size()) << 32);
            heap.emplace_back();

            entries.push_back(std::move(entryPattern));
        }

        pattern->setEntries(std::move(entries));
        pattern->setLocal(true);

        pattern->setVariableName(name);

        if (this->isDebugModeEnabled())
            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Creating local array variable '{} {}[{}]' at heap address 0x{:X}.", pattern->getTypeName(), pattern->getVariableName(), entryCount, pattern->getOffset()));

        variables.push_back(std::unique_ptr<ptrn::Pattern>(pattern));
    }

    void Evaluator::createVariable(const std::string &name, ast::ASTNode *type, const std::optional<Token::Literal> &value, bool outVariable, bool reference) {
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

        std::shared_ptr<ptrn::Pattern> pattern;
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
            else if (auto patternValue = std::get_if<ptrn::Pattern *>(&value.value()); patternValue != nullptr)
                pattern       = (*patternValue)->clone();
            else
                err::E0003.throwError("Cannot determine type of 'auto' variable.", "Try initializing it directly with a literal.", type);
        } else {
            pattern = std::move(typePattern.front());
        }

        pattern->setVariableName(name);

        if (!reference) {
            pattern->setLocal(true);
            pattern->setOffset(u64(this->getHeap().size()) << 32);
            this->getHeap().emplace_back().resize(pattern->getSize());
        }
        pattern->setReference(reference);

        if (outVariable) {
            if (this->isGlobalScope())
                this->m_outVariables[name] = pattern->clone();
            else
                err::E0003.throwError("Out variables can only be declared in the global scope.", {}, type);
        }

        if (this->isDebugModeEnabled())
            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Creating local variable '{} {}' at heap address 0x{:X}.", pattern->getTypeName(), pattern->getVariableName(), pattern->getOffset()));

        variables.push_back(std::move(pattern));
    }

    template<typename T>
    static T truncateValue(size_t bytes, const T &value) {
        T result = { };
        std::memcpy(&result, &value, std::min(sizeof(result), bytes));

        if (std::signed_integral<T>)
            result = hlp::signExtend(bytes * 8, result);

        return result;
    }

    static Token::Literal castLiteral(const ptrn::Pattern *pattern, const Token::Literal &literal) {
        return std::visit(hlp::overloaded {
            [&](auto &value) -> Token::Literal {
               if (dynamic_cast<const ptrn::PatternUnsigned*>(pattern) || dynamic_cast<const ptrn::PatternEnum*>(pattern))
                   return truncateValue<u128>(pattern->getSize(), u128(value));
               else if (dynamic_cast<const ptrn::PatternSigned*>(pattern))
                   return truncateValue<i128>(pattern->getSize(), i128(value));
               else if (dynamic_cast<const ptrn::PatternFloat*>(pattern)) {
                   if (pattern->getSize() == sizeof(float))
                       return double(float(value));
                   else
                       return double(value);
               } else if (dynamic_cast<const ptrn::PatternBoolean*>(pattern))
                   return value == 0 ? u128(0) : u128(1);
               else if (dynamic_cast<const ptrn::PatternCharacter*>(pattern))
                   return truncateValue(pattern->getSize(), u128(value));
               else if (dynamic_cast<const ptrn::PatternWideCharacter*>(pattern))
                   return truncateValue(pattern->getSize(), u128(value));
               else if (dynamic_cast<const ptrn::PatternString*>(pattern))
                   return Token::literalToString(value, false);
               else
                   err::E0004.throwError(fmt::format("Cannot cast from type 'integer' to type '{}'.", pattern->getTypeName()));
            },
            [&](const std::string &value) -> Token::Literal {
                if (dynamic_cast<const ptrn::PatternUnsigned*>(pattern)) {
                    if (value.size() <= pattern->getSize()) {
                        u128 result = 0;
                        std::memcpy(&result, value.data(), value.size());
                        return result;
                    } else {
                        err::E0004.throwError(fmt::format("String of size {} cannot be packed into integer of size {}", value.size(), pattern->getSize()));
                    }
                } else if (dynamic_cast<const ptrn::PatternBoolean*>(pattern))
                    return !value.empty();
                else if (dynamic_cast<const ptrn::PatternString*>(pattern))
                    return value;
                else
                    err::E0004.throwError(fmt::format("Cannot cast from type 'string' to type '{}'.", pattern->getTypeName()));
            },
            [&](ptrn::Pattern * const value) -> Token::Literal {
                if (value->getTypeName() == pattern->getTypeName())
                    return value;
                else
                    err::E0004.throwError(fmt::format("Cannot cast from type '{}' to type '{}'.", value->getTypeName(), pattern->getTypeName()));
            }
        }, literal);
    }

    void Evaluator::setVariable(const std::string &name, const Token::Literal &value) {
        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return;

        auto pattern = [&]() -> ptrn::Pattern * {
            std::shared_ptr<ptrn::Pattern> *variablePattern = nullptr;

            // Search for variable in current scope
            {
                auto &variables = *this->getScope(0).scope;
                for (auto &variable : variables) {
                    if (variable->getVariableName() == name) {
                        variablePattern = &variable;
                        break;
                    }
                }
            }

            // If there's no variable with that name in the current scope, search the global scope
            if (variablePattern == nullptr) {
                auto &variables = *this->getGlobalScope().scope;
                for (auto &variable : variables) {
                    if (variable->getVariableName() == name) {
                        if (!variable->isLocal())
                            err::E0011.throwError(fmt::format("Cannot modify global variable '{}' as it has been placed in memory.", name));

                        variablePattern = &variable;
                        break;
                    }
                }
            }

            // If the global scope also doesn't have a variable with this name, it doesn't exist. Bail out
            if (variablePattern == nullptr)
                return nullptr;

            // If the variable is being set to a pattern, adjust its layout to the real layout as it potentially contains dynamically sized members
            std::visit(hlp::overloaded {
                [&](ptrn::Pattern * const value) {
                    auto offset = variablePattern->get()->getOffset();
                    bool reference = variablePattern->get()->isReference();

                    *variablePattern = value->clone();

                    auto pattern = variablePattern->get();
                    pattern->setVariableName(name);
                    pattern->setReference(reference);

                    if (!reference) {
                        pattern->setLocal(true);
                        pattern->setOffset(offset);
                    }
                },
                [](const auto &) {}
            }, value);

            return variablePattern->get();
        }();

        if (pattern == nullptr)
            err::E0003.throwError(fmt::format("Cannot find variable '{}' in this scope.", name));

        this->setVariable(pattern, value);
    }

    void Evaluator::setVariable(ptrn::Pattern *pattern, const Token::Literal &value) {
        if (!pattern->isLocal()) return;

        if (pattern->getSize() > 0xFFFF'FFFF)
            err::E0003.throwError(fmt::format("Value is too large to place into local variable '{}'.", pattern->getVariableName()));

        // Cast values to type given by pattern
        Token::Literal castedValue = castLiteral(pattern, value);

        // Write value into variable storage
        {
            auto &storage = this->getHeap()[pattern->getHeapAddress()];
            std::visit(hlp::overloaded {
                    [this, &pattern, &storage](const auto &value) {
                        auto adjustedValue = hlp::changeEndianess(value, pattern->getSize(), pattern->getEndian());

                        storage.resize(pattern->getSize());
                        std::memcpy(storage.data(), &adjustedValue, pattern->getSize());

                        if (this->isDebugModeEnabled())
                            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Setting local variable '{}' to {}.", pattern->getVariableName(), value));
                    },
                    [this, &pattern, &storage](const i128 &value) {
                        auto adjustedValue = hlp::changeEndianess(value, pattern->getSize(), pattern->getEndian());
                        adjustedValue = hlp::signExtend(pattern->getSize() * 8, adjustedValue);

                        storage.resize(pattern->getSize());
                        std::memcpy(storage.data(), &adjustedValue, pattern->getSize());

                        if (this->isDebugModeEnabled())
                            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Setting local variable '{}' to {}.", pattern->getVariableName(), value));
                    },
                    [this, &pattern, &storage](const std::string &value) {
                        pattern->setSize(value.size());
                        storage.resize(value.size());
                        std::memcpy(storage.data(), value.data(), pattern->getSize());

                        if (this->isDebugModeEnabled())
                            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Setting local variable '{}' to {}.", pattern->getVariableName(), value));
                    },
                    [this, &pattern, &storage](ptrn::Pattern * const value) {
                        storage.resize(value->getSize());

                        this->readData(value->getOffset(), storage.data(), value->getSize(), value->isLocal());

                        if (this->isDebugModeEnabled())
                            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Setting local variable '{}' to {:02X}.", pattern->getVariableName(), fmt::join(storage, " ")));
                    }
            }, castedValue);
        }
    }

    void Evaluator::pushScope(const std::shared_ptr<ptrn::Pattern> &parent, std::vector<std::shared_ptr<ptrn::Pattern>> &scope) {
        if (this->m_scopes.size() > this->getEvaluationDepth())
            err::E0007.throwError(fmt::format("Evaluation depth exceeded set limit of '{}'.", this->getEvaluationDepth()), "If this is intended, try increasing the limit using '#pragma eval_depth <new_limit>'.");

        this->handleAbort();

        const auto &heap = this->getHeap();

        this->m_scopes.push_back({ parent, &scope, std::nullopt, { }, heap.size() });

        if (this->isDebugModeEnabled())
            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Entering new scope #{}. Parent: '{}', Heap Size: {}.", this->m_scopes.size(), parent == nullptr ? "None" : parent->getVariableName(), heap.size()));
    }

    void Evaluator::popScope() {
        if (this->m_scopes.empty())
            return;

        auto &currScope = this->getScope(0);

        auto &heap = this->getHeap();

        heap.resize(currScope.heapStartSize);

        if (this->isDebugModeEnabled())
            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Exiting scope #{}. Parent: '{}', Heap Size: {}.", this->m_scopes.size(), currScope.parent == nullptr ? "None" : currScope.parent->getVariableName(), heap.size()));


        this->m_scopes.pop_back();
    }

    bool Evaluator::evaluate(const std::string &sourceCode, const std::vector<std::shared_ptr<ast::ASTNode>> &ast) {
        this->m_heap.clear();
        this->m_customFunctions.clear();
        this->m_scopes.clear();
        this->m_mainResult.reset();
        this->m_aborted = false;
        this->m_colorIndex = 0;
        this->m_patterns.clear();
        this->m_evaluated = false;

        if (this->m_allowDangerousFunctions == DangerousFunctionPermission::Deny)
            this->m_allowDangerousFunctions = DangerousFunctionPermission::Ask;

        PL_ON_SCOPE_EXIT {
            this->m_envVariables.clear();
            this->m_evaluated = true;
        };

        this->m_currPatternCount = 0;

        this->m_customFunctionDefinitions.clear();

        try {
            this->setCurrentControlFlowStatement(ControlFlowStatement::None);
            pushScope(nullptr, this->m_patterns);

            for (auto &topLevelNode : ast) {

                std::vector<ast::ASTNode*> nodes;

                if (auto compoundNode = dynamic_cast<ast::ASTNodeCompoundStatement*>(topLevelNode.get())) {
                    for (const auto &statement : compoundNode->getStatements())
                        nodes.push_back(statement.get());
                } else {
                    nodes.push_back(topLevelNode.get());
                }

                for (auto node : nodes) {
                    if (node == nullptr)
                        continue;

                    auto startOffset = this->dataOffset();

                    if (dynamic_cast<ast::ASTNodeTypeDecl *>(node)) {
                        ;    // Don't create patterns from type declarations
                    } else if (dynamic_cast<ast::ASTNodeFunctionCall *>(node) != nullptr) {
                        (void)node->evaluate(this);
                    } else if (dynamic_cast<ast::ASTNodeFunctionDefinition *>(node) != nullptr) {
                        this->m_customFunctionDefinitions.push_back(node->evaluate(this));
                    } else if (auto varDeclNode = dynamic_cast<ast::ASTNodeVariableDecl *>(node); varDeclNode != nullptr) {
                        for (auto &pattern : varDeclNode->createPatterns(this)) {
                            if (varDeclNode->getPlacementOffset() == nullptr) {
                                auto type = varDeclNode->getType()->evaluate(this);

                                auto &name = pattern->getVariableName();
                                this->createVariable(name, type.get(), std::nullopt, varDeclNode->isOutVariable());

                                if (varDeclNode->isInVariable() && this->m_inVariables.contains(name))
                                    this->setVariable(name, this->m_inVariables[name]);

                                this->dataOffset() = startOffset;
                            } else {
                                this->m_patterns.push_back(std::move(pattern));
                            }
                        }
                    } else if (auto arrayVarDeclNode = dynamic_cast<ast::ASTNodeArrayVariableDecl *>(node); arrayVarDeclNode != nullptr) {
                        for (auto &pattern : arrayVarDeclNode->createPatterns(this)) {
                            if (arrayVarDeclNode->getPlacementOffset() == nullptr) {
                                auto type = arrayVarDeclNode->getType()->evaluate(this);

                                auto &name = pattern->getVariableName();
                                this->createArrayVariable(name, type.get(), dynamic_cast<ptrn::Iteratable*>(pattern.get())->getEntryCount());

                                this->dataOffset() = startOffset;
                            } else {
                                this->m_patterns.push_back(std::move(pattern));
                            }
                        }
                    } else {
                        auto newPatterns = node->createPatterns(this);
                        std::move(newPatterns.begin(), newPatterns.end(), std::back_inserter(this->m_patterns));
                    }

                    if (this->getCurrentControlFlowStatement() == ControlFlowStatement::Return)
                        break;
                    else
                        this->setCurrentControlFlowStatement(ControlFlowStatement::None);
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

            // Don't clear patterns on error if debug mode is enabled
            if (this->m_debugMode)
                return false;

            this->m_patterns.clear();

            this->m_currPatternCount = 0;

            return false;
        }

        return true;
    }

    void Evaluator::patternCreated() {
        if (this->m_currPatternCount > this->m_patternLimit && !this->m_evaluated)
            err::E0007.throwError(fmt::format("Pattern count exceeded set limit of '{}'.", this->getPatternLimit()), "If this is intended, try increasing the limit using '#pragma pattern_limit <new_limit>'.");
        this->m_currPatternCount++;
    }

    void Evaluator::patternDestroyed() {
        this->m_currPatternCount--;
    }

}