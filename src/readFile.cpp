#include <iostream>
#include <fstream>
#include <string>
#include "networkml.h"

using namespace std;

string readFile(string name) {
    ifstream inFile(name);
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