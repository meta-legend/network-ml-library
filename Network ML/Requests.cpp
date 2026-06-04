#include <string>
#include <fstream>
#include <sstream>
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

    // --- Requests methods (all libcurl-based) --------------------------------

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
