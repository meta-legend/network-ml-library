#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <functional>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "networkml.h"

// Auto-link the Windows system libraries that statically-linked libcurl
// (Schannel backend) needs, so consumers only have to link networkml.lib.
// Guarded so it has no effect on non-Windows / non-MSVC builds (CMake handles
// the platform dependencies there).
#if defined(_WIN32) && defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "normaliz.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "iphlpapi.lib")
#endif

using json = nlohmann::json;

namespace ML {

    // --- libcurl transport (file-local) --------------------------------------

    static size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
        size_t n = size * nmemb;
        static_cast<std::string*>(userdata)->append(ptr, n);
        return n;
    }

    // OpenAI and OpenAI-compatible providers (Groq) share the same wire format:
    // same endpoint path, Bearer auth, request body, response shape, and SSE.
    static bool usesOpenAIFormat(Provider p) {
        return p == Provider::OpenAI || p == Provider::Groq
            || p == Provider::OpenRouter || p == Provider::DeepSeek;
    }

    // Builds the HTTP headers for a provider (Content-Type + auth).
    static curl_slist* providerHeaders(Provider p, const std::string& apiKey) {
        curl_slist* h = curl_slist_append(nullptr, "Content-Type: application/json");
        if (usesOpenAIFormat(p)) {
            std::string auth = "Authorization: Bearer " + apiKey;
            h = curl_slist_append(h, auth.c_str());
        }
        else if (p == Provider::Anthropic) {
            std::string key = "x-api-key: " + apiKey;
            h = curl_slist_append(h, key.c_str());
            h = curl_slist_append(h, "anthropic-version: 2023-06-01");
        }
        return h;
    }

    // Extracts the assistant answer (returned) and the reasoning/"thinking"
    // (via reasoningOut) from a non-streaming response body.
    static std::string parseReply(Provider p, const std::string& bodyText,
        std::string& reasoningOut) {
        reasoningOut.clear();
        try {
            json res = json::parse(bodyText);
            if (usesOpenAIFormat(p)) {
                auto& msg = res.at("choices").at(0).at("message");
                if (msg.contains("reasoning") && !msg["reasoning"].is_null())
                    reasoningOut = msg["reasoning"].get<std::string>();
                else if (msg.contains("reasoning_content") && !msg["reasoning_content"].is_null())
                    reasoningOut = msg["reasoning_content"].get<std::string>();
                return msg.at("content").get<std::string>();
            }
            if (p == Provider::Anthropic) {
                return res.at("content").at(0).at("text").get<std::string>();
            }
            // Ollama
            auto& msg = res.at("message");
            if (msg.contains("thinking") && !msg["thinking"].is_null())
                reasoningOut = msg["thinking"].get<std::string>();
            return msg.at("content").get<std::string>();
        }
        catch (const json::exception&) {
            return "";
        }
    }

    // --- streaming plumbing (provider-aware) ---------------------------------

    struct StreamCtx {
        Provider provider;
        std::function<void(const std::string&)>* onToken;
        std::function<void(const std::string&)>* onReasoning;  // may be empty
        std::string lineBuf;   // an as-yet-incomplete line
        std::string full;      // the assembled answer
        std::string reasoning; // the assembled reasoning/"thinking"
    };

    // Parses one complete line and fires the callback if it carries a token.
    // Ollama streams raw NDJSON; OpenAI/Anthropic stream SSE ("data: {json}").
    static void handleStreamLine(StreamCtx* ctx, std::string line) {
        if (ctx->provider != Provider::Ollama) {
            // SSE: only "data:" lines carry payload; skip "event:" etc.
            if (line.rfind("data:", 0) != 0) return;
            line = line.substr(5);
            while (!line.empty() && line.front() == ' ') line.erase(line.begin());
            if (line == "[DONE]") return;
        }
        if (line.empty()) return;

        try {
            json j = json::parse(line);
            std::string tok;    // answer content
            std::string rtok;   // reasoning / "thinking"

            if (usesOpenAIFormat(ctx->provider)) {
                auto& choices = j["choices"];
                if (choices.is_array() && !choices.empty()) {
                    auto& delta = choices[0]["delta"];
                    if (delta.contains("content") && !delta["content"].is_null())
                        tok = delta["content"].get<std::string>();
                    // gpt-oss/OpenRouter use "reasoning"; DeepSeek "reasoning_content"
                    if (delta.contains("reasoning") && !delta["reasoning"].is_null())
                        rtok = delta["reasoning"].get<std::string>();
                    else if (delta.contains("reasoning_content") && !delta["reasoning_content"].is_null())
                        rtok = delta["reasoning_content"].get<std::string>();
                }
            }
            else if (ctx->provider == Provider::Anthropic) {
                if (j.value("type", std::string()) == "content_block_delta") {
                    auto& delta = j["delta"];
                    if (delta.contains("text"))
                        tok = delta["text"].get<std::string>();
                    else if (delta.contains("thinking"))   // extended thinking
                        rtok = delta["thinking"].get<std::string>();
                }
            }
            else { // Ollama
                if (j.contains("message")) {
                    auto& msg = j["message"];
                    if (msg.contains("content"))
                        tok = msg["content"].get<std::string>();
                    if (msg.contains("thinking") && !msg["thinking"].is_null())
                        rtok = msg["thinking"].get<std::string>();
                }
            }

            if (!tok.empty()) {
                (*ctx->onToken)(tok);
                ctx->full += tok;
            }
            if (!rtok.empty()) {
                ctx->reasoning += rtok;
                if (ctx->onReasoning && *ctx->onReasoning)
                    (*ctx->onReasoning)(rtok);
            }
        }
        catch (const json::exception&) {
            // ignore non-JSON / partial lines
        }
    }

    static size_t streamWrite(char* ptr, size_t size, size_t nmemb, void* userdata) {
        size_t n = size * nmemb;
        StreamCtx* ctx = static_cast<StreamCtx*>(userdata);
        ctx->lineBuf.append(ptr, n);

        size_t pos;
        while ((pos = ctx->lineBuf.find('\n')) != std::string::npos) {
            std::string line = ctx->lineBuf.substr(0, pos);
            ctx->lineBuf.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back(); // CRLF
            handleStreamLine(ctx, line);
        }
        return n;
    }

    // --- Chat ----------------------------------------------------------------

    Chat::Chat(Provider provider, std::string model, std::string apiKey, std::string host)
        : provider(provider), apiKey(apiKey), model(model), host(host),
        temperature(0.8), maxTokens(1024) {
        if (this->host.empty()) {
            switch (provider) {
            case Provider::OpenAI:    this->host = "https://api.openai.com"; break;
            case Provider::Anthropic: this->host = "https://api.anthropic.com"; break;
            case Provider::Groq:      this->host = "https://api.groq.com/openai"; break;
            case Provider::OpenRouter: this->host = "https://openrouter.ai/api"; break;
            case Provider::DeepSeek:  this->host = "https://api.deepseek.com"; break;
            case Provider::Ollama:
            default:                  this->host = "http://localhost:11434"; break;
            }
        }
    }

    // Null-safe overload: a missing env var (null) becomes an empty key.
    Chat::Chat(Provider provider, std::string model, const char* apiKey, std::string host)
        : Chat(provider, model, apiKey ? std::string(apiKey) : std::string(), host) {
    }

    void Chat::setSystem(std::string instruction) {
        if (!messages.empty() && messages.front().role == "system") {
            messages.front().content = instruction;
        }
        else {
            messages.insert(messages.begin(), Message{ "system", instruction });
        }
    }

    void Chat::setTemperature(double t) { temperature = t; }
    void Chat::setMaxTokens(int m) { maxTokens = m; }

    void Chat::reset() {
        if (!messages.empty() && messages.front().role == "system") {
            Message sys = messages.front();
            messages.clear();
            messages.push_back(sys);
        }
        else {
            messages.clear();
        }
    }

    const std::vector<Message>& Chat::history() const { return messages; }

    bool Chat::saveHistory(const std::string& path) const {
        json arr = json::array();
        for (const auto& m : messages) {
            arr.push_back({ {"role", m.role}, {"content", m.content} });
        }
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) return false;
        out << arr.dump(2);
        return true;
    }

    bool Chat::loadHistory(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;
        try {
            json arr;
            in >> arr;
            std::vector<Message> loaded;
            for (const auto& m : arr) {
                loaded.push_back(Message{
                    m.at("role").get<std::string>(),
                    m.at("content").get<std::string>() });
            }
            messages = std::move(loaded);
            return true;
        }
        catch (const json::exception&) {
            return false;   // malformed file; leave existing history untouched
        }
    }

    std::string Chat::endpoint() const {
        switch (provider) {
        case Provider::OpenAI:
        case Provider::Groq:
        case Provider::OpenRouter:
        case Provider::DeepSeek:  return host + "/v1/chat/completions";
        case Provider::Anthropic: return host + "/v1/messages";
        case Provider::Ollama:
        default:                  return host + "/api/chat";
        }
    }

    std::string Chat::buildBody(const std::string& prompt, bool streaming) {
        json body;
        body["model"] = model;
        body["stream"] = streaming;

        if (provider == Provider::Anthropic) {
            // Anthropic: system is a top-level field; messages exclude it and
            // max_tokens is required.
            body["max_tokens"] = maxTokens;
            body["temperature"] = temperature;

            std::string systemText;
            json msgs = json::array();
            for (const auto& m : messages) {
                if (m.role == "system") systemText += m.content;
                else msgs.push_back({ {"role", m.role}, {"content", m.content} });
            }
            msgs.push_back({ {"role", "user"}, {"content", prompt} });
            if (!systemText.empty()) body["system"] = systemText;
            body["messages"] = msgs;
        }
        else {
            // Ollama + OpenAI: system is just another message in the array.
            json msgs = json::array();
            for (const auto& m : messages) {
                msgs.push_back({ {"role", m.role}, {"content", m.content} });
            }
            msgs.push_back({ {"role", "user"}, {"content", prompt} });
            body["messages"] = msgs;

            if (usesOpenAIFormat(provider)) {
                body["temperature"] = temperature;
                if (maxTokens > 0) body["max_tokens"] = maxTokens;
            }
            else { // Ollama
                body["options"]["temperature"] = temperature;
            }
        }
        return body.dump();
    }

    Response Chat::raw(std::string prompt) {
        Response r;
        CURL* curl = curl_easy_init();
        if (!curl) return r;

        std::string url = endpoint();
        std::string body = buildBody(prompt, false);
        curl_slist* headers = providerHeaders(provider, apiKey);

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r.body);

        if (curl_easy_perform(curl) == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status);
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return r;
    }

    std::string Chat::ask(std::string prompt) {
        reasoningText.clear();
        Response r = raw(prompt);
        if (!r.ok()) return "";

        std::string reply = parseReply(provider, r.body, reasoningText);
        if (!reply.empty()) {
            messages.push_back(Message{ "user", prompt });
            messages.push_back(Message{ "assistant", reply });
        }
        return reply;
    }

    const std::string& Chat::lastReasoning() const {
        return reasoningText;
    }

    long Chat::stream(std::string prompt,
        std::function<void(const std::string&)> onToken,
        std::function<void(const std::string&)> onReasoning) {
        reasoningText.clear();

        CURL* curl = curl_easy_init();
        if (!curl) return 0;

        std::string url = endpoint();
        std::string body = buildBody(prompt, true);

        StreamCtx ctx;
        ctx.provider = provider;
        ctx.onToken = &onToken;
        ctx.onReasoning = &onReasoning;

        curl_slist* headers = providerHeaders(provider, apiKey);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, streamWrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

        long status = 0;
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        // Only record the exchange if the request actually succeeded (2xx).
        if (status >= 200 && status < 300) {
            reasoningText = ctx.reasoning;
            messages.push_back(Message{ "user", prompt });
            messages.push_back(Message{ "assistant", ctx.full });
        }
        return status;
    }
}
