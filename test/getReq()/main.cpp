#include <iostream>
#include "networkml.h"

using namespace std;

int main() {
	string url;
	cout << "Enter your file url: ";
	cin >> url;
	cout << getReq(url) << endl;
	cin.get();
	cin.get();
}