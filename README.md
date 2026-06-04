# Network ML

A small, easy-to-use C++ library for **REST API requests** and **local LLM chat** (via [Ollama](https://ollama.com)), built on **libcurl**.

It aims to make two things one-liners from C++:

```cpp
// HTTP request with a rich response
ML::Requests req;
ML::Response r = req.get("https://api.example.com/data");
if (r.ok()) std::cout << r.status << "\n" << r.body;

// Talk to a local LLM
ML::Chat chat("llama3.2:1b");
std::cout << chat.ask("Explain RAII in one sentence.");
```

## Features

- **`ML::Requests`**: `GET`/`POST`/`PUT`/`DELETE`/`HEAD` plus a modern `get()` returning a rich `Response` (status code + body).
- **`ML::Response`**: `status`, `body`, and `ok()` (true for any 2xx).
- **`ML::Chat`**: conversational client for a local Ollama server:
  - persistent **system prompt** and **conversation memory**
  - adjustable **temperature**
  - **token streaming** via a callback
- **`ML::File`**: cross-platform file/folder helpers (`std::filesystem`).
- All HTTP runs **in-process via libcurl** (HTTPS through Schannel on Windows), with no `curl` subprocess and no temp files.

## Requirements

- A C++17 compiler (MSVC 2017+, GCC 8+, Clang 7+)
- [CMake](https://cmake.org) 3.15+
- [vcpkg](https://github.com/microsoft/vcpkg) (provides libcurl automatically via the manifest)
- For the `Chat` features: [Ollama](https://ollama.com) running locally with a model pulled, e.g. `ollama pull llama3.2:1b`

## Building

The dependency (libcurl) is declared in `vcpkg.json`, so vcpkg installs it for you.

```sh
git clone https://github.com/meta-legend/Network-ML-Library
cd Network-ML-Library

# Configure (point CMake at your vcpkg toolchain)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake

# Build the static library
cmake --build build --config Release
```

This produces a static library (`networkml`) you can link into your own project.

> On Linux/macOS, drop the toolchain flag if libcurl is provided by your system
> package manager instead of vcpkg.

## Usage example

```cpp
#include <iostream>
#include "networkml.h"
using namespace ML;

int main() {
    Requests req;
    Response r = req.get("https://catfact.ninja/fact");
    std::cout << "status " << r.status << ": " << r.body << "\n";

    Chat chat("llama3.2:1b");
    chat.setSystem("You are a terse assistant.");

    // Streaming: tokens print as they are generated
    chat.stream("Why is C++ fast?",
        [](const std::string& token){ std::cout << token << std::flush; });
}
```

## License

MIT. See [LICENSE](LICENSE).
