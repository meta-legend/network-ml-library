#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>
#include <cctype>
#include <curl/curl.h>
#include "networkml.h"

// See Chat.cpp: auto-link libcurl's Windows system-library dependencies so
// consumers only have to link networkml.lib (no effect off Windows/MSVC).
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

namespace ML {

    // --- file-local helpers ---------------------------------------------------

    // libcurl write callback: append received bytes to a std::string.
    static size_t reqWrite(char* ptr, size_t size, size_t nmemb, void* userdata) {
        size_t n = size * nmemb;
        static_cast<std::string*>(userdata)->append(ptr, n);
        return n;
    }

    // Perform a configured easy handle and return the response body (or an
    // error string on transport failure). Does NOT clean up the handle.
    static std::string perform(CURL* curl) {
        std::string body;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, reqWrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            return std::string("[ERROR] ") + curl_easy_strerror(res);
        }
        return body;
    }

    // Read a whole file into a string (used by the file-based POST/PUT methods,
    // preserving the original "pass a payload file name" behaviour).
    static std::string readFileContents(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    // Captures each response header line into a map with lowercased keys.
    static size_t headerCb(char* buffer, size_t size, size_t nitems, void* userdata) {
        size_t n = size * nitems;
        auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
        std::string line(buffer, n);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            size_t start = val.find_first_not_of(' ');
            val = (start == std::string::npos) ? "" : val.substr(start);
            for (char& c : key) c = static_cast<char>(std::tolower((unsigned char)c));
            (*headers)[key] = val;
        }
        return n;
    }

    // One HTTP request: builds it from a method + optional body + header list +
    // timeout, and fills a full Response.
    static Response doSingleRequest(const std::string& method, const std::string& url,
        const std::string& body, const std::vector<std::string>& headers,
        long timeoutSeconds, bool noBody) {
        Response r;
        CURL* curl = curl_easy_init();
        if (!curl) return r;

        curl_slist* hdrs = nullptr;
        for (const auto& h : headers) hdrs = curl_slist_append(hdrs, h.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        if (hdrs) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        }
        if (noBody) curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        if (timeoutSeconds > 0) curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, reqWrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r.body);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCb);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &r.headers);

        if (curl_easy_perform(curl) == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status);
        }
        curl_slist_free_all(hdrs);
        curl_easy_cleanup(curl);
        return r;
    }

    // Wraps doSingleRequest with auto-retry on 429 / 5xx. Honors the server's
    // Retry-After header when present, else exponential backoff.
    static Response performRequest(const std::string& method, const std::string& url,
        const std::string& body, const std::vector<std::string>& headers,
        long timeoutSeconds, bool noBody, int maxRetries, int backoffMs) {
        Response r;
        for (int attempt = 0; ; ++attempt) {
            r = doSingleRequest(method, url, body, headers, timeoutSeconds, noBody);

            bool retryable = (r.status == 429) || (r.status >= 500 && r.status < 600);
            if (!retryable || attempt >= maxRetries) break;

            long delayMs = (long)backoffMs << attempt;   // exponential: base, 2x, 4x...
            auto it = r.headers.find("retry-after");
            if (it != r.headers.end()) {
                try { delayMs = std::stol(it->second) * 1000; } catch (...) {}
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
        return r;
    }

    void Requests::setRetries(int retries) { maxRetries = retries; }
    void Requests::setRetryBackoff(int baseMs) { retryBackoffMs = baseMs; }

    // --- modern verb methods (full Response) ---------------------------------

    Response Requests::get(std::string url, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("GET", url, "", headers, timeoutSeconds, false, maxRetries, retryBackoffMs);
    }
    Response Requests::post(std::string url, std::string body, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("POST", url, body, headers, timeoutSeconds, false, maxRetries, retryBackoffMs);
    }
    Response Requests::put(std::string url, std::string body, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("PUT", url, body, headers, timeoutSeconds, false, maxRetries, retryBackoffMs);
    }
    Response Requests::patch(std::string url, std::string body, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("PATCH", url, body, headers, timeoutSeconds, false, maxRetries, retryBackoffMs);
    }
    Response Requests::del(std::string url, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("DELETE", url, "", headers, timeoutSeconds, false, maxRetries, retryBackoffMs);
    }
    Response Requests::head(std::string url, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("HEAD", url, "", headers, timeoutSeconds, true, maxRetries, retryBackoffMs);
    }

    // --- download (stream to file) -------------------------------------------

    static size_t writeToFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
        std::ofstream* out = static_cast<std::ofstream*>(userdata);
        out->write(ptr, static_cast<std::streamsize>(size * nmemb));
        return size * nmemb;
    }

    static int progressCb(void* userdata, curl_off_t dltotal, curl_off_t dlnow,
        curl_off_t, curl_off_t) {
        auto* cb = static_cast<std::function<void(long, long)>*>(userdata);
        if (cb && *cb) (*cb)(static_cast<long>(dlnow), static_cast<long>(dltotal));
        return 0;
    }

    bool Requests::download(std::string url, std::string filePath,
        std::function<void(long, long)> onProgress) {
        std::ofstream out(filePath, std::ios::binary);
        if (!out.is_open()) return false;

        CURL* curl = curl_easy_init();
        if (!curl) return false;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
        if (onProgress) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCb);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &onProgress);
        }

        long status = 0;
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_cleanup(curl);
        out.close();
        return res == CURLE_OK && status >= 200 && status < 300;
    }

    // --- multipart upload ----------------------------------------------------

    Response Requests::upload(std::string url, std::vector<FormPart> parts,
        std::vector<std::string> headers, long timeoutSeconds) {
        Response r;
        CURL* curl = curl_easy_init();
        if (!curl) return r;

        curl_mime* mime = curl_mime_init(curl);
        for (const auto& p : parts) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, p.name.c_str());
            if (p.isFile) curl_mime_filedata(part, p.content.c_str());
            else          curl_mime_data(part, p.content.c_str(), CURL_ZERO_TERMINATED);
        }

        curl_slist* hdrs = nullptr;
        for (const auto& h : headers) hdrs = curl_slist_append(hdrs, h.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        if (hdrs) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        if (timeoutSeconds > 0) curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, reqWrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r.body);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCb);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &r.headers);

        if (curl_easy_perform(curl) == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status);
        }
        curl_mime_free(mime);
        curl_slist_free_all(hdrs);
        curl_easy_cleanup(curl);
        return r;
    }

    // --- auth & URL helpers --------------------------------------------------

    namespace Auth {
        static std::string base64(const std::string& in) {
            static const char* T =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out;
            int val = 0, bits = -6;
            for (unsigned char c : in) {
                val = (val << 8) + c;
                bits += 8;
                while (bits >= 0) {
                    out.push_back(T[(val >> bits) & 0x3F]);
                    bits -= 6;
                }
            }
            if (bits > -6) out.push_back(T[((val << 8) >> (bits + 8)) & 0x3F]);
            while (out.size() % 4) out.push_back('=');
            return out;
        }

        std::string bearer(const std::string& token) {
            return "Authorization: Bearer " + token;
        }
        std::string basic(const std::string& user, const std::string& password) {
            return "Authorization: Basic " + base64(user + ":" + password);
        }
        std::string apiKey(const std::string& headerName, const std::string& key) {
            return headerName + ": " + key;
        }
    }

    namespace Url {
        std::string encode(const std::string& value) {
            CURL* curl = curl_easy_init();
            if (!curl) return value;
            char* out = curl_easy_escape(curl, value.c_str(), (int)value.size());
            std::string result = out ? out : value;
            if (out) curl_free(out);
            curl_easy_cleanup(curl);
            return result;
        }
        std::string build(const std::string& base,
            const std::map<std::string, std::string>& params) {
            if (params.empty()) return base;
            std::string url = base;
            url += (base.find('?') == std::string::npos) ? "?" : "&";
            bool first = true;
            for (const auto& kv : params) {
                if (!first) url += "&";
                url += encode(kv.first) + "=" + encode(kv.second);
                first = false;
            }
            return url;
        }
    }

    // --- legacy string-returning methods (all libcurl-based) -----------------

    std::string Requests::pingDomain(std::string domain) {
        CURL* curl = curl_easy_init();
        if (!curl) return "[ERROR] curl init failed";
        curl_easy_setopt(curl, CURLOPT_URL, domain.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        std::string body = perform(curl);
        curl_easy_cleanup(curl);
        return body;
    }

    std::string Requests::getReq(std::string url) {
        CURL* curl = curl_easy_init();
        if (!curl) return "[ERROR] curl init failed";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        std::string body = perform(curl);
        curl_easy_cleanup(curl);
        return body;
    }

    std::string Requests::getReq(std::string url, std::string headers) {
        CURL* curl = curl_easy_init();
        if (!curl) return "[ERROR] curl init failed";
        curl_slist* h = curl_slist_append(nullptr, headers.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        std::string body = perform(curl);
        curl_slist_free_all(h);
        curl_easy_cleanup(curl);
        return body;
    }

    // Second argument is a file name whose contents become the POST body
    // (kept for backwards compatibility with the original API).
    std::string Requests::postReq(std::string url, std::string payloadFileName, std::string headers) {
        CURL* curl = curl_easy_init();
        if (!curl) return "[ERROR] curl init failed";
        std::string payload = readFileContents(payloadFileName);
        curl_slist* h = curl_slist_append(nullptr, headers.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload.size());
        std::string body = perform(curl);
        curl_slist_free_all(h);
        curl_easy_cleanup(curl);
        return body;
    }

    std::string Requests::headReq(std::string url) {
        CURL* curl = curl_easy_init();
        if (!curl) return "[ERROR] curl init failed";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);   // HEAD request
        curl_easy_setopt(curl, CURLOPT_HEADER, 1L);   // include headers in output
        std::string body = perform(curl);
        curl_easy_cleanup(curl);
        return body;
    }

    std::string Requests::headReq(std::string url, std::string headers) {
        CURL* curl = curl_easy_init();
        if (!curl) return "[ERROR] curl init failed";
        curl_slist* h = curl_slist_append(nullptr, headers.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
        std::string body = perform(curl);
        curl_slist_free_all(h);
        curl_easy_cleanup(curl);
        return body;
    }

    // Third argument is a file name whose contents become the PUT body.
    std::string Requests::putReq(std::string url, std::string headers, std::string payloadFileName) {
        CURL* curl = curl_easy_init();
        if (!curl) return "[ERROR] curl init failed";
        std::string payload = readFileContents(payloadFileName);
        curl_slist* h = curl_slist_append(nullptr, headers.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload.size());
        std::string body = perform(curl);
        curl_slist_free_all(h);
        curl_easy_cleanup(curl);
        return body;
    }

    std::string Requests::deleteReq(std::string url) {
        CURL* curl = curl_easy_init();
        if (!curl) return "[ERROR] curl init failed";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        std::string body = perform(curl);
        curl_easy_cleanup(curl);
        return body;
    }

    std::string Requests::deleteReq(std::string url, std::string headers) {
        CURL* curl = curl_easy_init();
        if (!curl) return "[ERROR] curl init failed";
        curl_slist* h = curl_slist_append(nullptr, headers.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        std::string body = perform(curl);
        curl_slist_free_all(h);
        curl_easy_cleanup(curl);
        return body;
    }
}
