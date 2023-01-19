#pragma once
#include <string>
#include <fstream>

using namespace std;

string pingDomain(string domain);
string getReq(string url);
string postJsonReq(string url, string jsonPayload);
string readFile(string name);