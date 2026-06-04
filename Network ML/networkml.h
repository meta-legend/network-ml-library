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

	// LLM backends supported by Chat.
	// (Groq, OpenRouter and DeepSeek use the same wire format as OpenAI
	// internally, but are distinct choices so callers don't have to know that or
	// set a custom host.)
	enum class Provider { Ollama, OpenAI, Anthropic, Groq, OpenRouter, DeepSeek };

	// Conversational LLM client. Talks to a local Ollama server by default, or
	// to OpenAI / Anthropic when constructed with a Provider and an API key.
	// Holds a running message history so the model remembers context across calls.
	class Chat {
	public:
		// Construct for any provider. The provider is always explicit; the API
		// key is only used by cloud providers (Ollama and other local backends
		// ignore it). Leave host empty to use the provider's default endpoint
		// (http://localhost:11434 for Ollama).
		Chat(Provider provider, std::string model, std::string apiKey = "",
			std::string host = "");

		// Null-safe overload: apiKey may come straight from std::getenv and be
		// null; a null key is treated as empty so a missing environment variable
		// does not crash the caller.
		Chat(Provider provider, std::string model, const char* apiKey,
			std::string host = "");

		// Set a persistent system instruction (model behaviour/persona).
		void setSystem(std::string instruction);

		// Sampling temperature: 0.0 = focused/deterministic, ~1.0 = creative.
		void setTemperature(double temperature);

		// Max tokens to generate. Required by Anthropic; optional for others.
		void setMaxTokens(int maxTokens);

		// Sends the prompt WITH the running history and returns the reply text.
		// On success, both the prompt and reply are appended to history.
		// Returns an empty string on failure.
		std::string ask(std::string prompt);

		// Lower-level, stateless: sends the prompt (plus existing history) and
		// returns the full HTTP Response without modifying history.
		Response raw(std::string prompt);

		// Streams the reply token-by-token as it is generated. onToken is called
		// repeatedly with each chunk of answer text. On a 2xx response the
		// exchange is appended to history (same as ask()). Returns the HTTP
		// status code (200 on success; 0 if the request could not be sent).
		//
		// onReasoning is optional: for reasoning models (e.g. gpt-oss,
		// deepseek-r1) it receives the model's "thinking" chunks as they stream,
		// separate from the answer. Leave it unset to ignore reasoning.
		long stream(std::string prompt,
			std::function<void(const std::string&)> onToken,
			std::function<void(const std::string&)> onReasoning = {});

		// The reasoning/"thinking" text from the most recent ask() or stream()
		// (empty for non-reasoning models). The answer itself is returned by
		// ask() / delivered to onToken.
		const std::string& lastReasoning() const;

		// Clears conversation history (keeps the system instruction, if any).
		void reset();

		// Read-only access to the current conversation.
		const std::vector<Message>& history() const;

	private:
		std::string endpoint() const;
		std::string buildBody(const std::string& prompt, bool streaming);

		Provider provider;
		std::string apiKey;
		std::string model;
		std::string host;
		std::vector<Message> messages;
		double temperature;
		int maxTokens;
		std::string reasoningText;
	};
}
