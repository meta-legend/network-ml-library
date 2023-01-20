#include <iostream>
#include "networkml.h"

using namespace std;

int main() {
	string file;
	cout << "Enter your file name: ";
	cin >> file;
	cout << readFile(file) << endl;
	cin.get();
	cin.get();
}