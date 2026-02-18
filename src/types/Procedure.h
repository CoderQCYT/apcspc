#pragma once
#include <string>
#include <vector>

struct Variable;
struct CompilerContext;
struct ExecResult;

struct Procedure {
	std::vector<std::string> parameters;
	std::vector<std::string> body;
};

ExecResult callProcedure(CompilerContext& ctx, const std::string& name, const std::vector<Variable>& args);