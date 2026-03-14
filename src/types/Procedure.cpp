#include "Procedure.h"
#include "CompilerContext.h"
#include "Variable.h"
#include "ExecResult.h"

#include "../expressions.h"
#include "../block.h"

#include <iostream>
#include <random>

std::random_device rd;

ExecResult callProcedure(CompilerContext& ctx, const std::string& name, const std::vector<Variable>& args) {
	if (name == "DISPLAY") {		// DISPLAY(value)
		if (ctx.qcExtensionsEnabled) { // All good programming languages create a new line when they're done.
			for (const Variable& arg : args) {
				std::cout << arg.toString() << " ";
			}
			std::cout << std::endl;
		}
		else {
			std::cout << args[0].toString() << " ";
		}
		return ExecResult::normal();
	}
	else if (name == "EMIT") {		// EMIT(value) [QC EXTENSIONS ONLY]
		if (!ctx.qcExtensionsEnabled)
			return ExecResult::err("EMIT is only available when QC extensions are enabled.");
		for (const Variable& arg : args) {
			std::cout << arg.toString();
		}
	} 
	else if (name == "INPUT") {	// INPUT([prompt])
		std::string input;
		std::getline(std::cin, input);
		return ExecResult::ret(Variable::makeString(input));
	} 
	else if (name == "RANDOM") {	// RANDOM(a, b)
		if (args.size() != 2 || args[0].type != Variable::NUMBER || args[1].type != Variable::NUMBER) {
			return ExecResult::err("RANDOM expects two number arguments.");
		}
		double a = args[0].number;
		double b = args[1].number;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis((int)a, (int)b);
		return ExecResult::ret(Variable::makeNumber(dis(gen)));
	}
	else if (name == "INSERT") {	// INSERT(aList, i, value)
		if (args.size() != 3 || args[0].type != Variable::LIST || args[1].type != Variable::NUMBER) {
			return ExecResult::err("INSERT expects a list, a number index, and a value.");
		}
		int index = (int)args[1].number;
		if (index < 1 || index > args[0].list->size() + 1) {
			return ExecResult::err("INSERT index out of bounds.");
		}
		args[0].list->insert(args[0].list->begin() + index - 1, args[2]);
		return ExecResult::normal();
	} 
	else if (name == "APPEND") {	// APPEND(aList, value)
		if (args.size() != 2 || args[0].type != Variable::LIST) {
			return ExecResult::err("APPEND expects a list and a value.");
		}
		args[0].list->emplace_back(args[1]);
		return ExecResult::normal();
	} 
	else if (name == "REMOVE") {	// REMOVE(aList, i)
		if (args.size() != 2 || args[0].type != Variable::LIST || args[1].type != Variable::NUMBER) {
			return ExecResult::err("REMOVE expects a list and a number index.");
		}
		int index = (int)args[1].number;
		if (index < 1 || index > args[0].list->size()) {
			return ExecResult::err("REMOVE index out of bounds.");
		}
		args[0].list->erase(args[0].list->begin() + index - 1);
		return ExecResult::normal();
	} 
	else if (name == "LENGTH") {	// LENGTH(aList)
		if (ctx.qcExtensionsEnabled) { // When QC extensions are enabled, LENGTH can also be used on strings.
			if (args.size() != 1)
				return ExecResult::err("LENGTH expects a list or a string.");
			else if (args[0].type == Variable::STRING)
				return ExecResult::ret(Variable::makeNumber(args[0].string->size()));
		}
		else {
			if (args.size() != 1)
				return ExecResult::err("LENGTH expects a list.");
			else if (args[0].type == Variable::STRING)
				return ExecResult::err("LENGTH expects a list.\nUse ENABLE_QC_EXTENSIONS to use LENGTH on strings.");
			else if (args[0].type != Variable::LIST)
				return ExecResult::err("LENGTH expects a list.");
		}
		return ExecResult::ret(Variable::makeNumber(args[0].list->size()));
	} 
	else if (name == "CONCAT") { // CONCAT(aString, bString)
		if (args.size() != 2 || args[0].type != Variable::STRING || args[1].type != Variable::STRING)
			return ExecResult::err("CONCAT expects two string arguments.");

		return ExecResult::ret(Variable::makeString(*args[0].string + *args[1].string));
	}
	else if (name == "SUBSTRING") { // SUBSTRING(text, start, end)
		if (args.size() != 3 || args[0].type != Variable::STRING || args[1].type != Variable::NUMBER || args[2].type != Variable::NUMBER)
			return ExecResult::err("SUBSTRING expects a string and two number arguments.");

		const std::string& text = *args[0].string;
		int start = (int)args[1].number;
		int end = (int)args[2].number;
		if (start < 1 || end > text.size() + 1 || start > end)
			return ExecResult::err("SUBSTRING indices out of bounds.");
		return ExecResult::ret(Variable::makeString(text.substr(start - 1, end - start)));
	}

	else { // User-defined procedure
		Variable& var = ctx.resolveVariable(name);
		if (var.type == Variable::PROCEDURE) {
			const Procedure& proc = *var.procedure;

			std::vector<Variable> userArgs(
				args.begin(),
				args.begin() + std::min(args.size(), proc.parameters.size())
			);
			userArgs.reserve(proc.parameters.size());

			CompilerContext localCtx;
			localCtx.parent = &ctx;
			localCtx.qcExtensionsEnabled = ctx.qcExtensionsEnabled;
			for (size_t i = 0; i < userArgs.size(); ++i) {
				localCtx.variables[proc.parameters[i]] = userArgs[i];
			}
			return runBlock(localCtx, proc.body);
		} else {
			return ExecResult::err("Undefined procedure");
		}
	}

	return ExecResult::normal();
}