#pragma once

#include <string>

namespace CLI { class App; }

namespace pl { class PatternLanguage; }

namespace pl::core::ast {
    class ASTNode;
    class ASTNodeTypeApplication;
    class ASTNodeTypeDecl;
    class ASTNodeBuiltinType;
    class ASTNodeStruct;
    class ASTNodeUnion;
    class ASTNodeBitfield;
    class ASTNodeEnum;
    class Attributable;
}

namespace pl::cli::sub {

    void addDocsSubcommand(CLI::App *app, pl::PatternLanguage &runtime);

    namespace docs {

        std::string getTypeName(const core::ast::ASTNode *type);
        std::string generateAttributes(const core::ast::Attributable *attributable);
        std::string generateTemplateParams(const core::ast::ASTNodeTypeDecl *type);
        std::string generateTypeDocumentation(const std::string &name, const core::ast::ASTNodeTypeDecl *type);

    }

}
