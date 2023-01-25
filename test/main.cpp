#include <iostream>
#include "networkml.h"

using namespace ML;

int main() {
	File file = File();
	Requests requests = Requests();
	
	// [EXAMPLE]: For the Requests class
	// also by the way every function has a overload besides the pingDomain function so you can put custom headers as a optional second parameter
	// for postReq and putReq though custom headers are required
	//std::string exampleUrl = "https://www.google.com";
	//std::string jsonFileName = "payload.json";
	//file.createFile("pingDomainResult.txt", requests.pingDomain(exampleUrl));
	//std::cout << file.readFile("pingDomainResult.txt") << std::endl;
	// cout << requests.postReq(exampleUrl, "your headers","the file name of the file containing your body") << std::endl;
	//file.createFile("getReqResult.txt", requests.getReq(exampleUrl));
	//std::cout << file.readFile("getReqResult.txt") << std::endl;
	//file.createFile("headReqResult.txt", requests.headReq(exampleUrl));
	//std::cout << file.readFile("headReqResult.txt") << std::endl;
	//file.createFolder("Test");
	// cout << requests.putReq("your_put_url", "your headers", "your file name of the file containing your body") << std::endl;
	// std::cout << requests.deleteReq("your_delete_url");
	/*
	// [EXAMPLE]: For the File class
	file.createFile("test.txt", "This\nis\na\ntest\ndocument");
	std::cout << file.readFile("test.txt") << std::endl;
	std::cout << file.fileCharacterCount("test.txt") << std::endl;
	file.deleteFile("test.txt");
	*/
	return 0;
}