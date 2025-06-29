#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_array_variable_decl.hpp>
#include <pl/core/ast/ast_node_pointer_variable_decl.hpp>
#include <pl/core/ast/ast_node_function_definition.hpp>
#include <pl/core/ast/ast_node_compound_statement.hpp>
#include <pl/core/ast/ast_node_control_flow_statement.hpp>
#include <pl/core/ast/ast_node_lvalue_assignment.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>

#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_enum.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_padding.hpp>
#include <pl/patterns/pattern_error.hpp>

#include <exception>
#include <utility>
#include "wolv/utils/string.hpp"

namespace pl::core {

    std::map<std::string, Token::Literal> Evaluator::getOutVariables() const {
        return m_outVariableValues;
    }

    void Evaluator::setDataSource(u64 baseAddress, size_t dataSize, std::function<void(u64, u8*, size_t)> readerFunction, std::optional<std::function<void(u64, const u8*, size_t)>> writerFunction) {
        this->m_dataBaseAddress = baseAddress;
        this->m_dataSize = dataSize;

        this->m_readerFunction = [this, readerFunction = std::move(readerFunction)](u64 offset, u8* buffer, size_t size) {
            this->m_lastReadAddress = offset;

            readerFunction(offset, buffer, size);
        };

        if (writerFunction.has_value()) {
            this->m_writerFunction = [this, writerFunction = std::move(writerFunction.value())](u64 offset, const u8* buffer, size_t size) {
                this->m_lastWriteAddress = offset;

                writerFunction(offset, buffer, size);
            };
        }
    }

    void Evaluator::alignToByte() {
        if (m_currBitOffset != 0 && !isReadOrderReversed()) {
            this->m_currOffset += 1;
        }
        this->m_currBitOffset = 0;
    }

    u64 Evaluator::getReadOffset() const {
        return this->m_currOffset;
    }

    void Evaluator::setReadOffset(u64 offset) {
        this->m_currOffset = offset;
        this->m_currBitOffset = 0;
    }

    void Evaluator::setStartAddress(u64 address) {
        this->m_startAddress = address;
    }

    u64 Evaluator::getStartAddress() const {
        return this->m_startAddress;
    }


    [[nodiscard]] ByteAndBitOffset Evaluator::getBitwiseReadOffsetAndIncrement(i128 bitSize) {
        ByteAndBitOffset readOffsets = {};

        if (isReadOrderReversed())
            bitSize = -bitSize;
        else
            readOffsets = getBitwiseReadOffset();

        this->m_currOffset += u64(bitSize >> 3);
        this->m_currBitOffset += i8(bitSize & 0x7);

        this->m_currOffset += this->m_currBitOffset >> 3;
        this->m_currBitOffset &= 0x7;

        if (isReadOrderReversed())
            readOffsets = getBitwiseReadOffset();
        return readOffsets;
    }

    u64 Evaluator::getReadOffsetAndIncrement(u64 incrementSize) {
        alignToByte();

        if (isReadOrderReversed()) {
            this->m_currOffset -= incrementSize;
            return this->m_currOffset;
        }

        auto offset = this->m_currOffset;
        this->m_currOffset += incrementSize;
        return offset;
    }

    [[nodiscard]] u128 Evaluator::readBits(u128 byteOffset, u8 bitOffset, u64 bitSize, u64 section, std::endian endianness) {
        u128 value = 0;

        size_t readSize = (bitOffset + bitSize + 7) / 8;
        readSize = std::min(readSize, sizeof(value));
        this->readData(u64(byteOffset), &value, readSize, section);
        value = hlp::changeEndianess(value, sizeof(value), endianness);

        size_t offset = endianness == std::endian::little ? bitOffset : (sizeof(value) * 8) - bitOffset - bitSize;
        auto mask = hlp::bitmask(bitSize);
        value = (value >> offset) & mask;
        return value;
    }

    void Evaluator::writeBits(u128 byteOffset, u8 bitOffset, u64 bitSize, u64 section, std::endian endianness, u128 value) {
        size_t writeSize = (bitOffset + bitSize + 7) / 8;
        writeSize = std::min(writeSize, sizeof(value));
        value = hlp::changeEndianess(value, writeSize, endianness);

        size_t offset = endianness == std::endian::little ? bitOffset : (sizeof(value) * 8) - bitOffset - bitSize;
        auto mask = hlp::bitmask(bitSize);
        value = (value & mask) << offset;

        u128 oldValue = 0;
        this->readData(u64(byteOffset), &oldValue, writeSize, section);
        oldValue = hlp::changeEndianess(oldValue, sizeof(oldValue), endianness);

        oldValue &= ~(mask << offset);
        oldValue |= value;

        oldValue = hlp::changeEndianess(oldValue, sizeof(oldValue), endianness);
        this->writeData(u64(byteOffset), &oldValue, writeSize, section);
    }



    bool Evaluator::addBuiltinFunction(const std::string &name, api::FunctionParameterCount numParams, std::vector<Token::Literal> defaultParameters, const api::FunctionCallback &function, bool dangerous) {
        const auto [iter, inserted] = this->m_builtinFunctions.insert({
            name, {
                numParams,
                std::move(defaultParameters),
                dangerous ? handleDangerousFunctionCall(name, function) : function
            }
        });

        return inserted;
    }

    api::FunctionCallback Evaluator::handleDangerousFunctionCall(const std::string &functionName, const api::FunctionCallback &function) {
        return [this, function, functionName](core::Evaluator *, const std::vector<core::Token::Literal> &params) -> std::optional<core::Token::Literal> {
            if (getDangerousFunctionPermission() != DangerousFunctionPermission::Allow) {
                dangerousFunctionCalled();

                if (getDangerousFunctionPermission() == DangerousFunctionPermission::Deny) {
                    err::E0009.throwError(fmt::format("Call to dangerous function '{}' has been denied.", functionName), { });
                }
            }

            return function(this, params);
        };
    }

    bool Evaluator::addCustomFunction(const std::string &name, api::FunctionParameterCount numParams, std::vector<Token::Literal> defaultParameters, const api::FunctionCallback &function) {
        const auto [iter, inserted] = this->m_customFunctions.insert({
            name, {numParams, std::move(defaultParameters), function}
        });

        return inserted;
    }

    [[nodiscard]] std::optional<api::Function> Evaluator::findFunction(const std::string &name) const {
        if (name.empty())
            return std::nullopt;

        const auto &customFunctions     = this->getCustomFunctions();
        const auto &builtinFunctions    = this->getBuiltinFunctions();

        if (auto customFunction = customFunctions.find(name); customFunction != customFunctions.end())
            return customFunction->second;
        else if (auto builtinFunction = builtinFunctions.find(name); builtinFunction != builtinFunctions.end())
            return builtinFunction->second;
        else
            return std::nullopt;
    }

    void Evaluator::createParameterPack(const std::string &name, const std::vector<Token::Literal> &values) {
        this->getScope(0).parameterPack = ParameterPack {
            name,
            values
        };
    }

    void Evaluator::createArrayVariable(const std::string &name, const ast::ASTNode *type, size_t entryCount, u64 section, bool constant) {
        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return;

        auto &variables = *this->getScope(0).scope;
        for (auto &variable : variables) {
            if (variable->getVariableName() == name) {
                err::E0003.throwError(fmt::format("Variable with name '{}' already exists in this scope.", name), {}, type->getLocation());
            }
        }

        auto startOffset = this->getBitwiseReadOffset();

        std::vector<std::shared_ptr<ptrn::Pattern>> typePatterns;
        type->createPatterns(this, typePatterns);
        auto typePattern = std::move(typePatterns.front());

        typePattern->setConstant(constant);

        this->setBitwiseReadOffset(startOffset);

        auto pattern = new ptrn::PatternArrayDynamic(this, 0, typePattern->getSize() * entryCount, 0);

        if (section == ptrn::Pattern::PatternLocalSectionId) {
            typePattern->setSection(section);
            std::vector<std::shared_ptr<ptrn::Pattern>> entries;
            for (size_t i = 0; i < entryCount; i++) {
                auto entryPattern = typePattern->clone();
                entryPattern->setSection(section);

                auto patternLocalAddress = this->m_patternLocalStorage.empty() ? 0 : this->m_patternLocalStorage.rbegin()->first + 1;
                entryPattern->setOffset(u64(patternLocalAddress) << 32);
                this->m_patternLocalStorage.insert({ patternLocalAddress, { } });
                this->m_patternLocalStorage[patternLocalAddress].data.resize(entryPattern->getSize());

                entries.push_back(std::move(entryPattern));
            }
            pattern->setEntries(entries);
            pattern->setSection(section);

        } else if (section == ptrn::Pattern::HeapSectionId) {
            typePattern->setLocal(true);
            std::vector<std::shared_ptr<ptrn::Pattern>> entries;
            for (size_t i = 0; i < entryCount; i++) {
                auto entryPattern = typePattern->clone();
                entryPattern->setLocal(true);

                auto &heap = this->getHeap();
                entryPattern->setOffset(u64(heap.size()) << 32);
                heap.emplace_back();

                entries.push_back(std::move(entryPattern));
            }
            pattern->setEntries(entries);
            pattern->setLocal(true);
        } else {
            typePattern->setSection(section);
            std::vector<std::shared_ptr<ptrn::Pattern>> entries;
            for (size_t i = 0; i < entryCount; i++) {
                auto entryPattern = typePattern->clone();
                entryPattern->setOffset(entryPattern->getSize() * i);
                entries.push_back(std::move(entryPattern));
            }
            pattern->setEntries(entries);
            pattern->setSection(section);
        }

        pattern->setVariableName(name);

        if (this->isDebugModeEnabled())
            this->getConsole().log(LogConsole::Level::Debug, fmt::format("Creating local array variable '{} {}[{}]' at heap address 0x{:X}.", pattern->getTypeName(), pattern->getVariableName(), entryCount, pattern->getOffset()));

        pattern->setConstant(constant);
        variables.push_back(std::unique_ptr<ptrn::Pattern>(pattern));
    }

    std::optional<std::string> Evaluator::findTypeName(const ast::ASTNodeTypeDecl *type) {
        const ast::ASTNodeTypeDecl *typeDecl = type;
        while (true) {
            if (auto name = typeDecl->getName(); !name.empty()) {
                if (const auto &templateParams = typeDecl->getTemplateParameters(); templateParams.empty()) {
                    return name;
                } else {
                    std::string templateTypeString;
                    for (const auto &templateParameter : templateParams) {
                        if (auto lvalue = dynamic_cast<ast::ASTNodeLValueAssignment *>(templateParameter.get())) {
                            if (!lvalue->getRValue())
                                err::E0003.throwError(fmt::format("No value set for non-type template parameter {}. This is a bug.", lvalue->getLValueName()), {}, type->getLocation());
                            auto valueNode = lvalue->getRValue()->evaluate(this);
                            if (auto literal = dynamic_cast<ast::ASTNodeLiteral*>(valueNode.get()); literal != nullptr) {
                                const auto &value = literal->getValue();

                                if (value.isString()) {
                                    auto string = value.toString();
                                    if (string.size() > 32)
                                        string = "...";
                                    templateTypeString += fmt::format("\"{}\", ", hlp::encodeByteString({ string.begin(), string.end() }));
                                } else if (value.isPattern()) {
                                    templateTypeString += fmt::format("{}{{ }}, ", value.toPattern()->getTypeName());
                                } else {
                                    templateTypeString += fmt::format("{}, ", value.toString(true));
                                }
                            } else {
                                err::E0003.throwError(fmt::format("Template parameter {} is not a literal. This is a bug.", lvalue->getLValueName()), {}, type->getLocation());
                            }
                        } else if (const auto *typeNode = dynamic_cast<ast::ASTNodeTypeDecl*>(templateParameter.get())) {
                            const auto *node = typeNode->getType().get();
                            while (node != nullptr) {
                                if (const auto *innerNode = dynamic_cast<const ast::ASTNodeTypeDecl*>(node)) {
                                    if (const auto &innerNodeName = innerNode->getName(); !innerNodeName.empty()) {
                                        templateTypeString += fmt::format("{}, ", innerNodeName);
                                        break;
                                    }
                                    node = innerNode->getType().get();
                                }
                                if (const auto *innerNode = dynamic_cast<const ast::ASTNodeBuiltinType*>(node)) {
                                    templateTypeString += fmt::format("{}, ", Token::getTypeName(innerNode->getType()));

                                    break;
                                }
                            }
                        }
                    }

                    templateTypeString = templateTypeString.substr(0, templateTypeString.size() - 2);

                    return fmt::format("{}<{}>", name, templateTypeString);
                }
            }
            else if (auto innerType = dynamic_cast<ast::ASTNodeTypeDecl*>(typeDecl->getType().get()); innerType != nullptr)
                typeDecl = innerType;
            else
                return std::nullopt;
        }
    }

    /*static ast::ASTNodeBuiltinType* getBuiltinType(const ast::ASTNodeTypeDecl *type) {
        const ast::ASTNodeTypeDecl *typeDecl = type;
        while (true) {
            if (auto innerType = dynamic_cast<ast::ASTNodeTypeDecl*>(typeDecl->getType().get()); innerType != nullptr)
                typeDecl = innerType;
            else if (auto builtinType = dynamic_cast<ast::ASTNodeBuiltinType*>(typeDecl->getType().get()); builtinType != nullptr)
                return builtinType;
            else
                return nullptr;
        }
    }*/

    std::shared_ptr<ptrn::Pattern> Evaluator::createVariable(const std::string &name, const ast::ASTNodeTypeDecl *type, const std::optional<Token::Literal> &value, bool outVariable, bool /*reference*/, bool /*templateVariable*/, bool constant) {
        auto startPos = this->getBitwiseReadOffset();
        ON_SCOPE_EXIT { this->setBitwiseReadOffset(startPos); };

        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return nullptr;

        std::vector<std::shared_ptr<ptrn::Pattern>> results;
        type->createPatterns(this, results);
        if (results.empty()) {
            std::shared_ptr<ptrn::Pattern> pattern = std::make_shared<ptrn::PatternPadding>(this, 0x00, 0x00, 0);
            pattern->setVariableName(name);
            this->setVariable(pattern, value.value());
            results.push_back(pattern);
        }

        auto result = results[0];
        if (outVariable)
            m_outVariables[name] = result;
        if (constant) {
            if (!value.has_value()) {
                err::E0007.throwError("Constant values must be initialized at definition");
            }

            result->setConstant(true);
            this->setVariable(result, value.value());
        }

        result->setLocal(true);
        result->setVariableName(name);
        this->getScope(0).scope->push_back(result);

        return result;
    }

    template<typename T>
    static T truncateValue(size_t bytes, const T &value) {
        T result = { };
        std::memcpy(&result, &value, std::min(sizeof(result), bytes));

        if (std::signed_integral<T>)
            result = hlp::signExtend(bytes * 8, i128(result));

        return result;
    }

    static Token::Literal castLiteral(const std::shared_ptr<ptrn::Pattern> &pattern, const Token::Literal &literal) {
        if (pattern == nullptr)
            return literal;

        const auto pointer = pattern.get();
        return std::visit(wolv::util::overloaded {
            [&](auto &value) -> Token::Literal {
               if (dynamic_cast<const ptrn::PatternUnsigned*>(pointer) || dynamic_cast<const ptrn::PatternEnum*>(pointer))
                   return truncateValue<u128>(pattern->getSize(), u128(value));
               else if (dynamic_cast<const ptrn::PatternSigned*>(pointer))
                   return truncateValue<i128>(pattern->getSize(), i128(value));
               else if (dynamic_cast<const ptrn::PatternFloat*>(pointer)) {
                   if (pattern->getSize() == sizeof(float))
                       return double(float(value));
                   else
                       return double(value);
               } else if (dynamic_cast<const ptrn::PatternBoolean*>(pointer))
                   return value == 0 ? u128(0) : u128(1);
               else if (dynamic_cast<const ptrn::PatternCharacter*>(pointer) || dynamic_cast<const ptrn::PatternWideCharacter*>(pointer))
                   return truncateValue(pattern->getSize(), u128(value));
               else if (dynamic_cast<const ptrn::PatternString*>(pointer))
                   return Token::Literal(value).toString(false);
               else if (dynamic_cast<const ptrn::PatternPadding*>(pointer))
                   return value;
               else
                   err::E0004.throwError(fmt::format("Cannot cast from type 'integer' to type '{}'.", pattern->getTypeName()));
            },
            [&](const std::string &value) -> Token::Literal {
                if (dynamic_cast<const ptrn::PatternUnsigned*>(pointer) != nullptr) {
                    if (value.size() <= pattern->getSize()) {
                        u128 result = 0;
                        std::memcpy(&result, value.data(), value.size());
                        return result;
                    } else {
                        err::E0004.throwError(fmt::format("String of size {} cannot be packed into integer of size {}", value.size(), pattern->getSize()));
                    }
                } else if (dynamic_cast<const ptrn::PatternBoolean*>(pointer) != nullptr)
                    return !value.empty();
                else if (dynamic_cast<const ptrn::PatternString*>(pointer) != nullptr)
                    return value;
                else if (dynamic_cast<const ptrn::PatternPadding*>(pointer) != nullptr)
                    return value;
                else
                    err::E0004.throwError(fmt::format("Cannot cast from type 'string' to type '{}'.", pattern->getTypeName()));
            },
            [&](const std::shared_ptr<ptrn::Pattern>& value) -> Token::Literal {
                if (value->getTypeName() == pattern->getTypeName() || value->getTypeName().empty())
                    return value;
                else
                    err::E0004.throwError(fmt::format("Cannot cast from type '{}' to type '{}'.", value->getTypeName(), pattern->getTypeName()));
            }
        }, literal);
    }

    void Evaluator::changePatternSection(ptrn::Pattern *pattern, u64 section) {
        for (auto &[address, child] : pattern->getChildren()) {
            auto childSection = child->getSection();
            if (childSection == 0) {
                if (!child->isPatternLocal()) {
                    u32 patternLocalAddress = this->m_patternLocalStorage.empty() ? 0 : this->m_patternLocalStorage.rbegin()->first + 1;
                    this->m_patternLocalStorage.insert({ patternLocalAddress, { } });
                }

                child->setSection(section);
            }
        }
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

    void Evaluator::setVariable(const std::string &name, const Token::Literal &variableValue) {
        // A variable named _ gets treated as "don't care"
        if (name == "_")
            return;

        auto &pattern = this->getVariableByName(name);
        this->setVariable(pattern, variableValue);
    }

    void Evaluator::changePatternType(std::shared_ptr<ptrn::Pattern> &pattern, std::shared_ptr<ptrn::Pattern> &&newPattern) const {
        if (dynamic_cast<ptrn::PatternPadding*>(pattern.get()) == nullptr)
            return;

        auto section = pattern->getSection();
        auto offset = pattern->getOffset();
        auto variableName = pattern->getVariableName();

        pattern = std::move(newPattern);

        pattern->setSection(section);
        pattern->setOffset(offset);
        pattern->setVariableName(variableName);
    }

    template<std::derived_from<ptrn::Pattern> T>
    static std::shared_ptr<ptrn::Pattern> initializeLocalPattern(Evaluator *evaluator, const auto &value) {
        std::shared_ptr<ptrn::Pattern> pattern;
        if constexpr (requires { T(evaluator, 0x00, sizeof(value), 0); })
            pattern = std::make_shared<T>(evaluator, 0x00, sizeof(value), 0);
        else
            pattern = std::make_shared<T>(evaluator, 0x00, 0);

        pattern->setLocal(true);
        evaluator->writeData(pattern->getOffset(), &value, sizeof(value), pattern->getSection());

        return pattern;
    }

    void Evaluator::setVariable(std::shared_ptr<ptrn::Pattern> &pattern, const Token::Literal &variableValue) {
        auto startPos = this->getBitwiseReadOffset();
        ON_SCOPE_EXIT { this->setBitwiseReadOffset(startPos); };

        auto boxedValue =
            std::visit(wolv::util::overloaded {
                [this](char value) -> std::shared_ptr<ptrn::Pattern> {
                    return initializeLocalPattern<ptrn::PatternCharacter>(this, value);
                },
                [this](bool value) -> std::shared_ptr<ptrn::Pattern> {
                    return initializeLocalPattern<ptrn::PatternBoolean>(this, value);
                },
                [this](u128 value) -> std::shared_ptr<ptrn::Pattern> {
                    return initializeLocalPattern<ptrn::PatternUnsigned>(this, value);
                },
                [this](i128 value) -> std::shared_ptr<ptrn::Pattern> {
                    return initializeLocalPattern<ptrn::PatternSigned>(this, value);
                },
                [this](double value) -> std::shared_ptr<ptrn::Pattern> {
                    return initializeLocalPattern<ptrn::PatternFloat>(this, value);
                },
                [this](std::string value) -> std::shared_ptr<ptrn::Pattern> {
                    auto pattern = std::make_shared<ptrn::PatternString>(this, 0x00, value.size(), 0);
                    pattern->setLocal(true);
                    this->writeData(pattern->getOffset(), value.data(), value.size(), pattern->getSection());
                    return pattern;
                },
                [&](std::shared_ptr<ptrn::Pattern> value) -> std::shared_ptr<ptrn::Pattern> {
                    if (pattern->isReference())
                        return value;
                    else {
                        auto result = value->clone();
                        result->setLocal(true);
                        return result;
                    }
                }
            }, castLiteral(pattern, variableValue));

        boxedValue->setVariableName(pattern->getVariableName());

        if (auto parent = pattern->getParent(); parent != nullptr) {
            if (auto iterable = dynamic_cast<ptrn::IIterable*>(parent); iterable != nullptr) {
                iterable->replaceEntry(pattern, boxedValue);
            }
        }

        pattern = std::move(boxedValue);
    }

    void Evaluator::setVariableAddress(const std::string &variableName, u64 address, u64 section) {
        if (section == ptrn::Pattern::HeapSectionId)
            err::E0005.throwError(fmt::format("Cannot place variable '{}' in heap.", variableName));

        auto variable = this->getVariableByName(variableName);

        variable->setLocal(false);
        variable->setOffset(address);
        variable->setSection(section);
    }

    void Evaluator::pushScope(const std::shared_ptr<ptrn::Pattern> &parent, std::vector<std::shared_ptr<ptrn::Pattern>> &scope) {
        if (this->m_scopes.size() > this->getEvaluationDepth())
            err::E0007.throwError(fmt::format("Evaluation depth exceeded set limit of '{}'.", this->getEvaluationDepth()), "If this is intended, try increasing the limit using '#pragma eval_depth <new_limit>'.");

        this->handleAbort();

        const auto &heap = this->getHeap();

        this->m_scopes.emplace_back(std::make_unique<Scope>(parent, &scope, heap.size()));

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

    std::vector<ast::ASTNode*> unpackCompoundStatements(const std::vector<std::shared_ptr<ast::ASTNode>> &nodes) {
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

        if (sectionId == ptrn::Pattern::MainSectionId) [[likely]] {
            if (!write) [[likely]] {
                this->m_readerFunction(address, static_cast<u8*>(buffer), size);
            } else {
                if (address < this->m_dataBaseAddress + this->m_dataSize)
                    this->m_writerFunction(address, static_cast<u8*>(buffer), size);
            }
        } else if (sectionId == ptrn::Pattern::HeapSectionId) {
            auto *patternData = reinterpret_cast<std::vector<u8>*>(address);
            if (!write) {
                std::memmove(buffer, patternData->data(), size);
            } else {
                if (patternData->size() != size)
                    patternData->resize(size);
                std::memmove(patternData->data(), buffer, size);
            }
        } else if (sectionId == ptrn::Pattern::PatternLocalSectionId) {
            auto &patternLocal = this->m_patternLocalStorage;

            auto heapAddress = (address >> 32);
            auto storageAddress = address & 0xFFFF'FFFF;
            if (patternLocal.contains(heapAddress)) {
                auto &storage = patternLocal[heapAddress].data;

                if (storageAddress + size > storage.size()) {
                    storage.resize(storageAddress + size);
                }

                if (!write)
                    std::memmove(buffer, storage.data() + storageAddress, size);
                else
                    std::memmove(storage.data() + storageAddress, buffer, size);
            }
            else
                err::E0011.throwError(fmt::format("Tried accessing out of bounds pattern local storage cell {}. This is a bug.", heapAddress));
        } else if (sectionId == ptrn::Pattern::InstantiationSectionId) {
            err::E0012.throwError("Cannot access data of type that hasn't been placed in memory.");
        } else {
            if (this->m_sections.contains(sectionId)) {
                auto &section = this->m_sections[sectionId];

                if (!write) {
                    if ((address + size) <= section.data.size())
                        std::memmove(buffer, section.data.data() + address, size);
                    else
                        std::memset(buffer, 0x00, size);
                } else {
                    if ((address + size) <= section.data.size())
                        std::memmove(section.data.data() + address, buffer, size);
                }
            } else
                err::E0012.throwError(fmt::format("Tried accessing a non-existing section with id {}.", sectionId));
        }

        if (this->isDebugModeEnabled()) [[unlikely]]
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
        else if (id == ptrn::Pattern::InstantiationSectionId)
            err::E0012.throwError("Cannot access data of type that hasn't been placed in memory.");
        else
            err::E0011.throwError(fmt::format("Tried accessing a non-existing section with id {}.", id));
    }

    u64 Evaluator::getSectionSize(u64 id) {
        if (id == ptrn::Pattern::MainSectionId)
            return this->getDataSize();
        else
            return this->getSection(id).size();
    }

    const std::map<u64, api::Section> &Evaluator::getSections() const {
        return this->m_sections;
    }

    u64 Evaluator::getSectionCount() const {
        return this->m_sections.size();
    }

    bool Evaluator::evaluate(const std::vector<std::shared_ptr<ast::ASTNode>> &ast) {
        this->m_readOrderReversed = false;
        this->m_currBitOffset = 0;

        this->m_sections.clear();
        this->m_sectionIdStack.clear();
        this->m_sectionId = 1;
        this->m_outVariables.clear();
        this->m_outVariableValues.clear();

        this->m_customFunctions.clear();
        this->m_patterns.clear();

        this->m_scopes.clear();
        this->m_callStack.clear();
        this->m_heap.clear();
        this->m_patternLocalStorage.clear();
        this->m_templateParameters.clear();
        this->m_stringPool.clear();

        this->m_mainResult.reset();
        this->m_aborted = false;
        this->m_evaluated = false;
        this->m_attributedPatterns.clear();

        this->setPatternColorPalette(DefaultPatternColorPalette);

        if (this->m_allowDangerousFunctions == DangerousFunctionPermission::Deny)
            this->m_allowDangerousFunctions = DangerousFunctionPermission::Ask;

        ON_SCOPE_EXIT {
            this->m_envVariables.clear();
            this->m_evaluated = true;
            this->m_mainSectionEditsAllowed = false;

            for (const auto &[name, pattern] : this->m_outVariables) {
                this->m_outVariableValues.insert({ name, pattern->getValue() });
            }
        };

        this->m_currPatternCount = 0;

        this->m_customFunctionDefinitions.clear();

        if (this->isDebugModeEnabled())
            this->m_console.log(LogConsole::Level::Debug, fmt::format("Base Pattern size: 0x{:02X} bytes", sizeof(ptrn::Pattern)));

        this->m_sourceLineLength.clear();
        for (auto &topLevelNode : ast) {
            if (topLevelNode->getLocation().source->mainSource) {
                std::vector<std::string> sourceLines = wolv::util::splitString(topLevelNode->getLocation().source->content, "\n");
                for (const auto &sourceLine : sourceLines) {
                    this->m_sourceLineLength.push_back(sourceLine.size());
                }
                break;
            }
        }
        this->m_lastPauseLine = std::nullopt;

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

                    auto startOffset = this->getBitwiseReadOffset();

                    if (dynamic_cast<ast::ASTNodeTypeDecl *>(node) != nullptr) {
                        // Don't create patterns from type declarations
                    } else if (dynamic_cast<ast::ASTNodeFunctionDefinition *>(node) != nullptr) {
                        this->m_customFunctionDefinitions.push_back(node->evaluate(this));
                    } else if (auto varDeclNode = dynamic_cast<ast::ASTNodeVariableDecl *>(node); varDeclNode != nullptr) {
                        bool localVariable = varDeclNode->getPlacementOffset() == nullptr;

                        if (localVariable)
                            this->pushSectionId(ptrn::Pattern::HeapSectionId);

                        std::vector<std::shared_ptr<ptrn::Pattern>> patterns;

                        ON_SCOPE_EXIT {
                            for (auto &pattern : patterns) {
                                if (localVariable) {
                                    auto name = pattern->getVariableName();
                                    wolv::util::unused(varDeclNode->execute(this));

                                    this->setBitwiseReadOffset(startOffset);
                                } else {
                                    this->m_patterns.push_back(std::move(pattern));
                                }

                                if (this->getCurrentControlFlowStatement() == ControlFlowStatement::Return)
                                    break;
                            }

                            {
                                auto name = varDeclNode->getName();
                                if (varDeclNode->isInVariable() && this->m_inVariables.contains(name))
                                    this->setVariable(name, this->m_inVariables[name]);
                            }

                            if (localVariable)
                                this->popSectionId();
                        };

                        varDeclNode->createPatterns(this, patterns);

                    } else if (auto arrayVarDeclNode = dynamic_cast<ast::ASTNodeArrayVariableDecl *>(node); arrayVarDeclNode != nullptr) {
                        bool localVariable = arrayVarDeclNode->getPlacementOffset() == nullptr;

                        if (localVariable)
                            this->pushSectionId(ptrn::Pattern::HeapSectionId);

                        std::vector<std::shared_ptr<ptrn::Pattern>> patterns;

                        ON_SCOPE_EXIT {
                            for (auto &pattern : patterns) {
                                if (localVariable) {
                                    wolv::util::unused(arrayVarDeclNode->execute(this));

                                    this->setBitwiseReadOffset(startOffset);
                                } else {
                                    this->m_patterns.push_back(std::move(pattern));
                                }
                            }

                            if (localVariable)
                                this->popSectionId();
                        };

                        arrayVarDeclNode->createPatterns(this, patterns);
                    } else if (auto pointerVarDecl = dynamic_cast<ast::ASTNodePointerVariableDecl *>(node); pointerVarDecl != nullptr) {
                        std::vector<std::shared_ptr<ptrn::Pattern>> patterns;

                        ON_SCOPE_EXIT {
                            for (auto &pattern : patterns) {
                                if (pointerVarDecl->getPlacementOffset() == nullptr) {
                                    err::E0003.throwError("Pointers cannot be used as local variables.");
                                } else {
                                    this->m_patterns.push_back(std::move(pattern));
                                }
                            }
                        };

                        pointerVarDecl->createPatterns(this, patterns);
                    } else if (auto controlFlowStatement = dynamic_cast<ast::ASTNodeControlFlowStatement *>(node); controlFlowStatement != nullptr) {
                        this->pushSectionId(ptrn::Pattern::HeapSectionId);
                        auto result = node->execute(this);
                        this->popSectionId();

                        if (result.has_value()) {
                            this->m_mainResult = result;
                        }

                        goto stop_evaluation;
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
            }

            stop_evaluation:

            if (!this->m_mainResult.has_value() && this->m_customFunctions.contains("main")) {
                auto mainFunction = this->m_customFunctions["main"];

                if (mainFunction.parameterCount.max > 0)
                    err::E0009.throwError("Entry point function 'main' may not have any parameters.");

                this->m_mainResult = mainFunction.func(this, {});
            }
        } catch (err::EvaluatorError::Exception &e) {

            const auto location = e.getUserData();

            this->getConsole().setHardError(err::PatternLanguageError(e.format(location), location.line, location.column, this->getReadOffset()));

            return false;
        }

        return true;
    }

    Evaluator::UpdateHandler::UpdateHandler(Evaluator *evaluator, const ast::ASTNode *node) : evaluator(evaluator) {
        if (evaluator->m_evaluated)
            return;

        evaluator->handleAbort();

        if (node != nullptr) {
            auto rawLine = node->getLocation().line;
            const auto line = rawLine + (rawLine == 0);
            auto rawColumn = node->getLocation().column;
            const auto column = rawColumn + (rawColumn == 0);
            if (const auto source = node->getLocation().source; source != nullptr && source->mainSource) {
                if (evaluator->m_lastPauseLine != line && column < evaluator->m_sourceLineLength[line - 1]) {
                    if (evaluator->m_shouldPauseNextLine || evaluator->m_breakpoints.contains(line)) {
                        if (evaluator->m_shouldPauseNextLine)
                            evaluator->m_shouldPauseNextLine = false;
                        evaluator->m_lastPauseLine = line;
                        evaluator->m_breakpointHitCallback();
                    } else if (!evaluator->m_breakpoints.contains(line))
                        evaluator->m_lastPauseLine = std::nullopt;
                }
            }
            evaluator->m_callStack.emplace_back(node->clone(), evaluator->getReadOffset());
        }
    }

    Evaluator::UpdateHandler::~UpdateHandler() {
        if (evaluator->m_evaluated)
            return;

        // Don't pop scopes if an exception is currently being thrown so we can generate
        // a stack trace
        if (std::uncaught_exceptions() > 0)
            return;

        evaluator->m_callStack.pop_back();
    }

    Evaluator::UpdateHandler Evaluator::updateRuntime(const ast::ASTNode *node) {
        return { this, node };
    }

    void Evaluator::addBreakpoint(u32 line) { this->m_breakpoints.insert(line); }
    void Evaluator::removeBreakpoint(u32 line) { this->m_breakpoints.erase(line); }
    void Evaluator::clearBreakpoints() { this->m_breakpoints.clear(); }
    void Evaluator::setBreakpointHitCallback(const std::function<void()> &callback) { this->m_breakpointHitCallback = callback; }
    const std::unordered_set<u32> &Evaluator::getBreakpoints() const { return this->m_breakpoints; }
    void Evaluator::setBreakpoints(const std::unordered_set<u32> &breakpoints) { m_breakpoints = breakpoints; }
    void Evaluator::pauseNextLine() { this->m_shouldPauseNextLine = true; }

    std::optional<u32> Evaluator::getPauseLine() const {
        return this->m_lastPauseLine;
    }

    void Evaluator::patternCreated(ptrn::Pattern *pattern) {
        this->m_lastPatternAddress = pattern->getOffset();

        if (this->m_patternLimit > 0 && this->m_currPatternCount > this->m_patternLimit && !this->m_evaluated)
            err::E0007.throwError(fmt::format("Pattern count exceeded set limit of '{}'.", this->getPatternLimit()), "If this is intended, try increasing the limit using '#pragma pattern_limit <new_limit>'.");
        this->m_currPatternCount += 1;

        // Make sure we don't throw an error if we're already in an error state
        if (std::uncaught_exceptions() != 0)
            return;

        if (pattern->isPatternLocal()) {
            if (auto it = this->m_patternLocalStorage.find(pattern->getHeapAddress()); it != this->m_patternLocalStorage.end()) {
                auto &[key, data] = *it;

                data.referenceCount++;
            } else {
                this->m_patternLocalStorage[pattern->getHeapAddress()] = { 1, {} };
            }
        }
    }

    void Evaluator::patternDestroyed(ptrn::Pattern *pattern) {
        this->m_currPatternCount -= 1;

        // Make sure we don't throw an error if we're already in an error state
        if (std::uncaught_exceptions() != 0)
            return;

        const auto &attributes = pattern->getAttributes();
        if (attributes != nullptr) {
            for (const auto &[attribute, args] : *attributes) {
                this->removeAttributedPattern(attribute, pattern);
            }
        }

        if (pattern->isPatternLocal()) {
            if (auto it = this->m_patternLocalStorage.find(pattern->getHeapAddress()); it != this->m_patternLocalStorage.end()) {
                auto &[key, data] = *it;

                data.referenceCount--;
                if (data.referenceCount == 0)
                    this->m_patternLocalStorage.erase(it);
            } else if (!this->m_evaluated) {
                err::E0001.throwError(fmt::format("Double free of variable named '{}'.", pattern->getVariableName()));
            }
        }
    }

}