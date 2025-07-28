#include "TypeExtractorAction.h"
#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendPluginRegistry.h>

class FooConsumer : public clang::ASTConsumer {};

// HACK: This technically creates a diamond inheritance situation, and
//  it's why we need to explictly define `CreateASTConsumer`, but this
//  allows for us to encapsulate our ASTConsumer in a shared library.
class TypeExtractorPluginAction : public TypeExtractorAction,
                                  public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI,
                    llvm::StringRef file) override {
    return TypeExtractorAction::CreateASTConsumer(CI, file);
  }
  bool ParseArgs(const clang::CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

static clang::FrontendPluginRegistry::Add<TypeExtractorPluginAction>
    X("type-extractor", "extract type information from CXX headers");
