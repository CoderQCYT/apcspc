#include "Variable.h"
#include "CompilerContext.h"

#ifdef HAVE_CX20_FORMAT
#include <format>
#else
#include <sstream>
#endif

Variable Variable::makeNone(const CompilerContext& ctx) {
	Variable var{};
	if (!ctx.qcExtensionsEnabled) {
		var.type = NUMBER;
		var.number = 0;
	}
	else var.type = NONE;
	return var;
}

Variable Variable::makeNumber(double n) {
	Variable var{};
	var.type = NUMBER;
	var.number = n;
	return var;
}

Variable Variable::makeString(const std::string& s) {
	Variable var{};
	var.type = STRING;
	var.string = new std::string(s);
	return var;
}

Variable Variable::makeBoolean(bool b) {
	Variable var{};
	var.type = BOOLEAN;
	var.boolean = b;
	return var;
}

Variable Variable::makeList(const std::vector<Variable>& l) {
	Variable var{};
	var.type = LIST;
	var.list = new std::vector<Variable>(l);
	return var;
}

Variable Variable::makeProcedure(const Procedure& p) {
	Variable var{};
	var.type = PROCEDURE;
	var.procedure = new Procedure(p);
	return var;
}



bool Variable::worksAs(Type expected) const {
	if (type == expected) return true;
	switch (expected) {
		case NONE:
			return type == NONE || (type == NUMBER && number == 0) || (type == STRING && string->empty());
		case NUMBER:
			switch (type) {
			case NUMBER:
			case BOOLEAN:
				return true;
			case STRING:
				try {
					std::stod(*string);
					return true;
				}
				catch (...) {
					return false;
				}
			default:
				return false;
			}
		case STRING:
			return type == STRING || type == NUMBER || type == BOOLEAN;
		case BOOLEAN:
			return type == BOOLEAN || type == NUMBER || type == STRING;
		case LIST:
			return type == LIST;
		case PROCEDURE:
			return type == PROCEDURE;
		default:
			return false;
	}
}

double Variable::toNumber() const {
	switch (type) {
		case NUMBER:
			return number;
		case BOOLEAN:
			return boolean ? 1 : 0;
		case STRING:
			try {
				return std::stod(*string);
			} catch (...) {
				std::cerr << "Error: Cannot convert string to number." << std::endl;
				exit(1);
			}
		case NONE:
			return 0;
		default:
			std::cerr << "Error: Cannot convert variable to number." << std::endl;
			exit(1);
	}
}

std::string Variable::toString() const {
	switch (type) {
		case NONE:
			return "null";
		case STRING:
			return *string;
		case NUMBER:
#ifdef HAVE_CX20_FORMAT
			return std::format("{}", number);
#else
			{
				std::ostringstream oss;
				oss << number;
				return oss.str();
			}
#endif
		case BOOLEAN:
			return boolean ? "true" : "false";
		case LIST: {
			std::string out = "[";
			for (size_t i = 0; i < list->size(); ++i) {
				out += (*list)[i].toString();
				if (i < list->size() - 1) out += ", ";
			}
			out += "]";
			return out;
		}
		case PROCEDURE:
			return "<procedure>";
		default:
			return "";
	}
}

bool Variable::toBoolean() const {
	switch (type) {
		case NONE:
		return false;
		case BOOLEAN:
			return boolean;
		case NUMBER:
			return number != 0;
		case STRING:
			return !string->empty();
		case LIST:
			return !list->empty();
		case PROCEDURE:
			return true;
		default:
			return false;
	}
}

Variable Variable::copyFrom(const Variable& other) {
	Variable var{};
	var.type = other.type;

	switch (var.type) {
	case NONE: break;
	case NUMBER:
		var.number = other.number;
		break;

	case BOOLEAN:
		var.boolean = other.boolean;
		break;

	case STRING:
		var.string = new std::string(*other.string);
		break;

	case LIST:
		var.list = new std::vector<Variable>(*other.list);
		break;

	case PROCEDURE:
		var.procedure = new Procedure(*other.procedure);
		break;
	}

	return var;
}
