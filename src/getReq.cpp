#include <iostream>
#include <fstream>
#include <string>
#include "networkml.h"

using namespace std;

string getReq(string url) {
    string command = "curl -X GET " + url + " -o output.txt";
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

