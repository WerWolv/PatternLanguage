#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_array_variable_decl.hpp>
#include <pl/core/ast/ast_node_pointer_variable_decl.hpp>
#include <pl/core/ast/ast_node_function_call.hpp>
#include <pl/core/ast/ast_node_function_definition.hpp>
#include <pl/core/ast/ast_node_compound_statement.hpp>

#include <pl/patterns/pattern_unsigned.hpp>
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

    void Evaluator::createArrayVariable(const std::string &name, ast::ASTNode *type, size_t entryCount, u64 section, bool constant) {
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

        if (section == ptrn::Pattern::HeapSectionId) {
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
        } else {
            typePattern->setSection(section);
            std::vector<std::shared_ptr<ptrn::Pattern>> entries;
            for (size_t i = 0; i < entryCount; i++) {
                auto entryPattern = typePattern->clone();
                entryPattern->setOffset(entryPattern->getSize() * i);
                entries.push_back(std::move(entryPattern));
            }
            pattern->setEntries(std::move(entries));
            pattern->setSection(section);
        }

        pattern->setVariableName(name);

        if (this->isDebugModeEnabled())
            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Creating local array variable '{} {}[{}]' at heap address 0x{:X}.", pattern->getTypeName(), pattern->getVariableName(), entryCount, pattern->getOffset()));

        pattern->setConstant(constant);
        variables.push_back(std::unique_ptr<ptrn::Pattern>(pattern));
    }

    std::shared_ptr<ptrn::Pattern> Evaluator::createVariable(const std::string &name, ast::ASTNode *type, const std::optional<Token::Literal> &value, bool outVariable, bool reference, bool templateVariable, bool constant) {
        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return nullptr;

        if (templateVariable) {
            std::erase_if(this->m_templateParameters.back(), [&](const auto &var) {
                return var->getVariableName() == name;
            });
        } else {
            auto &currScope = this->getScope(0);
            for (auto &variable : *currScope.scope) {
                if (variable->getVariableName() == name) {
                    err::E0003.throwError(fmt::format("Variable with name '{}' already exists in this scope.", name), {}, type);
                }
            }
        }

        auto sectionId = this->getSectionId();
        auto startOffset = this->dataOffset();

        auto heapAddress = u64(this->getHeap().size());
        u32 patternLocalAddress = 0;
        if (!reference) {
            if (sectionId == ptrn::Pattern::PatternLocalSectionId) {
                patternLocalAddress = this->m_patternLocalStorage.empty() ? 0 : this->m_patternLocalStorage.rbegin()->first + 1;
                this->m_patternLocalStorage.insert({ patternLocalAddress, { } });
            } else if (sectionId == ptrn::Pattern::HeapSectionId) {
                this->getHeap().emplace_back();
            } else {
                err::E0001.throwError(fmt::format("Attempted to place a variable into section 0x{:X}.", sectionId), {}, type);
            }
        }

        std::shared_ptr<ptrn::Pattern> pattern;

        this->pushSectionId(ptrn::Pattern::MainSectionId);
        auto typePattern = type->createPatterns(this);
        this->popSectionId();

        this->dataOffset() = startOffset;

        if (typePattern.empty()) {
            // Handle auto variables
            if (!value.has_value())
                err::E0003.throwError("Cannot determine type of 'auto' variable.", "Try initializing it directly with a literal.", type);

            if (std::get_if<u128>(&value.value()) != nullptr)
                pattern = std::make_shared<ptrn::PatternUnsigned>(this, 0, sizeof(u128));
            else if (std::get_if<i128>(&value.value()) != nullptr)
                pattern = std::make_shared<ptrn::PatternSigned>(this, 0, sizeof(i128));
            else if (std::get_if<double>(&value.value()) != nullptr)
                pattern = std::make_shared<ptrn::PatternFloat>(this, 0, sizeof(double));
            else if (std::get_if<bool>(&value.value()) != nullptr)
                pattern = std::make_shared<ptrn::PatternBoolean>(this, 0);
            else if (std::get_if<char>(&value.value()) != nullptr)
                pattern = std::make_shared<ptrn::PatternCharacter>(this, 0);
            else if (auto string = std::get_if<std::string>(&value.value()); string != nullptr)
                pattern = std::make_shared<ptrn::PatternString>(this, 0, string->size());
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

            if (sectionId == ptrn::Pattern::HeapSectionId) {
                pattern->setOffset(heapAddress << 32);
                this->getHeap()[heapAddress].resize(pattern->getSize());
            } else if (sectionId == ptrn::Pattern::PatternLocalSectionId) {
                pattern->setSection(sectionId);
                pattern->setOffset(u64(patternLocalAddress) << 32);
                this->m_patternLocalStorage[patternLocalAddress].data.resize(pattern->getSize());
            }
        }

        pattern->setReference(reference);
        pattern->setConstant(constant);

        if (outVariable) {
            if (this->isGlobalScope())
                this->m_outVariables[name] = pattern->clone();
            else
                err::E0003.throwError("Out variables can only be declared in the global scope.", {}, type);
        }

        if (this->isDebugModeEnabled())
            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Creating local variable '{} {}' at heap address 0x{:X}.", pattern->getTypeName(), pattern->getVariableName(), pattern->getOffset()));

        if (templateVariable)
            this->m_templateParameters.back().push_back(pattern);
        else
            this->getScope(0).scope->push_back(pattern);
        return pattern;
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
        return std::visit(wolv::util::overloaded {
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
               else if (dynamic_cast<const ptrn::PatternCharacter*>(pattern) || dynamic_cast<const ptrn::PatternWideCharacter*>(pattern))
                   return truncateValue(pattern->getSize(), u128(value));
               else if (dynamic_cast<const ptrn::PatternString*>(pattern))
                   return Token::Literal(value).toString(false);
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

    std::shared_ptr<ptrn::Pattern>& Evaluator::getVariableByName(const std::string &name) {
        // Search for variable in current scope
        {
            auto &variables = *this->getScope(0).scope;
            for (auto &variable : variables) {
                if (variable->getVariableName() == name) {
                    return variable;
                }
            }
        }

        // Search for variable in the template parameter list
        {
            auto &variables = this->m_templateParameters.back();
            for (auto &variable : variables) {
                if (variable->getVariableName() == name) {
                    return variable;
                }
            }
        }

        // If there's no variable with that name in the current scope, search the global scope
        auto &variables = *this->getGlobalScope().scope;
        for (auto &variable : variables) {
            if (variable->getVariableName() == name) {
                return variable;
            }
        }

        err::E0003.throwError(fmt::format("Cannot find variable '{}' in this scope.", name));
    }

    void Evaluator::setVariable(const std::string &name, const Token::Literal &value) {
        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return;

        auto pattern = [&]() -> std::shared_ptr<ptrn::Pattern> {
            auto& variablePattern = this->getVariableByName(name);

            if (!variablePattern->isLocal() && !variablePattern->isReference())
                err::E0011.throwError(fmt::format("Cannot modify global variable '{}' as it has been placed in memory.", name));

            // If the variable is being set to a pattern, adjust its layout to the real layout as it potentially contains dynamically sized members
            std::visit(wolv::util::overloaded {
                [&](ptrn::Pattern * const value) {
                    variablePattern = value->clone();

                    variablePattern->setVariableName(name);
                    variablePattern->setReference(true);
                },
                [&](const std::string &value) {
                    if (dynamic_cast<ptrn::PatternString*>(variablePattern.get()) != nullptr)
                        variablePattern->setSize(value.size());
                    else
                        err::E0004.throwError(fmt::format("Cannot assign value of type 'string' to variable of type '{}'.", variablePattern->getTypeName()));
                },
                [](const auto &) {}
            }, value);

            return variablePattern;
        }();

        this->setVariable(pattern.get(), value);
    }

    void Evaluator::setVariable(ptrn::Pattern *pattern, const Token::Literal &value) {
        if (pattern->isConstant() && pattern->isInitialized())
            err::E0011.throwError(fmt::format("Cannot modify constant variable '{}'.", pattern->getVariableName()));
        pattern->setInitialized(true);

        if (pattern->isReference()) {
            if (value.getType() == Token::ValueType::CustomType)
                return;
            else {
                pattern->setReference(false);
                pattern->setLocal(true);
                pattern->setOffset(u64(this->getHeap().size()) << 32);
                this->getHeap().emplace_back().resize(pattern->getSize());
            }
        }

        if (!pattern->isLocal())
            err::E0003.throwError(fmt::format("Cannot assign value to non-local pattern '{}'.", pattern->getVariableName()), {});

        if (pattern->getSize() > 0xFFFF'FFFF)
            err::E0003.throwError(fmt::format("Value is too large to place into local variable '{}'.", pattern->getVariableName()));

        // Cast values to type given by pattern
        Token::Literal castedValue = castLiteral(pattern, value);

        // Write value into variable storage
        {
            bool heapSection = pattern->getSection() == ptrn::Pattern::HeapSectionId;
            bool patternLocalSection = pattern->getSection() == ptrn::Pattern::PatternLocalSectionId;
            auto &storage = [&, this]() -> auto& {
                if (heapSection)
                    return this->getHeap()[pattern->getHeapAddress()];
                else if (patternLocalSection)
                    return this->m_patternLocalStorage[pattern->getHeapAddress()].data;
                else
                    return this->getSection(pattern->getSection());
            }();

            auto copyToStorage = [&](const auto &value) {
                u64 offset = (heapSection || patternLocalSection) ? pattern->getOffset() & 0xFFFF'FFFF : pattern->getOffset();

                if (storage.size() < offset + pattern->getSize())
                    storage.resize(offset + pattern->getSize());
                std::memcpy(storage.data() + offset, &value, pattern->getSize());

                if (this->isDebugModeEnabled())
                    this->getConsole().log(LogConsole::Level::Debug, fmt::format("Setting local variable '{}' to {}.", pattern->getVariableName(), value));
            };

            std::visit(wolv::util::overloaded {
                    [&](const auto &value) {
                        auto adjustedValue = hlp::changeEndianess(value, pattern->getSize(), pattern->getEndian());
                        copyToStorage(adjustedValue);
                    },
                    [&](const i128 &value) {
                        auto adjustedValue = hlp::changeEndianess(value, pattern->getSize(), pattern->getEndian());
                        adjustedValue = hlp::signExtend(pattern->getSize() * 8, adjustedValue);
                        copyToStorage(adjustedValue);
                    },
                    [&](const double &value) {
                        auto adjustedValue = hlp::changeEndianess(value, pattern->getSize(), pattern->getEndian());

                        storage.resize(pattern->getSize());

                        if (storage.size() == sizeof(float)) {
                            copyToStorage(float(adjustedValue));
                        } else {
                            copyToStorage(adjustedValue);
                        }
                    },
                    [&](const std::string &value) {
                        pattern->setSize(value.size());
                        copyToStorage(value[0]);
                    },
                    [&, this](ptrn::Pattern * const value) {
                        if (heapSection || patternLocalSection) {
                            storage.resize(pattern->getSize());
                            this->readData(value->getOffset(), storage.data(), value->getSize(), value->getSection());
                        } else if (storage.size() < pattern->getOffset() + pattern->getSize()) {
                            storage.resize(pattern->getOffset() + pattern->getSize());
                            this->readData(value->getOffset(), storage.data() + pattern->getOffset(), value->getSize(), value->getSection());
                        }


                        if (this->isDebugModeEnabled())
                            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Setting local variable '{}' to {:02X}.", pattern->getVariableName(), fmt::join(storage, " ")));
                    }
            }, castedValue);
        }
    }

    void Evaluator::setVariableAddress(const std::string &variableName, u64 address, u64 section) {
        if (section == ptrn::Pattern::HeapSectionId)
            err::E0005.throwError(fmt::format("Cannot place variable '{}' in heap.", variableName));

        auto variable = this->getVariableByName(variableName);

        variable->setOffset(address);
        variable->setSection(section);
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

    std::vector<ast::ASTNode*> unpackCompoundStatements(const std::vector<std::unique_ptr<ast::ASTNode>> &nodes) {
        std::vector<ast::ASTNode*> result;

        for (const auto &node : nodes) {
            if (auto compoundStatement = dynamic_cast<ast::ASTNodeCompoundStatement*>(node.get()); compoundStatement != nullptr) {
                auto unpacked = unpackCompoundStatements(compoundStatement->getStatements());

                std::move(unpacked.begin(), unpacked.end(), std::back_inserter(result));
            } else {
                result.push_back(node.get());
            }
        }

        return result;
    }

    void Evaluator::accessData(u64 address, void *buffer, size_t size, u64 sectionId, bool write) {
        if (size == 0 || buffer == nullptr)
            return;

        if (sectionId == ptrn::Pattern::HeapSectionId) {
            auto &heap = this->getHeap();

            auto heapAddress = (address >> 32);
            auto storageAddress = address & 0xFFFF'FFFF;
            if (heapAddress < heap.size()) {
                auto &storage = heap[heapAddress];

                if (storageAddress + size > storage.size()) {
                    storage.resize(storageAddress + size);
                }

                if (!write)
                    std::memcpy(buffer, storage.data() + storageAddress, size);
                else
                    std::memcpy(storage.data() + storageAddress, buffer, size);
            }
            else
                err::E0011.throwError(fmt::format("Tried accessing out of bounds heap cell {}. This is a bug.", heapAddress));
        } else if (sectionId == ptrn::Pattern::PatternLocalSectionId) {
            auto &patternLocal = this->m_patternLocalStorage;

            auto heapAddress = (address >> 32);
            auto storageAddress = address & 0xFFFF'FFFF;
            if (heapAddress < patternLocal.size()) {
                auto &storage = patternLocal[heapAddress].data;

                if (storageAddress + size > storage.size()) {
                    storage.resize(storageAddress + size);
                }

                if (!write)
                    std::memcpy(buffer, storage.data() + storageAddress, size);
                else
                    std::memcpy(storage.data() + storageAddress, buffer, size);
            }
            else
                err::E0011.throwError(fmt::format("Tried accessing out of bounds pattern local storage cell {}. This is a bug.", heapAddress));
        } else if (sectionId == ptrn::Pattern::MainSectionId) {
            if (!write) {
                if (address < this->m_dataBaseAddress + this->m_dataSize)
                    this->m_readerFunction(address, reinterpret_cast<u8*>(buffer), size);
                else
                    std::memset(buffer, 0x00, size);
            } else {
                if (address < this->m_dataBaseAddress + this->m_dataSize)
                    this->m_writerFunction(address, reinterpret_cast<u8*>(buffer), size);
            }
        } else {
            if (this->m_sections.contains(sectionId)) {
                auto &section = this->m_sections[sectionId];

                if (!write) {
                    if ((address + size) <= section.data.size())
                        std::memcpy(buffer, section.data.data() + address, size);
                    else
                        std::memset(buffer, 0x00, size);
                } else {
                    if ((address + size) <= section.data.size())
                        std::memcpy(section.data.data() + address, buffer, size);
                }
            } else
                err::E0012.throwError(fmt::format("Tried accessing a non-existing section with id {}.", sectionId));
        }

        if (this->isDebugModeEnabled())
            this->m_console.log(LogConsole::Level::Debug, fmt::format("{} {} bytes from address 0x{:02X} in section {:02X}", write ? "Writing" : "Reading", size, address, sectionId));
    }

    void Evaluator::pushSectionId(u64 id) {
        this->m_sectionIdStack.push_back(id);
    }

    void Evaluator::popSectionId() {
        this->m_sectionIdStack.pop_back();
    }

    [[nodiscard]] u64 Evaluator::getSectionId() const {
        if (this->m_sectionIdStack.empty())
            return 0;

        return this->m_sectionIdStack.back();
    }

    u64 Evaluator::createSection(const std::string &name) {
        auto id = this->m_sectionId;
        this->m_sectionId++;

        this->m_sections.insert({ id, { name, { } } });
        return id;
    }

    void Evaluator::removeSection(u64 id) {
        this->m_sections.erase(id);
    }

    std::vector<u8>& Evaluator::getSection(u64 id) {
        if (id == ptrn::Pattern::MainSectionId)
            err::E0011.throwError("Cannot access main section.");
        else if (id == ptrn::Pattern::HeapSectionId)
            return this->m_heap.back();
        else if (this->m_sections.contains(id))
            return this->m_sections[id].data;
        else
            err::E0011.throwError(fmt::format("Tried accessing a non-existing section with id {}.", id));
    }

    const std::map<u64, api::Section> &Evaluator::getSections() const {
        return this->m_sections;
    }

    u64 Evaluator::getSectionCount() const {
        return this->m_sections.size();
    }

    bool Evaluator::evaluate(const std::string &sourceCode, const std::vector<std::shared_ptr<ast::ASTNode>> &ast) {
        this->m_sections.clear();
        this->m_sectionIdStack.clear();
        this->m_sectionId = 1;
        this->m_outVariables.clear();

        this->m_scopes.clear();
        this->m_heap.clear();
        this->m_patternLocalStorage.clear();
        this->m_templateParameters.clear();

        this->m_customFunctions.clear();
        this->m_patterns.clear();

        this->m_mainResult.reset();
        this->m_colorIndex = 0;
        this->m_aborted = false;
        this->m_evaluated = false;

        if (this->m_allowDangerousFunctions == DangerousFunctionPermission::Deny)
            this->m_allowDangerousFunctions = DangerousFunctionPermission::Ask;

        ON_SCOPE_EXIT {
            this->m_envVariables.clear();
            this->m_evaluated = true;
        };

        this->m_currPatternCount = 0;

        this->m_customFunctionDefinitions.clear();

        if (this->isDebugModeEnabled())
            this->m_console.log(LogConsole::Level::Debug, fmt::format("Base Pattern size: 0x{:02X} bytes", sizeof(ptrn::Pattern)));

        try {
            this->setCurrentControlFlowStatement(ControlFlowStatement::None);
            this->pushScope(nullptr, this->m_patterns);
            this->pushTemplateParameters();

            for (auto &topLevelNode : ast) {
                std::vector<ast::ASTNode*> nodes;
                if (auto compoundNode = dynamic_cast<ast::ASTNodeCompoundStatement*>(topLevelNode.get()))
                    nodes = unpackCompoundStatements(compoundNode->getStatements());
                else
                    nodes.push_back(topLevelNode.get());

                for (auto node : nodes) {
                    if (node == nullptr)
                        continue;

                    auto startOffset = this->dataOffset();

                    if (dynamic_cast<ast::ASTNodeTypeDecl *>(node) != nullptr) {
                        // Don't create patterns from type declarations
                    } else if (dynamic_cast<ast::ASTNodeFunctionDefinition *>(node) != nullptr) {
                        this->m_customFunctionDefinitions.push_back(node->evaluate(this));
                    } else if (auto varDeclNode = dynamic_cast<ast::ASTNodeVariableDecl *>(node); varDeclNode != nullptr) {
                        bool localVariable = varDeclNode->getPlacementOffset() == nullptr;

                        if (localVariable)
                            this->pushSectionId(ptrn::Pattern::HeapSectionId);

                        for (auto &pattern : varDeclNode->createPatterns(this)) {
                            if (localVariable) {
                                auto name = pattern->getVariableName();
                                wolv::util::unused(varDeclNode->execute(this));

                                if (varDeclNode->isInVariable() && this->m_inVariables.contains(name))
                                    this->setVariable(name, this->m_inVariables[name]);

                                this->dataOffset() = startOffset;
                            } else {
                                this->m_patterns.push_back(std::move(pattern));
                            }

                            if (this->getCurrentControlFlowStatement() == ControlFlowStatement::Return)
                                break;
                        }

                        if (localVariable)
                            this->popSectionId();
                    } else if (auto arrayVarDeclNode = dynamic_cast<ast::ASTNodeArrayVariableDecl *>(node); arrayVarDeclNode != nullptr) {
                        bool localVariable = arrayVarDeclNode->getPlacementOffset() == nullptr;

                        if (localVariable)
                            this->pushSectionId(ptrn::Pattern::HeapSectionId);

                        for (auto &pattern : arrayVarDeclNode->createPatterns(this)) {
                            if (localVariable) {
                                wolv::util::unused(arrayVarDeclNode->execute(this));

                                this->dataOffset() = startOffset;
                            } else {
                                this->m_patterns.push_back(std::move(pattern));
                            }
                        }

                        if (localVariable)
                            this->popSectionId();
                    } else if (auto pointerVarDecl = dynamic_cast<ast::ASTNodePointerVariableDecl *>(node); pointerVarDecl != nullptr) {
                        for (auto &pattern : pointerVarDecl->createPatterns(this)) {
                            if (pointerVarDecl->getPlacementOffset() == nullptr) {
                                err::E0003.throwError("Pointers cannot be used as local variables.");
                            } else {
                                this->m_patterns.push_back(std::move(pattern));
                            }
                        }
                    } else {
                        this->pushSectionId(ptrn::Pattern::HeapSectionId);
                        wolv::util::unused(node->execute(this));
                        this->popSectionId();
                    }

                    if (this->getCurrentControlFlowStatement() == ControlFlowStatement::Return)
                        goto stop_evaluation;
                    else
                        this->setCurrentControlFlowStatement(ControlFlowStatement::None);
                }

                this->getScope(0).savedPatterns.clear();
            }

            stop_evaluation:

            if (!this->m_mainResult.has_value() && this->m_customFunctions.contains("main")) {
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

    void Evaluator::updateRuntime(const ast::ASTNode *node) {
        if (this->m_evaluated)
            return;

        this->handleAbort();

        auto line = node->getLine();
        if (this->m_breakpoints.contains(line)) {
            if (this->m_lastPauseLine != line) {
                this->m_lastPauseLine = line;
                this->m_breakpointHitCallback();
            }
        } else {
            this->m_lastPauseLine.reset();
        }
    }

    void Evaluator::addBreakpoint(u64 line) { this->m_breakpoints.insert(line); }
    void Evaluator::removeBreakpoint(u64 line) { this->m_breakpoints.erase(line); }
    void Evaluator::clearBreakpoints() { this->m_breakpoints.clear(); }
    void Evaluator::setBreakpointHitCallback(const std::function<void()> &callback) { this->m_breakpointHitCallback = callback; }
    const std::unordered_set<int> &Evaluator::getBreakpoints() const { return this->m_breakpoints; }

    std::optional<u32> Evaluator::getPauseLine() const {
        return this->m_lastPauseLine;
    }

    void Evaluator::patternCreated(ptrn::Pattern *pattern) {
        wolv::util::unused(pattern);

        if (this->m_currPatternCount > this->m_patternLimit && !this->m_evaluated)
            err::E0007.throwError(fmt::format("Pattern count exceeded set limit of '{}'.", this->getPatternLimit()), "If this is intended, try increasing the limit using '#pragma pattern_limit <new_limit>'.");
        this->m_currPatternCount++;

        if (pattern->isPatternLocal()) {
            if (auto it = this->m_patternLocalStorage.find(pattern->getHeapAddress()); it != this->m_patternLocalStorage.end()) {
                auto &[key, data] = *it;

                data.referenceCount++;
            }
        }
    }

    void Evaluator::patternDestroyed(ptrn::Pattern *pattern) {
        this->m_currPatternCount--;

        if (pattern->isPatternLocal()) {
            if (auto it = this->m_patternLocalStorage.find(pattern->getHeapAddress()); it != this->m_patternLocalStorage.end()) {
                auto &[key, data] = *it;

                data.referenceCount--;
                if (data.referenceCount == 0)
                    this->m_patternLocalStorage.erase(it);
            }
        }
    }

}