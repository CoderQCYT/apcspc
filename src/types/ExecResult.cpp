#include "ExecResult.h"

ExecResult ExecResult::normal() { return {}; }
ExecResult ExecResult::ret(const Variable& v) {
	ExecResult r;
	r.signal = ExecSignal::Return;
	r.variable = v;
	return r;
}
ExecResult ExecResult::err(const Variable& v) {
	ExecResult r;
	r.signal = ExecSignal::Error;
	r.variable = v;
	return r;
}
ExecResult ExecResult::err(const std::string s) {
	ExecResult r;
	r.signal = ExecSignal::Error;
	r.variable = Variable::makeString(s);
	return r;
}