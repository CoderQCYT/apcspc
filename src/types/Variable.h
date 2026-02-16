#pragma once
#include <string>
#include <vector>
#include "Procedure.h"

struct CompilerContext;
struct Variable {
	enum Type {
		NONE,
		NUMBER,
		STRING,
		BOOLEAN,
		LIST,
		PROCEDURE
	};

	Type type;
	union {
		double number;
		std::string* string;
		bool boolean;
		std::vector<Variable>* list;
		Procedure* procedure;
	};

	static Variable makeNone(const CompilerContext& ctx);
	static Variable makeNumber(const double n);
	static Variable makeString(const std::string& s);
	static Variable makeBoolean(const bool b);
	static Variable makeList(const std::vector<Variable>& l);
	static Variable makeProcedure(const Procedure& p);

	std::string toString() const;
	bool toBoolean() const;

	static Variable copyFrom(const Variable& other);
};