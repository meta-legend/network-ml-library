#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <cctype>
#include <curl/curl.h>
#include "networkml.h"

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

    // Core request used by the modern verb methods: builds the request from a
    // method + optional body + header list + timeout, and fills a full Response.
    static Response performRequest(const std::string& method, const std::string& url,
        const std::string& body, const std::vector<std::string>& headers,
        long timeoutSeconds, bool noBody = false) {
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

    // --- modern verb methods (full Response) ---------------------------------

    Response Requests::get(std::string url, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("GET", url, "", headers, timeoutSeconds);
    }
    Response Requests::post(std::string url, std::string body, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("POST", url, body, headers, timeoutSeconds);
    }
    Response Requests::put(std::string url, std::string body, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("PUT", url, body, headers, timeoutSeconds);
    }
    Response Requests::patch(std::string url, std::string body, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("PATCH", url, body, headers, timeoutSeconds);
    }
    Response Requests::del(std::string url, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("DELETE", url, "", headers, timeoutSeconds);
    }
    Response Requests::head(std::string url, std::vector<std::string> headers, long timeoutSeconds) {
        return performRequest("HEAD", url, "", headers, timeoutSeconds, /*noBody=*/true);
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
