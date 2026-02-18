#include <string>
#include "Variable.h"

enum class ExecSignal: char {
	None,
	Return,
	Error
};

struct ExecResult {
	ExecSignal signal = ExecSignal::None;
	Variable variable;

	static ExecResult normal();
	static ExecResult ret(const Variable& v);
	static ExecResult err(const Variable& v);
	static ExecResult err(const std::string s);
};