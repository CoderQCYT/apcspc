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

static void runCode(const std::string& code) {
	CompilerContext ctx;
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

int main(int argc, char* argv[]) {
	// load code from file
	if (argc < 2) {
		std::cerr << "Error: No input file specified." << std::endl;
		std::cerr << "Usage: apcspc <filename>" << std::endl;
		return 1;
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

	return 0;
}
