# Network ML

[![CI](https://github.com/meta-legend/network-ml-library/actions/workflows/ci.yml/badge.svg)](https://github.com/meta-legend/network-ml-library/actions/workflows/ci.yml)
[![Release v2.0.0](https://img.shields.io/badge/Release-v2.0.0-brightgreen)](https://github.com/meta-legend/network-ml-library/releases/tag/v2.0.0)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Wiki](https://img.shields.io/badge/docs-wiki-blue.svg)](https://github.com/meta-legend/network-ml-library/wiki)

A small, easy-to-use C++ library for REST API requests, LLM chat, and file
management, built on libcurl. The chat client works with a local model via
[Ollama](https://ollama.com) or cloud providers (Groq, OpenRouter, DeepSeek,
OpenAI, Anthropic) behind one API.

It aims to make two things one-liners from C++:

```cpp
// HTTP request with a rich response
ML::Requests req;
ML::Response r = req.get("https://api.example.com/data");
if (r.ok()) std::cout << r.status << "\n" << r.body;

// Talk to an LLM (local Ollama here; swap the Provider for a cloud model)
ML::Chat chat(ML::Provider::Ollama, "llama3.2:1b");
std::cout << chat.ask("Explain RAII in one sentence.");
```

## Documentation

Full documentation, including build instructions and usage examples, lives in the
[project wiki](https://github.com/meta-legend/Network-ML-Library/wiki):

- [Getting Started](https://github.com/meta-legend/Network-ML-Library/wiki/Getting-Started): requirements, building, prebuilt binaries
- [Requests](https://github.com/meta-legend/Network-ML-Library/wiki/Requests): the HTTP client
- [Chat](https://github.com/meta-legend/Network-ML-Library/wiki/Chat): the LLM client
- [File](https://github.com/meta-legend/Network-ML-Library/wiki/File): file utilities

## Features

- `ML::Requests`: full HTTP verbs (`get`/`post`/`put`/`patch`/`del`/`head`)
  returning a rich `Response`, with inline request bodies, multiple request
  headers, and per-request timeouts. Also file `download` (streamed to disk with
  optional progress), multipart `upload`, and opt-in auto-`retries` with backoff.
  (Legacy string-returning `getReq`/`postReq`/etc. are kept for compatibility.)
- `ML::Auth` / `ML::Url`: helpers for auth headers (`bearer`/`basic`/`apiKey`) and
  percent-encoded query URLs (`Url::build`).
- `ML::Response`: `status`, `body`, response `headers` (a map), and `ok()` (true for any 2xx).
- `ML::Chat`: conversational client for Ollama (local), Groq, OpenRouter, DeepSeek,
  OpenAI, or Anthropic:
  - one API across every provider
  - persistent system prompt and conversation memory
  - adjustable temperature and max tokens
  - token streaming via a callback
  - reasoning-token support for reasoning models (the model's "thinking" streams
    to a separate callback, available via `lastReasoning()`)
  - save/load conversation history to a JSON file (`saveHistory`/`loadHistory`)
- `ML::File`: cross-platform file/folder utilities (`std::filesystem`): text and
  binary read/write, append, copy/move, directory listing, recursive folder
  create/delete, and exists/size/last-modified queries.
- All HTTP runs in-process via libcurl (HTTPS through Schannel on Windows), with
  no `curl` subprocess and no temp files.

## License

MIT. See [LICENSE](LICENSE).
