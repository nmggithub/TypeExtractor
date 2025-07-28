#include "json.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <optional>
#include <string>

#ifndef TYPE_EXTRACTOR_UTIL_H
#define TYPE_EXTRACTOR_UTIL_H

inline std::optional<clang::Decl *>
qualTypeToDeclPtr(const clang::QualType &QT) {
  if (const clang::RecordType *RT = QT->getAs<clang::RecordType>()) {
    return RT->getDecl();
  } else if (const clang::EnumType *ET = QT->getAs<clang::EnumType>()) {
    return ET->getDecl();
  }
  return std::nullopt;
}

inline bool shouldParseDecl(const clang::NamedDecl *D) {
  if (D->isTemplated()) // TODO: Support templates in the future.
    return false;
  if (isa<clang::CXXMethodDecl>(D))
    return false;
  if (D->isImplicit())
    return false;
  if (D->isInvalidDecl())
    return false;

  const clang::DeclContext *DC = D->getDeclContext();
  while (DC && !isa<clang::TranslationUnitDecl>(DC)) {
    if (
        // If scope does not extend up to the file level, we skip it.
        (!DC->isTransparentContext() && !DC->isFileContext()) ||
        // If it's a named namespace, we skip it (anonymous ones are fine).
        (DC->isNamespace() &&
         !llvm::dyn_cast<clang::NamespaceDecl>(DC)->isAnonymousNamespace())) {
      return false;
    }
    DC = DC->getParent();
  }
  return true;
}

inline DeclIDAndTypeName typeToDeclIDAndTypeName(const clang::QualType &QT) {
  const auto D = qualTypeToDeclPtr(QT);
  std::optional<std::string> declID =
      D ? std::make_optional(std::to_string(D.value()->getID())) : std::nullopt;
  return {declID, QT.getAsString()};
}

#endif // TYPE_EXTRACTOR_UTIL_H