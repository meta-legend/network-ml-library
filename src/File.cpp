#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <cstdio>
#include <cerrno>
#include <windows.h>
#include <locale>
#include <codecvt>
#include "networkml.h"

namespace ML {
    void File::createFolder(std::string name) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wname = converter.from_bytes(name);
        if (CreateDirectoryW(wname.c_str(), NULL) == 0) {
            DWORD error = GetLastError();
            if (error == ERROR_ALREADY_EXISTS) {
                std::cerr << "Error creating folder: Folder already exists" << '\n';
            }
            else if (error == ERROR_ACCESS_DENIED) {
                std::cerr << "Error creating folder: Permission denied" << '\n';
            }
            else {
                std::cerr << "Error creating folder: " << error << '\n';
            }
        }
    }

    void File::createFile(std::string name, std::string content) {
        std::ofstream file;
        file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        try {
            file.open(name);
            file << content;
        }
        catch (const std::ofstream::failure& e) {
            if (errno == EACCES) {
                std::cerr << "Error creating file: Permission denied" << '\n';
            }
            else {
                std::cerr << "Error creating file: " << e.what() << '\n';
            }
        }
        file.close();
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
        }
        catch (const std::ifstream::failure& e) {
            std::cerr << "Error reading file: " << e.what() << '\n';
            return "Unable to open file";
        }

        inFile.close();
        return fileContent;
    }




    void File::deleteFile(std::string name) {
        int status = remove(name.c_str());
        if (status != 0) {
            char error_message[128];
            if (errno == EACCES) {
                std::cerr << "Error deleting file: Permission denied" << '\n';
            }
            else {
                strerror_s(error_message, 128, errno);
                std::cerr << "Error deleting file: " << error_message << '\n';
            }
        }
    }



    int File::fileCharacterCount(std::string name) {
        std::string fileContent = readFile(name);
        return fileContent.length();
    }
}
