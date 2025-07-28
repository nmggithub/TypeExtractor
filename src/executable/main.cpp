#include "TypeExtractorAction.h"
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  std::vector<std::string> args = {};

  // Add additional arguments from command line
  for (int i = 1; i < argc; ++i) {
    args.push_back(argv[i]);
  }

  // Take code from standard input
  std::string code =
      std::string((std::stringstream() << std::cin.rdbuf()).str());

  return clang::tooling::runToolOnCodeWithArgs(
      std::make_unique<TypeExtractorAction>(), code, args, "header.h");
}