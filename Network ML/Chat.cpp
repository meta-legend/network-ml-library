#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "networkml.h"

using json = nlohmann::json;

namespace ML {

    // --- libcurl transport (file-local) --------------------------------------

    // Appends received bytes to a std::string (buffered, non-streaming).
    static size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
        size_t n = size * nmemb;
        static_cast<std::string*>(userdata)->append(ptr, n);
        return n;
    }

    // Simple GET -> rich Response.
    Response Requests::get(std::string url) {
        Response r;
        CURL* curl = curl_easy_init();
        if (!curl) return r;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);   // follow redirects
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r.body);

        if (curl_easy_perform(curl) == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status);
        }
        curl_easy_cleanup(curl);
        return r;
    }

    // POST a JSON body -> rich Response (buffered).
    static Response postJson(const std::string& url, const std::string& body) {
        Response r;
        CURL* curl = curl_easy_init();
        if (!curl) return r;

        curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
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

    // --- streaming plumbing ---------------------------------------------------

    // Ollama streams NDJSON: one JSON object per line. We accumulate bytes,
    // split on newlines, parse each complete line, and fire the user callback
    // with each token's content.
    struct StreamCtx {
        std::function<void(const std::string&)>* onToken;
        std::string lineBuf;   // holds an as-yet-incomplete line
        std::string full;      // the assembled reply
    };

    static size_t streamWrite(char* ptr, size_t size, size_t nmemb, void* userdata) {
        size_t n = size * nmemb;
        StreamCtx* ctx = static_cast<StreamCtx*>(userdata);
        ctx->lineBuf.append(ptr, n);

        size_t pos;
        while ((pos = ctx->lineBuf.find('\n')) != std::string::npos) {
            std::string line = ctx->lineBuf.substr(0, pos);
            ctx->lineBuf.erase(0, pos + 1);
            if (line.empty()) continue;

            try {
                json j = json::parse(line);
                if (j.contains("message") && j["message"].contains("content")) {
                    std::string tok = j["message"]["content"].get<std::string>();
                    if (!tok.empty()) {
                        (*ctx->onToken)(tok);
                        ctx->full += tok;
                    }
                }
            }
            catch (const json::exception&) {
                // ignore any non-JSON / partial line
            }
        }
        return n;
    }

    // --- Chat (Ollama, history-aware) ----------------------------------------

    Chat::Chat(std::string model, std::string host)
        : model(model), host(host), temperature(0.8) {
    }

    void Chat::setSystem(std::string instruction) {
        if (!messages.empty() && messages.front().role == "system") {
            messages.front().content = instruction;
        }
        else {
            messages.insert(messages.begin(), Message{ "system", instruction });
        }
    }

    void Chat::setTemperature(double t) {
        temperature = t;
    }

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

    const std::vector<Message>& Chat::history() const {
        return messages;
    }

    std::string Chat::buildBody(const std::string& prompt, bool streaming) {
        json body;
        body["model"] = model;
        body["stream"] = streaming;
        body["options"]["temperature"] = temperature;

        body["messages"] = json::array();
        for (const auto& m : messages) {
            body["messages"].push_back({ {"role", m.role}, {"content", m.content} });
        }
        body["messages"].push_back({ {"role", "user"}, {"content", prompt} });

        return body.dump();
    }

    Response Chat::raw(std::string prompt) {
        return postJson(host + "/api/chat", buildBody(prompt, false));
    }

    std::string Chat::ask(std::string prompt) {
        Response r = raw(prompt);
        if (!r.ok()) return "";

        try {
            json res = json::parse(r.body);
            std::string reply = res.at("message").at("content").get<std::string>();
            messages.push_back(Message{ "user", prompt });
            messages.push_back(Message{ "assistant", reply });
            return reply;
        }
        catch (const json::exception&) {
            return "";
        }
    }

    void Chat::stream(std::string prompt, std::function<void(const std::string&)> onToken) {
        CURL* curl = curl_easy_init();
        if (!curl) return;

        std::string url = host + "/api/chat";
        std::string body = buildBody(prompt, true);

        StreamCtx ctx;
        ctx.onToken = &onToken;

        curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, streamWrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            messages.push_back(Message{ "user", prompt });
            messages.push_back(Message{ "assistant", ctx.full });
        }
    }
}
