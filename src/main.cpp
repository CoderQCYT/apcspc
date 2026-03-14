#include "expressions.h"
#include "block.h"
#include "util.h"

#include "types/CompilerContext.h"
#include "types/ExecResult.h"

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#ifdef WIN32 // For SetConsoleOutputCP
#include <windows.h>
#endif

CompilerContext ctx;
static void runCode(const std::string& code) {
	std::vector<std::string> lines;

	size_t start = 0;

	for (size_t c = 0; c <= code.size(); ++c) {
		if (c == code.size() || code[c] == '\n' || code[c] == '\r') {
			std::string line = trim(code.substr(start, c - start));
			if (line.starts_with("//")) { // Comments at the beginning of a line.
				start = c + 1;
				continue;
			}
			if (!line.empty())
				lines.push_back(line);

			start = c + 1;
		}
	}

	runBlock(ctx, lines);

	clearExprCache();
}

static void shell() {
	std::cout << "apcspc shell" << std::endl;
	std::cout << "WARNING: The shell is a tiny bit unstable, run a file with apcsp <filename>" << std::endl;
	std::vector<std::string> lines;
	int depth = 0;

#ifdef WIN32
	SetConsoleCP(CP_UTF8);
#endif

	while (true) {	
		std::cout << "> ";
		std::string input;

	reinput:
		if (depth > 0) {
			std::cout << ". ";
		}

		std::getline(std::cin, input);

		std::string trimmed_input = trim(input);
		if (trimmed_input == "exit" || trimmed_input == "EXIT" || trimmed_input == "EXIT()")
			return;

		lines.push_back(trimmed_input);

		depth += getBlockDepth(trimmed_input);
		if (depth > 0) goto reinput;

		try {
			ExecResult result = runBlock(ctx, lines);

			if (result.signal == ExecSignal::Return)
				std::cout << result.variable.toString() << std::endl;
			else if (result.signal == ExecSignal::Error)
				std::cerr << "Error: " << result.variable.toString() << std::endl;
		}
		catch (std::exception e) {
			std::cerr << "Error: " << e.what() << std::endl;
		}

		lines.clear();
		std::cout << std::endl;
	}

}

int main(int argc, char* argv[]) {
	// load code from file
	if (argc < 2) {
		/*
		std::cerr << "Error: No input file specified." << std::endl;
		std::cerr << "Usage: apcspc <filename>" << std::endl;
		return 1;
		*/
		shell();
		return 0;
	}

	std::ifstream file(argv[1]);
	if (!file.is_open()) {
		std::cerr << "Error opening file: " << argv[1] << std::endl;
		return 1;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string code = buffer.str();

	// Handle UTF-8 BOM
	if (code[0] == -17 && code[1] == -69 && code[2] == -65) {
#ifdef WIN32
		SetConsoleOutputCP(CP_UTF8);
#endif
		code = code.substr(3);
	}

	// Error if using UTF-16
	if ((code[0] == -1 && code[1] == -2) || (code[0] == -2 && code[1] == -1)) {
		std::cerr << "Error opening file: " << argv[1] << std::endl;
		std::cerr << "UTF-16 encoding is not supported. Please save the file as UTF-8 or ANSI." << std::endl;
		return 1;
	}

	runCode(code);

	std::cout << std::endl;

	return 0;
}
