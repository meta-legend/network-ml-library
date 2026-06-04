# Network ML

A small, easy-to-use C++ library for **REST API requests** and **local LLM chat** (via [Ollama](https://ollama.com)), built on **libcurl**.

It aims to make two things one-liners from C++:

```cpp
// HTTP request with a rich response
ML::Requests req;
ML::Response r = req.get("https://api.example.com/data");
if (r.ok()) std::cout << r.status << "\n" << r.body;

// Talk to a local LLM
ML::Chat chat(ML::Provider::Ollama, "llama3.2:1b");
std::cout << chat.ask("Explain RAII in one sentence.");
```

## Features

- **`ML::Requests`**: full HTTP verbs (`get`/`post`/`put`/`patch`/`del`/`head`)
  returning a rich `Response`, with inline request bodies, multiple request
  headers, and per-request timeouts. (Legacy string-returning `getReq`/`postReq`/
  etc. are kept for backwards compatibility.)
- **`ML::Response`**: `status`, `body`, response `headers` (a map), and `ok()` (true for any 2xx).
- **`ML::Chat`**: conversational client for **Ollama** (local), **Groq**, **OpenRouter**, **OpenAI**, or **Anthropic**:
  - one API across every provider
  - persistent **system prompt** and **conversation memory**
  - adjustable **temperature** and **max tokens**
  - **token streaming** via a callback
  - **reasoning-token** support for reasoning models (the model's "thinking"
    streams to a separate callback, available via `lastReasoning()`)
  - **save/load conversation history** to a JSON file (`saveHistory`/`loadHistory`)
- **`ML::File`**: cross-platform file/folder utilities (`std::filesystem`): text
  and binary read/write, append, copy/move, directory listing, recursive folder
  create/delete, and exists/size/last-modified queries.
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

    // POST with an inline JSON body, headers, and a 10s timeout
    Response p = req.post("https://httpbin.org/post",
                          R"({"name":"widget"})",
                          { "Content-Type: application/json" },
                          10);
    std::cout << "post -> " << p.status
              << " (type " << p.headers["content-type"] << ")\n";

    Chat chat(Provider::Ollama, "llama3.2:1b");
    chat.setSystem("You are a terse assistant.");

    // Streaming: tokens print as they are generated
    chat.stream("Why is C++ fast?",
        [](const std::string& token){ std::cout << token << std::flush; });
}
```

## Chat providers

The same `Chat` class targets several backends. Switch by choosing a constructor:

```cpp
// Ollama (local, no API key)
Chat ollama(Provider::Ollama, "llama3.2:1b");

// Groq (free tier, fast)
Chat groq(Provider::Groq, "llama-3.1-8b-instant", std::getenv("GROQ_API_KEY"));

// OpenRouter (many models, some free)
Chat router(Provider::OpenRouter, "meta-llama/llama-3.1-8b-instruct:free",
            std::getenv("OPENROUTER_API_KEY"));

// DeepSeek (use "deepseek-reasoner" to get reasoning tokens)
Chat deepseek(Provider::DeepSeek, "deepseek-chat", std::getenv("DEEPSEEK_API_KEY"));

// OpenAI
Chat gpt(Provider::OpenAI, "gpt-4o-mini", std::getenv("OPENAI_API_KEY"));

// Anthropic (max_tokens is required)
Chat claude(Provider::Anthropic, "claude-3-5-haiku-latest",
            std::getenv("ANTHROPIC_API_KEY"));
claude.setMaxTokens(1024);
```

Everything else (`setSystem`, `ask`, `stream`, history, `reset`) works the same
regardless of provider. The library handles each provider's endpoint, auth
headers, request shape, and streaming format (NDJSON for Ollama, SSE for
OpenAI/Anthropic) internally.

### Other OpenAI-compatible providers

[Groq](https://console.groq.com) and [OpenRouter](https://openrouter.ai) are built
in as `Provider::Groq` and `Provider::OpenRouter`. For any *other* OpenAI-compatible
service (e.g. Together), use `Provider::OpenAI` with a custom host:

```cpp
Chat together(Provider::OpenAI, "meta-llama/Llama-3-8b-chat-hf",
              std::getenv("TOGETHER_API_KEY"),
              "https://api.together.xyz");
```

`Provider::OpenAI` speaks the standard OpenAI chat format, so any endpoint that
implements it works by swapping the host and model.

## License

MIT. See [LICENSE](LICENSE).
