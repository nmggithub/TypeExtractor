# TypeExtractor

## Introduction

**TypeExtractor** is a Clang-driven type extractor for (Objective-)CXX headers.

## Design Philosophy

The initial design was driven by a need for a better parser to ingest macOS types into Ghidra, but it can (hopefully) be used for a lot more. It currently extracts **typedef**, **enum**, **struct**, **union**, and **function** declarations from headers. Support for more declarations may be added in the future.

TypeExtractor is designed to be very minimal. It's mostly a wrapper around Clang, with some parsing code that emits information about types that Clang finds. For more specific use cases, feel free to wrap it in your own tooling, suited to your needs.

## Building TypeExtractor

TypeExtractor uses [CMake](https://cmake.org/) for building. To build TypeExtractor, first make a directory for its build artifacts to be output to. From that directory, run the following (where `/path/to/project/root` is the path to your cloned copy of this repository):

```
$ cmake /path/to/project/root
```

Then, from the root of this repository, run the following (where `/path/to/build/directory` is the path to your previously-created build directory):
```
$ cmake --build /path/to/build/directory
```

### On platform support

Currently, TypeExtractor will only compile on macOS where [LLVM has been installed with Hombebrew](https://formulae.brew.sh/formula/llvm). I plan to get Linux support working soon. TypeExtractor will expicitly error if you attempt to compile it on Windows, but I might remove that error if I find it works well on Windows.

## Using TypeExtractor

The Clang compiler has two stages that TypeExtractor makes use of:

1. The Clang **driver** is what the `clang` executable calls when compiling code. TypeExtractor provides a Clang plugin (`libte.so`) that can be used with any compatible Clang executable like so:
```
$ /path/to/clang -fplugin=/path/to/libte.so -Xclang -plugin -Xclang type-extractor [args...]
```
2. The Clang **frontend** is often then called by the driver, but can also be called on its own. TypeExtractor provides a standalone executable (`te`) that takes in code through `stdin` and can be called like so:
```
$ cat header.h | /path/to/te [args...]
```

In both cases, the `[args...]` passed in are then passed directly to the Clang driver or frontend (respectively). The driver approach (with a `clang` executable) might be useful if you're fine with the driver making some configuration decisions for you. The frontend approach (with the standalone `te` exectuable) is best if you want more manual control over the configuration of the Clang compiler.

### Parsing Output

The `stdout` will be a series of lines, each one a JSON object describing a type that exists in the passed-in header (or headers it includes, and so on). These are emitted in an order that should allow for ease of parsing, with dependent types emitted first before their parent type. The schema of these objects may be subject to change, so they will not be documented here until they are more stable.

The `stderr` may include some error text from Clang, if it encounters warnings and/or errors (and is configured to print them out).

# Upcoming Improvements

- Support for other platforms besides macOS
- Better handling of inlinable functions
- Support for more kinds of type declarations
- General improvements
