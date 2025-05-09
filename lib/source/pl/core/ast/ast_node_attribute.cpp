#include <pl/core/ast/ast_node_attribute.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

#include <pl/patterns/pattern_pointer.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_bitfield.hpp>

#include <bit>

namespace pl::core::ast {

    ASTNodeAttribute::ASTNodeAttribute(std::string attribute, std::vector<std::unique_ptr<ASTNode>> &&value, std::string aliasNamespaceString, std::string autoNamespace)
        : ASTNode(), m_attribute(std::move(attribute)), m_value(std::move(value)), m_aliasNamespaceString(std::move(aliasNamespaceString)), m_autoNamespace(std::move(autoNamespace)) { }

    ASTNodeAttribute::ASTNodeAttribute(const ASTNodeAttribute &other) : ASTNode(other) {
        this->m_attribute = other.m_attribute;

        for (const auto &value : other.m_value)
            this->m_value.emplace_back(value->clone());

        this->m_autoNamespace = other.m_autoNamespace;
        this->m_aliasNamespaceString = other.m_aliasNamespaceString;
    }



    Attributable::Attributable(const Attributable &other) {
        for (auto &attribute : other.m_attributes) {
            auto copy = attribute->clone();
            if (auto node = dynamic_cast<ASTNodeAttribute *>(copy.get())) {
                this->m_attributes.push_back(std::unique_ptr<ASTNodeAttribute>(node));
                (void)copy.release();
            }
        }
    }

    void Attributable::addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) {
        this->m_attributes.push_back(std::move(attribute));
    }

    const std::vector<std::unique_ptr<ASTNodeAttribute>>& Attributable::getAttributes() const {
        return this->m_attributes;
    }

    [[nodiscard]] ASTNodeAttribute *Attributable::getAttributeByName(const std::string &key) const {
        auto it = std::find_if(this->getAttributes().begin(), this->getAttributes().end(), [&](const std::unique_ptr<ASTNodeAttribute> &attribute) {
            return attribute->getAttribute() == key;
        });
        if (it == this->getAttributes().end())
            return nullptr;
        return it->get();
    }

    [[nodiscard]] ASTNodeAttribute *Attributable::getFirstAttributeByName(const std::vector<std::string> &keys) const {
        for (const auto &key : keys) {
            if (auto result = getAttributeByName(key); result != nullptr)
                return result;
        }

        return nullptr;
    }

    [[nodiscard]] bool Attributable::hasAttribute(const std::string &key, bool needsParameter) const {
        return std::any_of(this->m_attributes.begin(), this->m_attributes.end(), [&](const std::unique_ptr<ASTNodeAttribute> &attribute) {
            if (attribute->getAttribute() == key) {
                if (needsParameter && attribute->getArguments().empty())
                    err::E0008.throwError(fmt::format("Attribute '{}' expected a parameter.", key), fmt::format("Try [[{}(\"value\")]] instead.", key), attribute->getLocation());
                else if (!needsParameter && !attribute->getArguments().empty())
                    err::E0008.throwError(fmt::format("Attribute '{}' did not expect a parameter.", key), fmt::format("Try [[{}]] instead.", key), attribute->getLocation());
                else
                    return true;
            }

            return false;
        });
    }

    [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>>& Attributable::getAttributeArguments(const std::string &key) const {
        auto *attribute = this->getAttributeByName(key);
        if (attribute == nullptr) {
            static std::vector<std::unique_ptr<ASTNode>> empty;
            return empty;
        }
        return attribute->getArguments();
    }

    [[nodiscard]] std::string Attributable::getFirstAttributeAutoNamespace(const std::vector<std::string> &keys) const {
        auto attribute = this->getFirstAttributeByName(keys);
        if (attribute == nullptr)
            return "";
        else
            return attribute->getAutoNamespace();
    }

    [[nodiscard]] std::string Attributable::getFirstAttributeAliasNamespace(const std::vector<std::string> &keys) const {
        auto attribute = this->getFirstAttributeByName(keys);
        if (attribute == nullptr)
            return "";
        else
            return attribute->getAliasNamespaceString();
    }

    [[nodiscard]] std::shared_ptr<ASTNode> Attributable::getFirstAttributeValue(const std::vector<std::string> &keys) const {
        for (const auto &key : keys) {
            if (const auto &arguments = this->getAttributeArguments(key); !arguments.empty())
                return arguments.front()->clone();
        }

        return nullptr;
    }

    [[nodiscard]] std::vector<std::string> Attributable::getAttributeKeys() const {
        std::vector<std::string> result;
        for (const auto &attribute : this->m_attributes) {
            result.push_back(attribute->getAttribute());
        }

        return result;
    }


    namespace {

        std::string getAttributeValueAsString(const auto &value, Evaluator *evaluator) {
            auto literalNode = value->evaluate(evaluator);
            auto literal = static_cast<ASTNodeLiteral*>(literalNode.get());

            return literal->getValue().toString(true);
        }

        u128 getAttributeValueAsInteger(const auto &value, Evaluator *evaluator) {
            auto literalNode = value->evaluate(evaluator);
            auto literal = static_cast<ASTNodeLiteral*>(literalNode.get());

            return literal->getValue().toUnsigned();
        }

        std::string getAttributeValueAsFunctionName(const auto &value, const Attributable *attributable, Evaluator *evaluator) {
            auto literalNode = value->evaluate(evaluator);
            auto literal = static_cast<ASTNodeLiteral*>(literalNode.get());

            auto result = literal->getValue().toString(true);

            const auto attributeKeys = attributable->getAttributeKeys();
            auto autoNamespace = attributable->getFirstAttributeAutoNamespace(attributeKeys);
            auto aliasNamespace = attributable->getFirstAttributeAliasNamespace(attributeKeys);
            if (result.starts_with(autoNamespace)) {
                result = aliasNamespace + result.substr(autoNamespace.size());
            }

            return result;
        }

    }

    void applyTypeAttributes(Evaluator *evaluator, const ASTNode *node, const std::shared_ptr<ptrn::Pattern> &pattern) {
        if (std::uncaught_exceptions() > 0)
            return;

        auto attributable = dynamic_cast<const Attributable *>(node);
        if (attributable == nullptr)
            err::E0008.throwError("Attributes cannot be applied to this statement.", {}, node->getLocation());

        if (attributable->hasAttribute("inline", false)) {
            auto inlinable = dynamic_cast<ptrn::IInlinable *>(pattern.get());

            if (inlinable == nullptr)
                err::E0008.throwError("[[inline]] attribute can only be used with nested types.", "Try applying it to a struct, union, bitfield or array instead.", node->getLocation());
            else
                inlinable->setInlined(true);
        }

        if (attributable->hasAttribute("merge", false)) {
            auto inlinable = dynamic_cast<ptrn::IInlinable *>(pattern.get());

            if (inlinable == nullptr)
                err::E0008.throwError("[[merge]] attribute can only be used with nested types.", "Try applying it to a struct, union, bitfield or array instead.", node->getLocation());
            else {
                auto child = dynamic_cast<ptrn::IIterable*>(pattern.get());
                auto &currScope = evaluator->getScope(0);

                if (currScope.parent == pattern)

                for (const auto& entry : child->getEntries()) {
                    for (auto &existingPattern : *currScope.scope) {
                        if (entry->hasVariableName() && existingPattern->getVariableName() == entry->getVariableName()) {
                            err::E0008.throwError(fmt::format("Cannot merge '{}' from Type '{}' into current scope. Pattern with this name already exists.", entry->getVariableName(), pattern->getTypeName()), "", node->getLocation());
                        }
                    }

                    if (entry->hasAttribute("private"))
                        continue;

                    currScope.scope->emplace_back(entry);
                }

                child->setEntries({});
                pattern->setVisibility(ptrn::Visibility::Hidden);
            }
        }

        if (auto value = attributable->getFirstAttributeValue({ "format", "format_read" }); value) {
            auto functionName = getAttributeValueAsFunctionName(value, attributable, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Formatter function '{}' does not exist.", functionName), {}, node->getLocation());

            if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                err::E0009.throwError(fmt::format("Formatter function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pattern->getTypeName()), node->getLocation());

            pattern->setReadFormatterFunction(functionName);
        }

        if (const auto &arguments = attributable->getAttributeArguments("format_write"); arguments.size() == 1) {
            auto functionName = getAttributeValueAsFunctionName(arguments.front(), attributable, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Formatter function '{}' does not exist.", functionName), {}, node->getLocation());

            if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                err::E0009.throwError(fmt::format("Formatter function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pattern->getTypeName()), node->getLocation());

            pattern->setWriteFormatterFunction(functionName);
        }

        if (const auto &value = attributable->getFirstAttributeValue({ "format_entries", "format_read_entries" }); value) {
            auto functionName = getAttributeValueAsFunctionName(value, attributable, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Formatter function '{}' does not exist.", functionName), {}, node->getLocation());

            if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                err::E0009.throwError(fmt::format("Formatter function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pattern->getTypeName()), node->getLocation());

            auto array = dynamic_cast<ptrn::PatternArrayDynamic *>(pattern.get());
            if (array == nullptr)
                err::E0009.throwError("The [[format_read_entries]] attribute can only be applied to dynamic array types.", {}, node->getLocation());

            for (const auto &entry : array->getEntries()) {
                entry->setReadFormatterFunction(functionName);
            }
        }

        if (const auto &arguments = attributable->getAttributeArguments("format_write_entries"); arguments.size() == 1) {
            auto functionName = getAttributeValueAsFunctionName(arguments.front(), attributable, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Formatter function '{}' does not exist.", functionName), {}, node->getLocation());

            if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                err::E0009.throwError(fmt::format("Formatter function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pattern->getTypeName()), node->getLocation());

            auto array = dynamic_cast<ptrn::PatternArrayDynamic *>(pattern.get());
            if (array == nullptr)
                err::E0009.throwError("The [[format_write_entries]] attribute can only be applied to dynamic array types.", {}, node->getLocation());

            for (const auto &entry : array->getEntries()) {
                entry->setWriteFormatterFunction(functionName);
            }
        }

        if (const auto &arguments = attributable->getAttributeArguments("transform"); arguments.size() == 1) {
            auto functionName = getAttributeValueAsFunctionName(arguments.front(), attributable, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Transform function '{}' does not exist.", functionName), {}, node->getLocation());

            if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                err::E0009.throwError(fmt::format("Transform function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pattern->getTypeName()), node->getLocation());

            pattern->setTransformFunction(functionName);
        }

        if (const auto &arguments = attributable->getAttributeArguments("transform_entries"); arguments.size() == 1) {
            auto functionName = getAttributeValueAsFunctionName(arguments.front(), attributable, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Transform function '{}' does not exist.", functionName), {}, node->getLocation());

            if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                err::E0009.throwError(fmt::format("Transform function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pattern->getTypeName()), node->getLocation());

            auto array = dynamic_cast<ptrn::PatternArrayDynamic *>(pattern.get());
            if (array == nullptr)
                err::E0009.throwError("The [[transform_entries]] attribute can only be applied to dynamic array types.", {}, node->getLocation());

            for (const auto &entry : array->getEntries()) {
                entry->setTransformFunction(functionName);
            }
        }

        if (const auto &arguments = attributable->getAttributeArguments("pointer_base"); arguments.size() == 1) {
            auto functionName = getAttributeValueAsFunctionName(arguments.front(), attributable, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Pointer base function '{}' does not exist.", functionName), {}, node->getLocation());


            if (auto pointerPattern = dynamic_cast<ptrn::PatternPointer *>(pattern.get())) {
                i128 pointerValue = pointerPattern->getPointedAtAddress();

                if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                    err::E0009.throwError(fmt::format("Pointer base function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pointerPattern->getPointerType()->getTypeName()), node->getLocation());

                auto result = function->func(evaluator, { pointerValue });

                if (!result.has_value())
                    err::E0009.throwError(fmt::format("Pointer base function '{}' did not return a value.", functionName), "Try adding a 'return <value>;' statement in all code paths.", node->getLocation());

                pointerPattern->rebase(u64(result.value().toSigned()));
            } else {
                err::E0009.throwError("The [[pointer_base]] attribute can only be applied to pointer types.", {}, node->getLocation());
            }
        }

        if (attributable->hasAttribute("hidden", false)) {
            pattern->setVisibility(ptrn::Visibility::Hidden);
        }

        if (attributable->hasAttribute("highlight_hidden", false)) {
            pattern->setVisibility(ptrn::Visibility::HighlightHidden);
        }

        if (attributable->hasAttribute("tree_hidden", false)) {
            pattern->setVisibility(ptrn::Visibility::TreeHidden);
        }

        if (attributable->hasAttribute("sealed", false)) {
            pattern->setSealed(true);
        }

        if (const auto &arguments = attributable->getAttributeArguments("fixed_size"); arguments.size() == 1) {
            auto requestedSize = size_t(getAttributeValueAsInteger(arguments.front(), evaluator));
            auto actualSize = pattern->getSize();
            if (actualSize < requestedSize) {
                pattern->setSize(requestedSize);
                evaluator->setReadOffset(pattern->getOffset() + requestedSize);
            }
            else if (actualSize > requestedSize)
                err::E0004.throwError("Type size larger than expected", fmt::format("Pattern of type {} is larger than expected. Expected size {}, got {}", pattern->getTypeName(), requestedSize, actualSize), node->getLocation());

        }

        if (!pattern->hasOverriddenColor()) {
            if (const auto &arguments = attributable->getAttributeArguments("color"); arguments.size() == 1) {
                auto colorString = getAttributeValueAsString(arguments.front(), evaluator);
                u32 color = strtoul(colorString.c_str(), nullptr, 16);
                pattern->setColor(hlp::changeEndianess(color, std::endian::big) >> 8);
            } else if (auto singleColor = attributable->hasAttribute("single_color", false); singleColor) {
                pattern->setColor(pattern->getColor());
            }
        }

        for (const auto &attribute : attributable->getAttributes()) {
            if (const auto &arguments = attribute->getArguments(); !arguments.empty()) {
                std::vector<core::Token::Literal> evaluatedArguments;
                for (const auto &argument : arguments) {
                    auto evaluatedArgument = argument->evaluate(evaluator);
                    if (auto literalNode = dynamic_cast<ASTNodeLiteral*>(evaluatedArgument.get()); literalNode != nullptr)
                        evaluatedArguments.push_back(literalNode->getValue());
                }

                pattern->addAttribute(attribute->getAttribute(), evaluatedArguments);
            }
            else
                pattern->addAttribute(attribute->getAttribute());
        }
    }

    void applyVariableAttributes(Evaluator *evaluator, const ASTNode *node, const std::shared_ptr<ptrn::Pattern> &pattern) {
        if (std::uncaught_exceptions() > 0)
            return;

        auto attributable = dynamic_cast<const Attributable *>(node);
        if (attributable == nullptr)
            err::E0008.throwError("Attributes cannot be applied to this statement.", {}, node->getLocation());

        auto endOffset = evaluator->getBitwiseReadOffset();
        evaluator->setReadOffset(pattern->getOffset());
        ON_SCOPE_EXIT {
            if (attributable->hasAttribute("no_unique_address", false)) {
                if (auto bitfieldPattern = dynamic_cast<const ptrn::PatternBitfieldField*>(pattern.get()); bitfieldPattern != nullptr) {
                    evaluator->setBitwiseReadOffset(ByteAndBitOffset {
                        .byteOffset = bitfieldPattern->getOffset(),
                        .bitOffset = bitfieldPattern->getBitOffset()
                    });
                }
            } else if (attributable->hasAttribute("fixed_size", true)) {
                // read offset might be modified by fixed_size's padding. keep it as is.
            } else {
                evaluator->setBitwiseReadOffset(endOffset);
            }
        };

        auto thisScope = evaluator->getScope(0).scope;
        evaluator->pushScope(pattern, *thisScope);
        ON_SCOPE_EXIT {
            evaluator->popScope();
        };

        applyTypeAttributes(evaluator, node, pattern);

        if (const auto &arguments = attributable->getAttributeArguments("color"); arguments.size() == 1) {
            auto colorString = getAttributeValueAsString(arguments.front(), evaluator);
            u32 color = strtoul(colorString.c_str(), nullptr, 16);
            pattern->setColor(hlp::changeEndianess(color, std::endian::big) >> 8);
        } else if (auto singleColor = attributable->hasAttribute("single_color", false); singleColor) {
            pattern->setColor(pattern->getColor());
        }

        if (const auto &arguments = attributable->getAttributeArguments("name"); arguments.size() == 1) {
            pattern->setDisplayName(getAttributeValueAsString(arguments.front(), evaluator));
        }

        if (const auto &arguments = attributable->getAttributeArguments("comment"); arguments.size() == 1) {
            pattern->setComment(getAttributeValueAsString(arguments.front(), evaluator));
        }
    }

}
