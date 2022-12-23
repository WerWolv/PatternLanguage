#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

#include <pl/patterns/pattern_pointer.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>


namespace pl::core::ast {

    class ASTNodeAttribute : public ASTNode {
    public:
        explicit ASTNodeAttribute(std::string attribute, std::unique_ptr<ASTNode> value = nullptr)
            : ASTNode(), m_attribute(std::move(attribute)), m_value(std::move(value)) { }

        ~ASTNodeAttribute() override = default;

        ASTNodeAttribute(const ASTNodeAttribute &other) : ASTNode(other) {
            this->m_attribute = other.m_attribute;
            if (other.m_value != nullptr)
                this->m_value     = other.m_value->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeAttribute(*this));
        }

        [[nodiscard]] const std::string &getAttribute() const {
            return this->m_attribute;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getValue() const {
            return this->m_value;
        }

    private:
        std::string m_attribute;
        std::unique_ptr<ASTNode> m_value;
    };


    class Attributable {
    protected:
        Attributable() = default;

        Attributable(const Attributable &other) {
            for (auto &attribute : other.m_attributes) {
                auto copy = attribute->clone();
                if (auto node = dynamic_cast<ASTNodeAttribute *>(copy.get())) {
                    this->m_attributes.push_back(std::unique_ptr<ASTNodeAttribute>(node));
                    (void)copy.release();
                }
            }
        }

    public:
        virtual void addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) {
            this->m_attributes.push_back(std::move(attribute));
        }

        [[nodiscard]] const auto &getAttributes() const {
            return this->m_attributes;
        }

        [[nodiscard]] bool hasAttribute(const std::string &key, bool needsParameter) const {
            return std::any_of(this->m_attributes.begin(), this->m_attributes.end(), [&](const std::unique_ptr<ASTNodeAttribute> &attribute) {
                if (attribute->getAttribute() == key) {
                    if (needsParameter && attribute->getValue() == nullptr)
                        err::E0008.throwError(fmt::format("Attribute '{}' expected a parameter.", key), fmt::format("Try [[{}(\"value\")]] instead.", key), attribute.get());
                    else if (!needsParameter && attribute->getValue() != nullptr)
                        err::E0008.throwError(fmt::format("Attribute '{}' did not expect a parameter.", key), fmt::format("Try [[{}]] instead.", key), attribute.get());
                    else
                        return true;
                }

                return false;
            });
        }

        [[nodiscard]] std::shared_ptr<ASTNode> getAttributeValue(const std::string &key) const {
            auto attribute = std::find_if(this->m_attributes.begin(), this->m_attributes.end(), [&](const std::unique_ptr<ASTNodeAttribute> &attribute) {
                return attribute->getAttribute() == key;
            });

            if (attribute != this->m_attributes.end())
                return (*attribute)->getValue()->clone();
            else
                return nullptr;
        }

        [[nodiscard]] std::shared_ptr<ASTNode> getFirstAttributeValue(const std::vector<std::string> &keys) const {
            for (const auto &key : keys) {
                if (auto value = this->getAttributeValue(key); value != nullptr)
                    return value;
            }

            return nullptr;
        }

    private:
        std::vector<std::unique_ptr<ASTNodeAttribute>> m_attributes;
    };

    namespace {

        std::string getAttributeValueAsString(const auto &value, Evaluator *evaluator) {
            auto literalNode = value->evaluate(evaluator);
            auto literal = static_cast<ASTNodeLiteral*>(literalNode.get());

            return Token::literalToString(literal->getValue(), true);
        }

    }

    inline void applyTypeAttributes(Evaluator *evaluator, const ASTNode *node, ptrn::Pattern *pattern) {
        auto attributable = dynamic_cast<const Attributable *>(node);
        if (attributable == nullptr)
            err::E0008.throwError("Attributes cannot be applied to this statement.", {}, node);

        if (attributable->hasAttribute("inline", false)) {
            auto inlinable = dynamic_cast<ptrn::Inlinable *>(pattern);

            if (inlinable == nullptr)
                err::E0008.throwError("[[inline]] attribute can only be used with nested types.", "Try applying it to a struct, union, bitfield or array instead.", node);
            else
                inlinable->setInlined(true);
        }

        if (auto value = attributable->getFirstAttributeValue({ "format", "format_read" }); value) {
            auto functionName = getAttributeValueAsString(value, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Formatter function '{}' does not exist.", functionName), {}, node);

            if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                err::E0009.throwError(fmt::format("Formatter function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pattern->getTypeName()), node);

            pattern->setReadFormatterFunction(functionName);
        }

        if (auto value = attributable->getAttributeValue("format_write"); value) {
            auto functionName = getAttributeValueAsString(value, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Formatter function '{}' does not exist.", functionName), {}, node);

            if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                err::E0009.throwError(fmt::format("Formatter function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pattern->getTypeName()), node);

            pattern->setWriteFormatterFunction(functionName);
        }

        if (auto value = attributable->getAttributeValue("format_entries"); value) {
            auto functionName = getAttributeValueAsString(value, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Formatter function '{}' does not exist.", functionName), {}, node);

            if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                err::E0009.throwError(fmt::format("Formatter function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pattern->getTypeName()), node);

            auto array = dynamic_cast<ptrn::PatternArrayDynamic *>(pattern);
            if (array == nullptr)
                err::E0009.throwError("The [[inline_array]] attribute can only be applied to dynamic array types.", {}, node);

            for (const auto &entry : array->getEntries()) {
                entry->setReadFormatterFunction(functionName);
            }
        }

        if (auto value = attributable->getAttributeValue("transform"); value) {
            auto functionName = getAttributeValueAsString(value, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Formatter function '{}' does not exist.", functionName), {}, node);

            if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                err::E0009.throwError(fmt::format("Transform function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pattern->getTypeName()), node);

            pattern->setTransformFunction(functionName);
        }

        if (auto value = attributable->getAttributeValue("pointer_base"); value) {
            auto functionName = getAttributeValueAsString(value, evaluator);
            auto function = evaluator->findFunction(functionName);
            if (!function.has_value())
                err::E0009.throwError(fmt::format("Pointer base function '{}' does not exist.", functionName), {}, node);


            if (auto pointerPattern = dynamic_cast<ptrn::PatternPointer *>(pattern)) {
                i128 pointerValue = pointerPattern->getPointedAtAddress();

                if (function->parameterCount != api::FunctionParameterCount::exactly(1))
                    err::E0009.throwError(fmt::format("Transform function '{}' needs to take exactly one parameter.", functionName), fmt::format("Try 'fn {}({} value)' instead", functionName, pointerPattern->getPointerType()->getTypeName()), node);

                auto result = function->func(evaluator, { pointerValue });

                if (!result.has_value())
                    err::E0009.throwError(fmt::format("Pointer base function '{}' did not return a value.", functionName), "Try adding a 'return <value>;' statement in all code paths.", node);

                pointerPattern->rebase(Token::literalToSigned(result.value()));
            } else {
                err::E0009.throwError("The [[pointer_base]] attribute can only be applied to pointer types.", {}, node);
            }
        }

        if (attributable->hasAttribute("hidden", false)) {
            pattern->setHidden(true);
        }

        if (attributable->hasAttribute("sealed", false)) {
            pattern->setSealed(true);
        }

        if (!pattern->hasOverriddenColor()) {
            if (auto colorValue = attributable->getAttributeValue("color"); colorValue) {
                auto colorString = getAttributeValueAsString(colorValue, evaluator);
                u32 color = strtoul(colorString.c_str(), nullptr, 16);
                pattern->setColor(hlp::changeEndianess(color, std::endian::big) >> 8);
            } else if (auto singleColor = attributable->hasAttribute("single_color", false); singleColor) {
                pattern->setColor(pattern->getColor());
            }
        }

        for (const auto &attribute : attributable->getAttributes()) {
            if (const auto &value = attribute->getValue(); value != nullptr)
                pattern->addAttribute(attribute->getAttribute(), getAttributeValueAsString(value, evaluator));
            else
                pattern->addAttribute(attribute->getAttribute());
        }
    }

    inline void applyVariableAttributes(Evaluator *evaluator, const ASTNode *node, ptrn::Pattern *pattern) {
        auto attributable = dynamic_cast<const Attributable *>(node);
        if (attributable == nullptr)
            err::E0008.throwError("Attributes cannot be applied to this statement.", {}, node);

        auto endOffset          = evaluator->dataOffset();
        evaluator->dataOffset() = pattern->getOffset();
        PL_ON_SCOPE_EXIT { evaluator->dataOffset() = endOffset; };

        applyTypeAttributes(evaluator, node, pattern);

        if (auto colorValue = attributable->getAttributeValue("color"); colorValue) {
            auto colorString = getAttributeValueAsString(colorValue, evaluator);
            u32 color = strtoul(colorString.c_str(), nullptr, 16);
            pattern->setColor(hlp::changeEndianess(color, std::endian::big) >> 8);
        } else if (auto singleColor = attributable->hasAttribute("single_color", false); singleColor) {
            pattern->setColor(pattern->getColor());
        }

        if (auto value = attributable->getAttributeValue("name"); value) {
            pattern->setDisplayName(getAttributeValueAsString(value, evaluator));
        }

        if (auto value = attributable->getAttributeValue("comment"); value) {
            pattern->setComment(getAttributeValueAsString(value, evaluator));
        }

        if (attributable->hasAttribute("no_unique_address", false)) {
            endOffset -= pattern->getSize();
        }
    }

}
