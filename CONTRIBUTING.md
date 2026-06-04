# Contributing to Network ML

Thanks for your interest in contributing! This is a small C++ library, so the
process is lightweight. This guide covers how to set up, build, make changes,
and submit them.

## Ways to contribute

- **Report bugs**: open an issue with steps to reproduce, your OS/compiler, and
  what you expected vs. what happened.
- **Suggest features**: open an issue describing the use case.
- **Submit code**: fix a bug or add a feature via a pull request (see below).
- **Improve docs**: corrections and clarifications to the README or this file
  are very welcome.

## Prerequisites

- A **C++17** compiler (MSVC 2017+, GCC 8+, or Clang 7+)
- **CMake** 3.15+
- **vcpkg** (provides the dependencies declared in `vcpkg.json`: `curl` and
  `nlohmann-json`)
- For working on the `Chat` features: **[Ollama](https://ollama.com)** running
  locally with a model pulled, e.g. `ollama pull llama3.2:1b`

## Getting started

```sh
# 1. Fork the repo on GitHub, then clone your fork
git clone https://github.com/<your-username>/Network-ML-Library
cd Network-ML-Library

# 2. Configure with the vcpkg toolchain (installs curl + nlohmann-json)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# 3. Build
cmake --build build --config Release
```

## Project layout

```
CMakeLists.txt        Canonical cross-platform build
vcpkg.json            Dependency manifest (curl, nlohmann-json)
Network ML/
  networkml.h         Public API (the only header consumers include)
  Requests.cpp        HTTP methods (libcurl)
  Chat.cpp            Ollama chat client + Response transport
  File.cpp            Cross-platform file/folder helpers
```

Note: the `.sln`/`.vcxproj` files are intentionally **not** committed; CMake is
the canonical build. You can still develop in Visual Studio via
**File > Open > Folder** (VS understands CMake projects natively).

## Making changes

1. Create a branch: `git checkout -b my-change`
2. Keep the **public API in `networkml.h` stable** where possible. If you change
   it, update the README usage examples to match.
3. Build and exercise your change before submitting (a small `main()` that calls
   the affected functions is the simplest way to verify behaviour).
4. Keep dependencies minimal: prefer the standard library and the two existing
   dependencies over pulling in new ones.

## Coding conventions

Match the existing style so the codebase stays consistent:

- All public types live in the **`ML` namespace**.
- Classes/types: `PascalCase` (`Requests`, `Chat`, `Response`).
- Functions/methods and variables: `camelCase` (`getReq`, `setSystem`).
- Use libcurl for HTTP (no shelling out to external processes) and
  `nlohmann/json` for JSON.
- Prefer `std::filesystem` and other standard facilities for portability;
  avoid platform-specific APIs unless guarded by an `#ifdef`.
- 4-space indentation, braces on the same line.

## Submitting a pull request

1. Push your branch to your fork.
2. Open a pull request against `main` with:
   - a clear description of **what** changed and **why**,
   - any new behaviour or API changes called out,
   - confirmation that it builds (and how you tested it).
3. Keep PRs focused: one logical change per PR is easier to review.

## License

By contributing, you agree that your contributions are licensed under the
project's [MIT License](LICENSE).
