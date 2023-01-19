#include <iostream>
#include <fstream>
#include <string>
#include "networkml.h"

using namespace std;

string pingDomain(string domain) {
    string command = "curl " + domain + " -o domainOutput.txt";
    system(command.c_str());

    ifstream inFile("domainOutput.txt");
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
        return "[ERROR] File cannot be opened";
    }

    return fileContent;
}
