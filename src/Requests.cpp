#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include "networkml.h"

namespace ML {
    std::string Requests::pingDomain(std::string domain) {
        std::string command = "curl " + domain + " -o domainOutput.txt";
        system(command.c_str());

        std::ifstream inFile("domainOutput.txt");
        std::string fileContent;

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                fileContent += line;
                fileContent.push_back('\n');
            }
            inFile.close();
        }
        else {
            return "[ERROR] File cannot be opened";
        }

        std::remove("output.txt");

        return fileContent;
    }

    std::string Requests::getReq(std::string url) {
        std::string command = "curl -X GET " + url + " -o output.txt";
        system(command.c_str());

        std::ifstream inFile("output.txt");
        std::string fileContent;

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                fileContent += line;
                fileContent.push_back('\n');
            }
            inFile.close();
        }
        else {
            return "Unable to open file";
        }

        std::remove("output.txt");

        return fileContent;
    }


    std::string Requests::getReq(std::string url, std::string headers) {
        std::string command = "curl -X GET -H '" + headers + "' " + url + " -o output.txt";
        system(command.c_str());

        std::ifstream inFile("output.txt");
        std::string fileContent;

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                fileContent += line;
                fileContent.push_back('\n');
            }
            inFile.close();
        }
        else {
            return "Unable to open file";
        }

        std::remove("output.txt");

        return fileContent;
    }

    std::string Requests::postReq(std::string url, std::string payloadFileName, std::string headers) {
        std::string command = "curl -X POST -H \"" + headers + "\" -d \"@" + payloadFileName + "\" " + url + " > output.txt";
        system(command.c_str());

        std::ifstream inFile("output.txt");
        std::string fileContent;

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                fileContent += line;
                fileContent.push_back('\n');
            }
            inFile.close();
        }
        else {
            return "Unable to open file";
        }

        std::remove("output.txt");

        return fileContent;
    }

    std::string Requests::headReq(std::string url) {
        std::string command = "curl -I " + url + " > output.txt";
        system(command.c_str());

        std::ifstream inFile("output.txt");
        std::string fileContent;

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                fileContent += line;
                fileContent.push_back('\n');
            }
            inFile.close();
        }
        else {
            return "Unable to open file";
        }

        std::remove("output.txt");

        return fileContent;
    }

    std::string Requests::headReq(std::string url, std::string headers) {
        std::string command = "curl -I -H '" + headers + "' " + url + " > output.txt";
        system(command.c_str());

        std::ifstream inFile("output.txt");
        std::string fileContent;

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                fileContent += line;
                fileContent.push_back('\n');
            }
            inFile.close();
        }
        else {
            return "Unable to open file";
        }

        std::remove("output.txt");

        return fileContent;
    }

    std::string Requests::putReq(std::string url, std::string headers, std::string payloadFileName) {
        std::string command = "curl -X PUT -H \"" + headers + "\" -d \"@" + payloadFileName + "\" " + url + " > output.txt";
        std::cout << command << std::endl;
        system(command.c_str());

        std::ifstream inFile("output.txt");
        std::string fileContent;

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                fileContent += line;
                fileContent.push_back('\n');
            }
            inFile.close();
        }
        else {
            return "Unable to open file";
        }

        std::remove("output.txt");

        return fileContent;
    }

    std::string Requests::deleteReq(std::string url) {
        std::string command = "curl -X DELETE " + url + " > output.txt";
        system(command.c_str());

        std::ifstream inFile("output.txt");
        std::string fileContent;

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                fileContent += line;
                fileContent.push_back('\n');
            }
            inFile.close();
        }
        else {
            return "Unable to open file";
        }

        std::remove("output.txt");

        return fileContent;
    }

    std::string Requests::deleteReq(std::string url, std::string headers) {
        std::string command = "curl -X DELETE -H '" + headers + "' " + url + " > output.txt";
        system(command.c_str());

        std::ifstream inFile("output.txt");
        std::string fileContent;

        if (inFile.is_open()) {
            std::string line;
            while (std::getline(inFile, line)) {
                fileContent += line;
                fileContent.push_back('\n');
            }
            inFile.close();
        }
        else {
            return "Unable to open file";
        }

        std::remove("output.txt");

        return fileContent;
    }
}

