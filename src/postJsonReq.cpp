#include <iostream>
#include <fstream>
#include <string>
#include "networkml.h"

using namespace std;

string postJsonReq(string url, string jsonPayload) {
    string command = "curl -v -X POST -H 'Content-Type: application/json' -d '" + jsonPayload + "' " + url + " > output.txt";
    system(command.c_str());

    ifstream inFile("output.txt");
    string fileContent;

    if (inFile.is_open()) {
        string line;
        while (getline(inFile, line)) {
            fileContent += line;
            fileContent.push_back('\n');
        }
        inFile.close();
    }
    else {
        return "Unable to open file";
    }
    return fileContent;
}