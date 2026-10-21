#include <pl/cli/subcommands/docs.hpp>

#include <pl/core/ast/ast_node_bitfield.hpp>
#include <pl/core/ast/ast_node_enum.hpp>
#include <pl/core/ast/ast_node_struct.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_union.hpp>
#include <pl/core/ast/visitors/ast_stringify.hpp>

#include <fmt/format.h>

namespace pl::cli::sub::docs {

    std::string getTypeName(const core::ast::ASTNode *type) {
        return core::ast::ASTStringifier().stringifyTypeName(type);
    }

    std::string generateAttributes(const core::ast::Attributable *attributable) {
        const auto &attributes = attributable->getAttributes();
        if (attributes.empty())
            return "";

        std::string result = " [[";
        core::ast::ASTStringifier stringifier;
        for (const auto &attribute : attributes) {
            result += attribute->getAttribute();

            const auto &arguments = attribute->getArguments();
            if (!arguments.empty()) {
                result += "(";
                for (size_t i = 0; i < arguments.size(); i++) {
                    if (i > 0)
                        result += ", ";
                    result += stringifier.stringifyExpression(arguments[i].get());
                }
                result += ")";
            }

            result += ", ";
        }

        result.pop_back();
        result.pop_back();

        return result + "]]";
    }

    std::string generateTemplateParams(const core::ast::ASTNodeTypeDecl *type) {
        const auto &templateParams = type->getTemplateParameters();
        if (templateParams.empty())
            return "";

        std::string result = "<";
        for (const auto &templateParam : templateParams) {
            if (templateParam->isType())
                result += templateParam->getName().get();
            else
                result += fmt::format("auto {}", templateParam->getName().get());

            result += ", ";
        }

        result.pop_back();
        result.pop_back();

        return result + ">";
    }

    std::string generateTypeDocumentation(const std::string &name, const core::ast::ASTNodeTypeDecl *type) {
        if (!type->isValid())
            return "";

        if (auto typeApp = dynamic_cast<core::ast::ASTNodeTypeApplication*>(type->getType().get())) {
            auto underlyingType = core::ast::ASTStringifier().stringifyTypeName(typeApp, &type->getTemplateParameters());
            return fmt::format("```pl\nusing {}{} = {}{};\n```", name, generateTemplateParams(type), underlyingType, generateAttributes(type));
        } else if (dynamic_cast<core::ast::ASTNodeStruct*>(type->getType().get())) {
            return fmt::format("```pl\nstruct {}{} {{ ... }}{};\n```", name, generateTemplateParams(type), generateAttributes(type));
        } else if (dynamic_cast<core::ast::ASTNodeUnion*>(type->getType().get())) {
            return fmt::format("```pl\nunion {}{} {{ ... }}{};\n```", name, generateTemplateParams(type), generateAttributes(type));
        } else if (dynamic_cast<core::ast::ASTNodeBitfield*>(type->getType().get())) {
            return fmt::format("```pl\nbitfield {}{} {{ ... }}{};\n```", name, generateTemplateParams(type), generateAttributes(type));
        } else if (auto enumDecl = dynamic_cast<core::ast::ASTNodeEnum*>(type->getType().get())) {
            auto result = fmt::format("```pl\nenum {}{} : {} {{\n", name, generateTemplateParams(type), getTypeName(enumDecl->getUnderlyingType().get()));
            for (auto &[enumValueName, enumValues] : enumDecl->getEntries()) {
                result += fmt::format("    {},\n", enumValueName);
            }

            result.pop_back();
            result.pop_back();

            return result + fmt::format("\n}}{};\n```", generateAttributes(type));
        } else {
            return "";
        }
    }

}
