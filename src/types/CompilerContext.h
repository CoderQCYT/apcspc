#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include "Variable.h"	

struct CompilerContext {
	std::unordered_map<std::string, Variable> variables;
	bool qcExtensionsEnabled = false;
};