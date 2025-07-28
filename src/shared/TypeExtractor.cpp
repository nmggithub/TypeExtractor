#include "TypeExtractorAction.h"
#include "json.h"
#include "util.h"
#include <clang/AST/RecordLayout.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <vector>

// No Windows support yet.
#if defined(_WIN32) || defined(_WIN64)
#error "Windows support is not implemented yet."
#endif

llvm::StringRef getDeclFilePath(clang::Decl *D) {
  clang::SourceLocation loc = D->getLocation();
  clang::SourceManager &SM = D->getASTContext().getSourceManager();
  clang::SourceLocation expansionLoc = SM.getExpansionLoc(loc);
  llvm::StringRef filePath = SM.getFilename(expansionLoc);
  if (llvm::sys::path::is_relative(filePath)) {
    const clang::FileEntry *FE =
        SM.getFileEntryForID(SM.getFileID(expansionLoc));
    if (FE) {
      llvm::StringRef absPath = FE->tryGetRealPathName();
      return absPath;
    }
    return "unknown.h"; // Fallback to a pseudo-filename if we can't resolve it.
  }
  return filePath;
}

static llvm::StringRef sysroot;
static llvm::StringRef resourceDir;

enum class FileRoot { Sysroot, ResourceDir, Unknown };

// Convert FileRoot to a short usable root path.
llvm::StringRef fileRootToPseudoRoot(FileRoot root) {
  llvm::StringRef unknownRoot = "Unknown";
  switch (root) {
  case FileRoot::Sysroot:
    return "Sysroot";
  case FileRoot::ResourceDir:
    return "ResourceDir";
  case FileRoot::Unknown:
    return unknownRoot;
  }
  return unknownRoot; // Fallback, should not be reached.
}

std::pair<FileRoot, llvm::StringRef>
getRelativeFilePathIfRelevant(llvm::StringRef filePath) {
  if (filePath.starts_with(sysroot)) {
    return std::make_pair(FileRoot::Sysroot,
                          filePath.drop_front(sysroot.size()));
  }
  if (filePath.starts_with(resourceDir)) {
    return std::make_pair(FileRoot::ResourceDir,
                          filePath.drop_front(resourceDir.size()));
  }
  return std::make_pair(FileRoot::Unknown, filePath);
}

std::map<llvm::StringRef, std::vector<std::string>> pathComponentsMap;
std::set<std::string> processedDeclIDs;

bool emitTypeDecl(clang::NamedDecl *D, bool parseAnyway = false) {
  auto preEmitIfRecordDecl = [](clang::QualType &QT,
                                DeclIDAndTypeName &declIDAndTypeName) {
    if (auto RD = QT->getAsRecordDecl()) {
      // Ensure the record type is emitted before other types that reference it.
      emitTypeDecl(RD, true);
      // If the type is an anonymous struct or union, we use an empty name.
      declIDAndTypeName.second =
          RD->isAnonymousStructOrUnion() ? "" : RD->getNameAsString();
    }
  };

  // Skip if the declaration is not valid or should not be parsed.
  if ((!D || !shouldParseDecl(D)) && !parseAnyway) {
    return true; // Ignore invalid or unparseable declarations
  }

  // == Common setup for all declarations ==

  // Get the declaration ID and type name.
  auto declIDAsString = std::to_string(D->getID());
  if (processedDeclIDs.contains(declIDAsString)) {
    return true; // Skip if we've already processed this declaration ID.
  }
  processedDeclIDs.insert(declIDAsString);
  auto name = D->getNameAsString();
  DeclIDAndTypeName currentDeclIDAndTypeName = {
      std::make_optional(declIDAsString), name};

  // Handle the file path and ensure it's absolute.
  auto absoluteFilePath = getDeclFilePath(D);
  if (!llvm::sys::path::is_absolute(absoluteFilePath)) {
    return true; // Somehow we got a relative path, skip it.
  }
  auto [fileRoot, filePath] = getRelativeFilePathIfRelevant(absoluteFilePath);
  auto pseudoRoot = fileRootToPseudoRoot(fileRoot).str();

  // Get the file path components, caching them if necessary.
  std::vector<std::string> filePathComponents;
  if (pathComponentsMap.contains(filePath)) {
    // Use cached components if available.
    filePathComponents = pathComponentsMap[filePath];
  } else {
    // Get the file path and determine its root.
    // Remove the root path.
    filePath = filePath.drop_front(llvm::sys::path::root_path(filePath).size());
    // Get the components of the file path, ignoring empty components.
    for (llvm::StringRef component :
         llvm::make_range(llvm::sys::path::begin(filePath),
                          llvm::sys::path::end(filePath))) {
      if (!component.empty()) {
        filePathComponents.push_back(component.str());
      }
    }
    // Cache the components for future use.
    pathComponentsMap[filePath] = filePathComponents;
  }

  // == Kind-specific declaration handling ==

  // Typedef declaration handling.
  if (auto TD = llvm::dyn_cast<clang::TypedefDecl>(D)) {
    auto underlyingType = TD->getUnderlyingType();
    DeclIDAndTypeName underlyingDeclIDAndTypeName =
        typeToDeclIDAndTypeName(underlyingType);
    preEmitIfRecordDecl(underlyingType, underlyingDeclIDAndTypeName);

    auto typeName = underlyingType.getAsString();
    auto json =
        json_typdef_decl(currentDeclIDAndTypeName, underlyingDeclIDAndTypeName,
                         pseudoRoot, filePathComponents);
    llvm::outs() << json << "\n";
    return true;
  }

  // Record declaration handling.
  else if (auto RD = llvm::dyn_cast<clang::RecordDecl>(D)) {
    auto isUnion = RD->isUnion();
    auto isStruct = RD->isStruct();
    if (!RD->isCompleteDefinition()) {
      // Emit empty struct for forward declarations.
      std::vector<std::pair<std::string, StructField>> emptyFields;
      auto json = json_struct_decl(currentDeclIDAndTypeName, emptyFields,
                                   pseudoRoot, filePathComponents);
      llvm::outs() << json << "\n";
      return true;
    }
    if (!isUnion && !isStruct) {
      // Not a union or struct, skip.
      return true; // Ignore non-union/struct record declarations.
    }

    currentDeclIDAndTypeName.second =
        RD->isAnonymousStructOrUnion() ? "" : RD->getNameAsString();
    ;

    if (isUnion) {
      // Ordering doesn't really matter for unions, but nice to have.
      OrderedTypesMap fields;
      for (const auto *field : RD->fields()) {
        auto fieldType = field->getType();
        auto fieldDeclIDAndTypeName = typeToDeclIDAndTypeName(fieldType);
        preEmitIfRecordDecl(fieldType, fieldDeclIDAndTypeName);
        fields.emplace_back(field->getNameAsString(), fieldDeclIDAndTypeName);
      }
      auto json = json_union_decl(currentDeclIDAndTypeName, fields, pseudoRoot,
                                  filePathComponents);
      llvm::outs() << json << "\n";

    } else {
      std::vector<std::pair<std::string, StructField>> fields;
      const auto &layout = RD->getASTContext().getASTRecordLayout(RD);
      for (const auto *field : RD->fields()) {
        auto fieldOffset = layout.getFieldOffset(field->getFieldIndex());
        auto fieldType = field->getType();
        auto fieldDeclIDAndTypeName = typeToDeclIDAndTypeName(fieldType);
        preEmitIfRecordDecl(fieldType, fieldDeclIDAndTypeName);
        auto [fieldSize, _, __] = RD->getASTContext().getTypeInfo(fieldType);
        StructField fieldStruct = {fieldOffset, fieldSize,
                                   fieldDeclIDAndTypeName};
        fields.emplace_back(field->getNameAsString(), fieldStruct);
      }
      auto json = json_struct_decl(currentDeclIDAndTypeName, fields, pseudoRoot,
                                   filePathComponents);
      llvm::outs() << json << "\n";
    }

    return true;
  }

  // Enum declaration handling.
  else if (auto ED = llvm::dyn_cast<clang::EnumDecl>(D)) {
    auto name = ED->getNameAsString();
    auto backingType = ED->getIntegerType();
    std::vector<std::pair<std::string, uint64_t>> entries;
    for (const auto *enumerator : ED->enumerators()) {
      entries.emplace_back(enumerator->getNameAsString(),
                           enumerator->getInitVal().getZExtValue());
    }
    auto json = json_enum_decl(currentDeclIDAndTypeName,
                               typeToDeclIDAndTypeName(backingType), entries,
                               pseudoRoot, filePathComponents);
    llvm::outs() << json << "\n";
    return true;
  }

  // Function declaration handling.
  else if (auto FD = llvm::dyn_cast<clang::FunctionDecl>(D)) {
    auto name = FD->getNameAsString();
    auto returnType = FD->getReturnType();
    auto returnDeclIDAndTypeName = typeToDeclIDAndTypeName(returnType);
    preEmitIfRecordDecl(returnType, returnDeclIDAndTypeName);
    OrderedTypesMap parameters;
    for (const auto *param : FD->parameters()) {
      auto paramType = param->getType();
      auto paramDeclIDAndTypeName = typeToDeclIDAndTypeName(paramType);
      preEmitIfRecordDecl(paramType, paramDeclIDAndTypeName);
      parameters.emplace_back(param->getNameAsString(), paramDeclIDAndTypeName);
    }
    auto json =
        json_function_decl(currentDeclIDAndTypeName, returnDeclIDAndTypeName,
                           parameters, pseudoRoot, filePathComponents);
    llvm::outs() << json << "\n";
    return true;
  } else {
    return true; // Ignore other types of declarations
  }
  return true; // Fallthrough case, should not be reached.
}

class TypeExtractorVisitor
    : public clang::RecursiveASTVisitor<TypeExtractorVisitor> {
public:
  bool VisitTypedefDecl(clang::TypedefDecl *TD) { return emitTypeDecl(TD); }

  bool VisitRecordDecl(clang::RecordDecl *RD) { return emitTypeDecl(RD); }

  bool VisitEnumDecl(clang::EnumDecl *ED) { return emitTypeDecl(ED); }

  bool VisitFunctionDecl(clang::FunctionDecl *FD) { return emitTypeDecl(FD); }
};

class TypeExtractor : public clang::ASTConsumer {
private:
  TypeExtractorVisitor Visitor;

public:
  void HandleTranslationUnit(clang::ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }
};

std::unique_ptr<clang::ASTConsumer>
TypeExtractorAction::CreateASTConsumer(clang::CompilerInstance &CI,
                                       llvm::StringRef file) {
  sysroot = CI.getHeaderSearchOpts().Sysroot;
  resourceDir = CI.getHeaderSearchOpts().ResourceDir;
  return std::make_unique<TypeExtractor>();
}