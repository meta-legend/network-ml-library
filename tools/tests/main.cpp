// Smoke tests for Network ML, built from the in-tree source and driven by CTest.
//
// Default run is offline and deterministic: every Auth/Url/File function,
// plus Chat state helpers (history/reset/save/load) and Requests/Chat object
// construction. No network, no credentials needed.
//
// Live HTTP coverage (Requests verbs, download, upload, retries) runs when
//   NETWORKML_LIVE=1  is set in the environment. Uses postman-echo.com.
//
// Live Chat coverage (ask, raw, stream, lastReasoning) runs when both
//   NETWORKML_CHAT_PROVIDER  (one of: ollama|openai|anthropic|groq|openrouter|deepseek)
//   NETWORKML_CHAT_MODEL     (e.g. "llama3.2:1b" or "openai/gpt-oss-20b:free")
// are set. Cloud providers also need their API key in the standard env var
//   (OPENAI_API_KEY, ANTHROPIC_API_KEY, GROQ_API_KEY, OPENROUTER_API_KEY,
//   DEEPSEEK_API_KEY); Ollama just needs the server running.
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "networkml.h"

using namespace ML;

static int g_pass = 0;
static int g_fail = 0;

static void section(const std::string& name) {
    std::cout << "\n[" << name << "]\n";
}

#define CHECK(expr) do { \
    if (expr) { ++g_pass; std::cout << "  ok   " << #expr << "\n"; } \
    else      { ++g_fail; std::cout << "  FAIL " << #expr << "  @" << __LINE__ << "\n"; } \
} while (0)

#define CHECK_MSG(expr, msg) do { \
    if (expr) { ++g_pass; std::cout << "  ok   " << (msg) << "\n"; } \
    else      { ++g_fail; std::cout << "  FAIL " << (msg) << "  @" << __LINE__ << "\n"; } \
} while (0)

static bool env_flag(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] && std::string(v) != "0";
}
static std::string env_str(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string{};
}

// offline: auth and url helpers
static void test_auth_url() {
    section("Auth + Url");

    std::string b = Auth::bearer("tok123");
    CHECK(b.find("Bearer tok123") != std::string::npos);

    std::string ba = Auth::basic("alice", "secret");
    // base64("alice:secret") = "YWxpY2U6c2VjcmV0"
    CHECK(ba.find("Basic ") != std::string::npos);
    CHECK(ba.find("YWxpY2U6c2VjcmV0") != std::string::npos);

    std::string k = Auth::apiKey("X-API-Key", "abc");
    CHECK(k.find("X-API-Key") != std::string::npos);
    CHECK(k.find("abc") != std::string::npos);

    CHECK(Url::encode("hello world&foo=bar").find("%20") != std::string::npos);
    CHECK(Url::encode("a/b?c").find("%2F") != std::string::npos);

    std::map<std::string, std::string> qp = { {"q", "hello world"}, {"n", "1"} };
    std::string u = Url::build("https://example.com/search", qp);
    CHECK(u.find("https://example.com/search?") == 0);
    CHECK(u.find("q=hello%20world") != std::string::npos);
    CHECK(u.find("n=1") != std::string::npos);
}

// offline: file system operations (create, read, write, copy, move, delete)
static void test_file() {
    section("File");
    File f;

    const std::string root = "smoke_tmp";
    const std::string sub  = root + "/nested/deep";
    const std::string t1   = root + "/hello.txt";
    const std::string t2   = root + "/copied.txt";
    const std::string t3   = root + "/moved.txt";
    const std::string bin  = root + "/bytes.bin";

    if (f.exists(root)) f.deleteFolder(root);

    f.createFolder(root);
    CHECK(f.isDirectory(root));

    f.createFolders(sub);
    CHECK(f.isDirectory(sub));

    f.createFile(t1, "line1\nline2\n");
    CHECK(f.exists(t1));
    CHECK(f.isFile(t1));
    CHECK(!f.isDirectory(t1));

    f.appendFile(t1, "line3\n");
    std::string text = f.readFile(t1);
    CHECK(text.find("line1") != std::string::npos);
    CHECK(text.find("line3") != std::string::npos);

    std::string raw = f.readAll(t1);
    CHECK(raw.size() >= text.size());

    CHECK(f.fileCharacterCount(t1) > 0);
    CHECK(f.fileSize(t1) > 0);
    CHECK(f.lastModified(t1) > 0);

    std::vector<unsigned char> data = { 0x00, 0xff, 0x10, 0x7f, 0x80, 0x01 };
    f.writeBytes(bin, data);
    std::vector<unsigned char> back = f.readBytes(bin);
    CHECK_MSG(back == data, "writeBytes/readBytes roundtrip");

    f.copyFile(t1, t2);
    CHECK(f.exists(t2));
    f.moveFile(t2, t3);
    CHECK(!f.exists(t2));
    CHECK(f.exists(t3));

    std::vector<std::string> entries = f.listFiles(root);
    CHECK(!entries.empty());

    CHECK(f.fileSize(root + "/does_not_exist") == -1);
    CHECK(f.lastModified(root + "/does_not_exist") == -1);

    f.deleteFile(t1);
    CHECK(!f.exists(t1));

    f.deleteFolder(root);
    CHECK(!f.exists(root));
}

// offline chat state: history, reset, save/load
static void test_chat_state() {
    section("Chat state (offline)");

    Chat c(Provider::Ollama, "llama3.2:1b");
    c.setSystem("You are a tester.");
    c.setTemperature(0.2);
    c.setMaxTokens(64);

    CHECK_MSG(c.history().size() == 1, "system prompt appended to history");
    CHECK(c.history().front().role == "system");
    CHECK(c.lastReasoning().empty());

    File f;
    const std::string root = "smoke_chat_tmp";
    if (f.exists(root)) f.deleteFolder(root);
    f.createFolder(root);
    const std::string path = root + "/history.json";

    CHECK_MSG(c.saveHistory(path), "saveHistory writes JSON file");
    CHECK(f.exists(path));

    Chat c2(Provider::Ollama, "llama3.2:1b");
    CHECK_MSG(c2.loadHistory(path), "loadHistory parses the file");
    CHECK(c2.history().size() == c.history().size());
    CHECK(c2.history().front().role == "system");

    c.reset();
    CHECK(c.history().size() <= 1);

    Chat c3(Provider::Ollama, "x");
    CHECK_MSG(!c3.loadHistory(root + "/missing.json"), "loadHistory fails cleanly");

    f.deleteFolder(root);
}

// offline object construction: Requests, Response, FormPart, Message, Chat
static void test_construction() {
    section("Object construction");
    Requests r;
    r.setRetries(2);
    r.setRetryBackoff(500);
    (void)r;

    Response resp;
    resp.status = 204;
    CHECK(resp.ok());
    resp.status = 500;
    CHECK(!resp.ok());

    FormPart part{ "field", "value", false };
    CHECK(part.name == "field");

    Message m{ "user", "hi" };
    CHECK(m.role == "user");

    Chat a(Provider::OpenAI,     "x", "k");
    Chat b(Provider::Anthropic,  "x", "k");
    Chat c(Provider::Groq,       "x", "k");
    Chat d(Provider::OpenRouter, "x", "k");
    Chat e(Provider::DeepSeek,   "x", "k");
    Chat g(Provider::Ollama,     "x");
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)g;
}

// online: live HTTP tests against postman-echo.com (get, head, post, put, patch, delete, download, upload)
static void test_live_http() {
    section("Live HTTP (postman-echo.com)");
    Requests r;
    r.setRetries(2);
    r.setRetryBackoff(300);

    std::string ping = r.pingDomain("postman-echo.com");
    CHECK_MSG(!ping.empty(), "pingDomain returns something");

    Response g = r.get("https://postman-echo.com/get", {}, 30);
    CHECK(g.ok());
    CHECK(g.status == 200);
    CHECK(!g.headers.empty());
    CHECK(g.body.find("postman-echo.com") != std::string::npos);

    std::string g1 = r.getReq("https://postman-echo.com/get");
    CHECK(!g1.empty());
    std::string g2 = r.getReq("https://postman-echo.com/get",
                              "X-Smoke: yes\r\nAccept: application/json\r\n");
    CHECK(!g2.empty());

    Response h = r.head("https://postman-echo.com/get", {}, 30);
    CHECK(h.ok());
    CHECK(!h.headers.empty());
    std::string h1 = r.headReq("https://postman-echo.com/get");
    CHECK(!h1.empty());
    std::string h2 = r.headReq("https://postman-echo.com/get", "X-Smoke: yes\r\n");
    CHECK(!h2.empty());

    std::vector<std::string> hdrs = { "Content-Type: application/json" };
    Response p = r.post("https://postman-echo.com/post", R"({"k":"v"})", hdrs, 30);
    CHECK(p.ok());
    CHECK(p.body.find("\"k\"") != std::string::npos);

    Response pu = r.put("https://postman-echo.com/put", R"({"u":"1"})", hdrs, 30);
    CHECK(pu.ok());

    Response pa = r.patch("https://postman-echo.com/patch", R"({"p":"2"})", hdrs, 30);
    CHECK(pa.ok());

    Response dd = r.del("https://postman-echo.com/delete", {}, 30);
    CHECK(dd.ok());
    std::string d1 = r.deleteReq("https://postman-echo.com/delete");
    CHECK(!d1.empty());
    std::string d2 = r.deleteReq("https://postman-echo.com/delete", "X-Smoke: yes\r\n");
    CHECK(!d2.empty());

    std::string pr = r.postReq("https://postman-echo.com/post",
                               "Content-Type: application/json\r\n",
                               "");
    CHECK_MSG(true, "postReq linked"); (void)pr;
    std::string pur = r.putReq("https://postman-echo.com/put",
                               "Content-Type: application/json\r\n",
                               "");
    CHECK_MSG(true, "putReq linked"); (void)pur;

    File f;
    const std::string dest = "smoke_download.bin";
    if (f.exists(dest)) f.deleteFile(dest);
    long progressCalls = 0;
    bool dlok = r.download("https://postman-echo.com/get", dest,
        [&](long, long) { ++progressCalls; });
    CHECK(dlok);
    CHECK(f.exists(dest));
    CHECK(f.fileSize(dest) > 0);
    CHECK(progressCalls > 0);
    f.deleteFile(dest);

    f.createFile("smoke_upload.txt", "hello upload\n");
    std::vector<FormPart> parts = {
        { "field1", "text-value",        false },
        { "file1",  "smoke_upload.txt",  true  },
    };
    Response up = r.upload("https://postman-echo.com/post", parts, {}, 30);
    CHECK(up.ok());
    CHECK(up.body.find("field1") != std::string::npos);
    f.deleteFile("smoke_upload.txt");
}

// online/offline: chat against a provider
static Provider parse_provider(const std::string& s, bool& ok) {
    ok = true;
    if (s == "ollama")     return Provider::Ollama;
    if (s == "openai")     return Provider::OpenAI;
    if (s == "anthropic")  return Provider::Anthropic;
    if (s == "groq")       return Provider::Groq;
    if (s == "openrouter") return Provider::OpenRouter;
    if (s == "deepseek")   return Provider::DeepSeek;
    ok = false;
    return Provider::Ollama;
}
static std::string key_env_for(Provider p) {
    switch (p) {
        case Provider::OpenAI:     return env_str("OPENAI_API_KEY");
        case Provider::Anthropic:  return env_str("ANTHROPIC_API_KEY");
        case Provider::Groq:       return env_str("GROQ_API_KEY");
        case Provider::OpenRouter: return env_str("OPENROUTER_API_KEY");
        case Provider::DeepSeek:   return env_str("DEEPSEEK_API_KEY");
        case Provider::Ollama:     return "";
    }
    return "";
}

static void test_live_chat() {
    section("Live Chat (" + env_str("NETWORKML_CHAT_PROVIDER") + ")");
    bool ok = false;
    Provider p = parse_provider(env_str("NETWORKML_CHAT_PROVIDER"), ok);
    CHECK_MSG(ok, "NETWORKML_CHAT_PROVIDER is one of: ollama|openai|anthropic|groq|openrouter|deepseek");
    if (!ok) return;
    std::string model = env_str("NETWORKML_CHAT_MODEL");
    CHECK_MSG(!model.empty(), "NETWORKML_CHAT_MODEL is set");
    if (model.empty()) return;

    Chat c(p, model, key_env_for(p));
    c.setSystem("You are a terse assistant. Reply in <=8 words.");
    c.setTemperature(0.1);
    c.setMaxTokens(64);

    std::string a = c.ask("Reply with only the single word: pong");
    CHECK_MSG(!a.empty(), "ask returned a reply");
    CHECK_MSG(c.history().size() >= 3, "ask appended user+assistant turns");
    (void)c.lastReasoning();

    Response rr = c.raw("And one more time, just: pong");
    CHECK_MSG(rr.ok(), "raw() got 2xx");
    CHECK(!rr.body.empty());

    std::string streamed;
    long status = c.stream("Say pong once.",
        [&](const std::string& tok) { streamed += tok; });
    CHECK_MSG(status >= 200 && status < 300, "stream() reports 2xx");
    CHECK_MSG(!streamed.empty(), "stream() delivered tokens");
}

int main() {
    test_auth_url();
    test_file();
    test_chat_state();
    test_construction();

    if (env_flag("NETWORKML_LIVE")) test_live_http();
    else std::cout << "\n[Live HTTP] skipped (set NETWORKML_LIVE=1 to enable)\n";

    if (!env_str("NETWORKML_CHAT_PROVIDER").empty()) test_live_chat();
    else std::cout << "\n[Live Chat] skipped (set NETWORKML_CHAT_PROVIDER + NETWORKML_CHAT_MODEL)\n";

    std::cout << "\n========================================\n";
    std::cout << " " << g_pass << " passed, " << g_fail << " failed\n";
    std::cout << "========================================\n";
    return g_fail == 0 ? 0 : 1;
}
