#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>
#include <filesystem>
#include <system_error>
#include <chrono>
#include "networkml.h"

namespace ML {

    // create and write

    void File::createFolder(std::string name) {
        std::error_code ec;
        if (!std::filesystem::create_directory(name, ec)) {
            if (ec) std::cerr << "Error creating folder: " << ec.message() << '\n';
            else    std::cerr << "Error creating folder: Folder already exists" << '\n';
        }
    }

    void File::createFolders(std::string path) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec) std::cerr << "Error creating folders: " << ec.message() << '\n';
    }

    void File::createFile(std::string name, std::string content) {
        std::ofstream file(name, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error creating file: cannot open " << name << '\n';
            return;
        }
        file << content;
    }

    void File::appendFile(std::string name, std::string content) {
        std::ofstream file(name, std::ios::binary | std::ios::app);
        if (!file.is_open()) {
            std::cerr << "Error appending file: cannot open " << name << '\n';
            return;
        }
        file << content;
    }

    void File::writeBytes(std::string name, const std::vector<unsigned char>& data) {
        std::ofstream file(name, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error writing file: cannot open " << name << '\n';
            return;
        }
        file.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    }

    // read

    std::string File::readFile(std::string name) {
        std::ifstream inFile(name);
        if (!inFile.is_open()) {
            std::cerr << "Error reading file: cannot open " << name << '\n';
            return "Unable to open file";
        }
        std::string fileContent, line;
        while (inFile >> std::ws && std::getline(inFile, line)) {
            fileContent += line;
            fileContent.push_back('\n');
        }
        return fileContent;
    }

    std::string File::readAll(std::string name) {
        std::ifstream inFile(name, std::ios::binary);
        if (!inFile.is_open()) {
            std::cerr << "Error reading file: cannot open " << name << '\n';
            return "";
        }
        std::ostringstream ss;
        ss << inFile.rdbuf();   // exact bytes (no whitespace mangling)
        return ss.str();
    }

    std::vector<unsigned char> File::readBytes(std::string name) {
        std::ifstream inFile(name, std::ios::binary);
        if (!inFile.is_open()) {
            std::cerr << "Error reading file: cannot open " << name << '\n';
            return {};
        }
        return std::vector<unsigned char>(
            (std::istreambuf_iterator<char>(inFile)),
            std::istreambuf_iterator<char>());
    }

    // delete, copy, and move

    void File::deleteFile(std::string name) {
        std::error_code ec;
        if (!std::filesystem::remove(name, ec)) {
            if (ec) std::cerr << "Error deleting file: " << ec.message() << '\n';
            else    std::cerr << "Error deleting file: File does not exist" << '\n';
        }
    }

    void File::deleteFolder(std::string name) {
        std::error_code ec;
        std::filesystem::remove_all(name, ec);
        if (ec) std::cerr << "Error deleting folder: " << ec.message() << '\n';
    }

    void File::copyFile(std::string src, std::string dst) {
        std::error_code ec;
        std::filesystem::copy_file(src, dst,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) std::cerr << "Error copying file: " << ec.message() << '\n';
    }

    void File::moveFile(std::string src, std::string dst) {
        std::error_code ec;
        std::filesystem::rename(src, dst, ec);
        if (ec) std::cerr << "Error moving file: " << ec.message() << '\n';
    }

	// listing, queries, and metadata

    std::vector<std::string> File::listFiles(std::string folder) {
        std::vector<std::string> entries;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
            entries.push_back(entry.path().filename().string());
        }
        if (ec) std::cerr << "Error listing folder: " << ec.message() << '\n';
        return entries;
    }

    bool File::exists(std::string name) {
        std::error_code ec;
        return std::filesystem::exists(name, ec);
    }

    bool File::isFile(std::string name) {
        std::error_code ec;
        return std::filesystem::is_regular_file(name, ec);
    }

    bool File::isDirectory(std::string name) {
        std::error_code ec;
        return std::filesystem::is_directory(name, ec);
    }

    int File::fileCharacterCount(std::string name) {
        return static_cast<int>(readAll(name).length());
    }

    long File::fileSize(std::string name) {
        std::error_code ec;
        auto size = std::filesystem::file_size(name, ec);
        if (ec) return -1;
        return static_cast<long>(size);
    }

    long long File::lastModified(std::string name) {
        std::error_code ec;
        auto ftime = std::filesystem::last_write_time(name, ec);
        if (ec) return -1;
        // Convert the filesystem clock to system_clock 
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now()
                  + std::chrono::system_clock::now());
        return static_cast<long long>(std::chrono::system_clock::to_time_t(sctp));
    }
}
