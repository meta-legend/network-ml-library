#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <fstream>
#include <stdio.h>
namespace ML {
	// Rich result of an HTTP request: status code + body.
	// ok() is true for any 2xx status.
	struct Response {
		long status = 0;
		std::string body;
		bool ok() const { return status >= 200 && status < 300; }
	};

	// One turn in a conversation.
	struct Message {
		std::string role;     // "system" | "user" | "assistant"
		std::string content;
	};

	class File {
	public:
		void createFolder(std::string name);
		void createFile(std::string name, std::string content);
		std::string readFile(std::string name);
		void deleteFile(std::string name);
		int fileCharacterCount(std::string name);
	};

	class Requests {
	public:
		std::string pingDomain(std::string domain);
		std::string getReq(std::string url);
		std::string getReq(std::string url, std::string headers);
		std::string postReq(std::string url, std::string jsonPayload, std::string headers);
		std::string headReq(std::string url);
		std::string headReq(std::string url, std::string headers);
		std::string putReq(std::string url, std::string headers, std::string payload);
		std::string deleteReq(std::string url);
		std::string deleteReq(std::string url, std::string headers);

		// Newer API: returns a rich Response (status + body) instead of a bare string.
		Response get(std::string url);
	};

	// Conversational client for a local Ollama server (http://localhost:11434).
	// Holds a running message history so the model remembers context across calls.
	class Chat {
	public:
		Chat(std::string model = "llama3.2:1b",
			std::string host = "http://localhost:11434");

		// Set a persistent system instruction (model behaviour/persona).
		void setSystem(std::string instruction);

		// Sampling temperature: 0.0 = focused/deterministic, ~1.0 = creative.
		void setTemperature(double temperature);

		// Sends the prompt WITH the running history and returns the reply text.
		// On success, both the prompt and reply are appended to history.
		// Returns an empty string on failure.
		std::string ask(std::string prompt);

		// Lower-level, stateless: sends the prompt (plus existing history) and
		// returns the full HTTP Response without modifying history.
		Response raw(std::string prompt);

		// Streams the reply token-by-token as it is generated. onToken is called
		// repeatedly with each chunk of text. On success the exchange is appended
		// to history (same as ask()).
		void stream(std::string prompt, std::function<void(const std::string&)> onToken);

		// Clears conversation history (keeps the system instruction, if any).
		void reset();

		// Read-only access to the current conversation.
		const std::vector<Message>& history() const;

	private:
		std::string buildBody(const std::string& prompt, bool streaming);

		std::string model;
		std::string host;
		std::vector<Message> messages;
		double temperature;
	};
}
