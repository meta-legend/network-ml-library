#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <system_error>
#include "networkml.h"

namespace ML {
    // Cross-platform: std::filesystem works identically on Windows, macOS, Linux.
    void File::createFolder(std::string name) {
        std::error_code ec;
        bool created = std::filesystem::create_directory(name, ec);
        if (!created) {
            if (ec) {
                std::cerr << "Error creating folder: " << ec.message() << '\n';
            }
            else {
                // create_directory returns false (with no error) when it already exists
                std::cerr << "Error creating folder: Folder already exists" << '\n';
            }
        }
    }

    void File::createFile(std::string name, std::string content) {
        std::ofstream file;
        file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        try {
            file.open(name);
            file << content;
            file.close();
        }
        catch (const std::ofstream::failure& e) {
            std::cerr << "Error creating file: " << e.what() << '\n';
        }
    }

    std::string File::readFile(std::string name) {
        std::ifstream inFile;
        inFile.exceptions(std::ifstream::badbit);
        std::string fileContent;
        try {
            inFile.open(name);
            std::string line;
            while (inFile >> std::ws && std::getline(inFile, line)) {
                fileContent += line;
                fileContent.push_back('\n');
            }
            inFile.close();
        }
        catch (const std::ifstream::failure& e) {
            std::cerr << "Error reading file: " << e.what() << '\n';
            return "Unable to open file";
        }
        return fileContent;
    }

    void File::deleteFile(std::string name) {
        std::error_code ec;
        bool removed = std::filesystem::remove(name, ec);
        if (!removed) {
            if (ec) {
                std::cerr << "Error deleting file: " << ec.message() << '\n';
            }
            else {
                std::cerr << "Error deleting file: File does not exist" << '\n';
            }
        }
    }

    int File::fileCharacterCount(std::string name) {
        std::string fileContent = readFile(name);
        return static_cast<int>(fileContent.length());
    }
}
