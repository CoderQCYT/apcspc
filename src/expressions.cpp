#include "expressions.h"
#include "util.h"

#include "types/CompilerContext.h"
#include "types/Variable.h"
#include "types/ExecResult.h"

#include <sstream>
#include <tuple>

struct Token {
	enum Type {
		// literals and identifiers
		Identifier,
		Number,
		String,
		List,
		True,
		False,

		// operators
		Add,
		Subtract,
		Multiply,
		Divide,
		Modulo,
		Comma,
		Assignment,

		// logical operators
		And,
		Not,
		Or,

		// comparison operators
		Equal,
		NotEqual,
		Greater,
		Less,
		GreaterEqual,
		LessEqual,

		// wrappers
		LParen,
		RParen,
		LBracket,
		RBracket,
		Function,

		End
	} type;
	std::string value;
};

struct Expr {
	enum Type {
		None,
		Literal,
		Identifier,
		Unary,
		Binary,
		Index,
		Call,
		List
	} type = Expr::None;

	Variable value; // for literals and variables
	std::string op = ""; // for operators
	std::vector<Expr*> children = {};

};

static std::vector<Token> tokens;
static std::unordered_map<std::string, Expr*> cachedExprs;
static size_t pos = 0;

static bool isIdentityStart(char c) {
	if (c == -30) return false;
	if (c <= 0x00) return true;
	return std::isalpha((unsigned char)c) || c == '_';
}
static bool isIdentityPart(char c) {
	if (c == -30) return false;
	if (c <= 0x00) return true;
	return std::isalnum((unsigned char)c) || c == '_';
}

static Token peek() {
	if (pos < tokens.size()) return tokens[pos];
	return Token{ Token::End, "" };
}
static Token advance() {
	Token t = peek();
	if (pos < tokens.size()) ++pos;
	return t;
}
static void expect(Token::Type type) {
	Token t = advance();
	if (t.type != type) return;
}

static std::pair<int, int> bindingPower(const Token& t) {
	switch (t.type) {
	case Token::Assignment:	return { 3, 2 };
	case Token::Or:
	case Token::And:		return { 5, 6 };
	case Token::Equal:
	case Token::NotEqual:
	case Token::Greater:
	case Token::Less:
	case Token::GreaterEqual:
	case Token::LessEqual:	return { 7, 8 };
	case Token::Add:
	case Token::Subtract:	return { 10, 11 };
	case Token::Not:		return { 15, 16 };
	case Token::Multiply:
	case Token::Divide:		
	case Token::Modulo:		return { 20, 21 };
	case Token::LBracket:	return { 100, 121 };
	case Token::LParen:		return { 120, 121 };
	default:				return { -1, -1 };
	}
}

static std::vector<Token> tokenize(const std::string& _expr) {
	std::vector<Token> out;

	std::string expr = trim(_expr);
	for (size_t i = 0; i < expr.size(); ++i) {
		char c = expr[i];

		if (c > 0)
			if (std::isspace(c)) continue;

		// boolean literals
		if (expr.compare(i, 4, "true") == 0 && (i + 4 >= expr.size() || !isIdentityPart(expr[i + 4]))) {
			out.push_back({ Token::True, "true" });
			i += 3;
			continue;
		} else if (expr.compare(i, 5, "false") == 0 && (i + 5 >= expr.size() || !isIdentityPart(expr[i + 5]))) {
			out.push_back({ Token::False, "false" });
			i += 4;
			continue;
		}

		// numbers (integers and decimals)
		if (std::isdigit((unsigned char)c) ||
			(c == '.' && i + 1 < expr.size() && std::isdigit((unsigned char)expr[i + 1]))) {

			std::string number;
			bool sawDot = false;

			while (i < expr.size()) {
				char current = expr[i];

				if (std::isdigit((unsigned char)current)) {
					number += current;
				}
				else if (current == '.' && !sawDot) {
					sawDot = true;
					number += current;
				}
				else {
					break;
				}

				++i;
			}

			--i; // compensate for outer for-loop

			out.push_back({ Token::Number, number });
			continue;
		}


		// strings
		if (c == '"') {
			std::string str;
			++i; // skip opening quote

			while (i < expr.size()) {
				char current = expr[i];

				if (current == '"') {
					break; // end of string
				}

				if (current == '\\') {
					++i;
					if (i >= expr.size()) {
						std::cerr << "Invalid escape sequence.\n";
						exit(1);
					}

					char esc = expr[i];

					switch (esc) {
					case 'n':  str += '\n'; break;
					case 't':  str += '\t'; break;
					case 'r':  str += '\r'; break;
					case '\\': str += '\\'; break;
					case '"':  str += '"';  break;

					case 'x': {  // \xXX
						if (i + 2 >= expr.size()) {
							std::cerr << "Invalid \\x escape.\n";
							exit(1);
						}

						std::string hex = expr.substr(i + 1, 2);

						unsigned int value;
						std::stringstream ss;
						ss << std::hex << hex;
						ss >> value;

						str += static_cast<char>(value);

						i += 2;
						break;
					}

					default:
						std::cerr << "Unknown escape sequence: \\" << esc << "\n";
						exit(1);
					}
				}
				else {
					str += current;
				}

				++i;
			}

			if (i >= expr.size() || expr[i] != '"') {
				std::cerr << "Unterminated string literal.\n";
				exit(1);
			}

			out.push_back({ Token::String, str });
			continue;
		}

		
		// slightly longer UTF-8 operators, e.g. equality (≠, ≥, ≤)
		if (c == (char)-30 and expr[i+1] == (char)-119 and expr[i+2] == (char)-96) {
			out.push_back({ Token::NotEqual, "!=" });
			i += 2;
			continue;
		}
		else if (c == (char)-30 and expr[i+1] == (char)-119 and expr[i+2] == (char)-91) {
			out.push_back({ Token::GreaterEqual, ">=" });
			i += 2;
			continue;
		}
		else if (c == (char)-30 and expr[i+1] == (char)-119 and expr[i+2] == (char)-92) {
			out.push_back({ Token::LessEqual, "<=" });
			i += 2;
			continue;
		}
		else if (c == (char)-30 and expr[i+1] == (char)-122 and expr[i+2] == (char)-112) {
			out.push_back({ Token::Assignment, "←" });
			i += 2;
			continue;
		// now handle normal ASCII operators because I'm not an asshole
		}
		else if (c == '!' and expr[i+1] == '=') {
			out.push_back({ Token::NotEqual, "!=" });
			i += 1;
			continue;
		} 
		else if (c == '>' and expr[i+1] == '=') {
			out.push_back({ Token::GreaterEqual, ">=" });
			i += 1;
			continue;
		}
		else if (c == '<' and expr[i+1] == '=') {
			out.push_back({ Token::LessEqual, "<=" });
			i += 1;
			continue;
		}
		else if (c == '<' and expr[i + 1] == '-') {
			out.push_back({ Token::Assignment, "←" });
			i += 1;
			continue;
		} else if (c == '/' and expr[i + 1] == '/') { // Handle comments in the expression stage.
			break;
		}

		// identifiers and longer operators
		if (isIdentityStart(c)) {
			std::string ident;
			while (i < expr.size() && isIdentityPart(expr[i])) {
				ident += expr[i];
				++i;
			}
			--i;

			if (ident == "MOD") out.push_back({ Token::Modulo, "MOD" });
			else if (ident == "AND") out.push_back({ Token::And, "AND" });
			else if (ident == "OR") out.push_back({ Token::Or, "OR" });
			else if (ident == "NOT") out.push_back({ Token::Not, "NOT" });
			else out.push_back({ Token::Identifier, ident });
			continue;
		}


		// basic operators
		switch (c) {
			case '+': out.push_back({ Token::Add,      "+" });  break;
			case '-': out.push_back({ Token::Subtract, "-" });  break;
			case '*': out.push_back({ Token::Multiply, "*" });  break;
			case '/': out.push_back({ Token::Divide,   "/" });  break;
			case '(': out.push_back({ Token::LParen,   "(" });  break;
			case ')': out.push_back({ Token::RParen,   ")" });  break;
			case '[': out.push_back({ Token::LBracket, "[" });  break;
			case ']': out.push_back({ Token::RBracket, "]" });  break;
			case ',': out.push_back({ Token::Comma,    "," });  break;
			case '=': out.push_back({ Token::Equal,    "=" });  break;
			case '>': out.push_back({ Token::Greater,  ">" });  break;
			case '<': out.push_back({ Token::Less,     "<" });  break;
		}
	}

	return out;
}

static Expr* parseExpr(int min_bp, CompilerContext& ctx);
static Variable evalExpr(Expr* expr, CompilerContext& ctx);


static Expr* nud(const Token& token, CompilerContext& ctx) {
	switch (token.type) {
		case Token::True: {
			Expr* expr = new Expr;
			expr->type = Expr::Literal;
			expr->value = Variable::makeBoolean(true);
			return expr;
		}
		case Token::False: {
			Expr* expr = new Expr;
			expr->type = Expr::Literal;
			expr->value = Variable::makeBoolean(false);
			return expr;
		}
		case Token::Number: {
			Expr* expr = new Expr;
			expr->type = Expr::Literal;
			expr->value = Variable::makeNumber(std::stod(token.value));
			return expr;
		}
		case Token::String: {
			Expr* expr = new Expr;
			expr->type = Expr::Literal;
			expr->value.type = Variable::STRING;
			expr->value = Variable::makeString(token.value);
			return expr;
		}
		case Token::Identifier: {
			if (peek().type == Token::LParen) {
				advance(); // '('

				Expr* call = new Expr;
				call->type = Expr::Call;
				call->op = token.value;

				if (peek().type != Token::RParen) {
					while (true) {
						call->children.emplace_back(parseExpr(0, ctx));
						if (peek().type == Token::Comma) {
							advance();
							continue;
						}
						break;
					}
				}

				expect(Token::RParen);
				return call;
			}
			Expr* expr = new Expr;
			expr->type = Expr::Identifier;
			expr->value.type = Variable::STRING;
			expr->value = Variable::makeString(token.value);
			return expr;
		}
		case Token::Assignment: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "←";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(0, ctx));
			return expr;
		}
		case Token::Add: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "+";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(10, ctx));
			return expr;
		}
		case Token::Subtract: {
			Expr* expr = new Expr;
			expr->type = Expr::Unary;
			expr->op = "-";
			expr->children.push_back(parseExpr(15, ctx));
			return expr;
		}
		case Token::Multiply: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "*";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(20, ctx));
			return expr;
		}
		case Token::Divide: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "/";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(20, ctx));
			return expr;
		}
		case Token::Modulo: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "MOD";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(20, ctx));
			return expr;
		}
		case Token::And: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "AND";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(5, ctx));
			return expr;
		}
		case Token::Or: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "OR";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(5, ctx));
			return expr;
		}
		case Token::Not: {
			Expr* expr = new Expr;
			expr->type = Expr::Unary;
			expr->op = "NOT";
			expr->children.emplace_back(parseExpr(15, ctx));
			return expr;
		}
		case Token::Equal: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "=";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(7, ctx));
			return expr;
		}
		case Token::NotEqual: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "!=";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(7, ctx));
			return expr;
		}
		case Token::Greater: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = ">";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(7, ctx));
			return expr;
		}
		case Token::Less: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "<";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(7, ctx));
			return expr;
		}
		case Token::GreaterEqual: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = ">=";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(7, ctx));
			return expr;
		}
		case Token::LessEqual: {
			Expr* expr = new Expr;
			expr->type = Expr::Binary;
			expr->op = "<=";
			expr->children.emplace_back(new Expr);
			expr->children.emplace_back(parseExpr(7, ctx));
			return expr;
		}
		case Token::LParen: {
			Expr* expr = parseExpr(0, ctx);
			expect(Token::RParen);
			return expr;
		}
		case Token::LBracket: {
			Expr* expr = new Expr;
			expr->type = Expr::List;

			while (peek().type != Token::RBracket) {
				expr->children.push_back(parseExpr(0, ctx));
				if (peek().type == Token::Comma)
					advance();
			}

			expect(Token::RBracket);
			return expr;
		}
		default:
			std::cerr << "Unexpected token: " << token.value << std::endl;	
			exit(1);
	}
}

static Expr* parseExpr(int min_bp, CompilerContext& ctx) {
	const Token& t = advance();
	Expr* left = nud(t, ctx);

	while (true) {
		const Token& op = peek();

		int lbp, rbp;
		std::tie(lbp, rbp) = bindingPower(op);

		if (lbp < min_bp)
			break;

		advance();

		Expr* right = parseExpr(rbp, ctx);

		Expr* expr = new Expr;
		if (op.type == Token::LBracket) {
			expr->type = Expr::Index;
			expr->children.emplace_back(left);
			expr->children.emplace_back(right);
			expect(Token::RBracket);
			left = expr;
			continue;
		}

		expr->type = Expr::Binary;
		expr->op = op.value;
		expr->children.emplace_back(left);
		expr->children.emplace_back(right);

		left = expr;
	}

	return left;
}

static Variable evalExpr(Expr* expr, CompilerContext& ctx) {
	switch (expr->type) {
		case Expr::Literal: return expr->value;
		case Expr::Identifier:
			if (ctx.variables.find(*expr->value.string) != ctx.variables.end()) {
				return ctx.variables[*expr->value.string];
			} else {
				std::cerr << "Undefined variable: " << *expr->value.string << std::endl;
				exit(1);
			}
		case Expr::Call: {
			std::vector<Variable> args = {};
			for (Expr* child : expr->children) {
				args.push_back(evalExpr(child, ctx));
			}
			ExecResult result = callProcedure(ctx, expr->op, args);
			if (result.signal == ExecSignal::Error) {
				std::cerr << "Error calling procedure " << expr->op << ": " << result.variable.toString() << std::endl;
				exit(1);
			}
			if (result.signal == ExecSignal::Return) {
				return result.variable;
			}
			return Variable::makeNone(ctx);
		}
		case Expr::Binary: {
			Expr* leftExpr = expr->children[0];
			Expr* rightExpr = expr->children[1];

			Variable rightVal = evalExpr(rightExpr, ctx);

			if (expr->op == "←") {
				if (leftExpr->type == Expr::Index) {
					Expr* baseExpr = leftExpr->children[0];
					Variable& leftVal = ctx.variables[*baseExpr->value.string];
					Variable indexVal = evalExpr(leftExpr->children[1], ctx);

					if (leftExpr->type == Expr::Index) {

						Expr* baseExpr = leftExpr->children[0];
						Expr* indexExpr = leftExpr->children[1];

						if (baseExpr->type != Expr::Identifier) {
							std::cerr << "Indexed assignment requires a variable." << std::endl;
							exit(1);
						}

						Variable& leftVal = ctx.variables[*baseExpr->value.string];
						Variable indexVal = evalExpr(indexExpr, ctx);

						if (leftVal.type != Variable::LIST ||
							indexVal.type != Variable::NUMBER) {
							std::cerr << "Invalid index assignment." << std::endl;
							exit(1);
						}

						int index = (int)indexVal.number;
						if (index < 1 || index > leftVal.list->size()) {
							std::cerr << "List index out of bounds." << std::endl;
							exit(1);
						}

						(*leftVal.list)[index - 1] = rightVal;
						return rightVal;
					} else if (leftVal.type == Variable::STRING && indexVal.type == Variable::NUMBER) {
						int index = (int)indexVal.number;
						if (index < 1 || index > leftVal.string->size()) {
							std::cerr << "String index out of bounds." << std::endl;
							exit(1);
						}
						std::string newStr = *leftVal.string;
						newStr[index - 1] = rightVal.toString()[0];
						leftVal.string->assign(newStr);
						return rightVal;
					} else {
						std::cerr << "Index assignment requires a list or string on the left and a number on the right." << std::endl;
						exit(1);
					}
					
				}
				else if (leftExpr->type != Expr::Identifier) {
					std::cerr << "Left-hand side of assignment must be a variable." << std::endl;
					exit(1);
				}
				ctx.variables[*leftExpr->value.string] = Variable::copyFrom(rightVal);
				return rightVal;
			}

			Variable leftVal = evalExpr(leftExpr, ctx);

			if (leftVal.type == Variable::NUMBER && rightVal.type == Variable::NUMBER) {
				if (expr->op == "+") return Variable::makeNumber(leftVal.number + rightVal.number);
				if (expr->op == "-") return Variable::makeNumber(leftVal.number - rightVal.number);
				if (expr->op == "*") return Variable::makeNumber(leftVal.number * rightVal.number);
				if (expr->op == "/") return Variable::makeNumber(leftVal.number / rightVal.number);
				if (expr->op == "MOD") return Variable::makeNumber((const int)leftVal.number % (const int)rightVal.number);
				if (expr->op == ">") return Variable::makeBoolean(leftVal.number > rightVal.number);
				if (expr->op == "<") return Variable::makeBoolean(leftVal.number < rightVal.number);
				if (expr->op == ">=") return Variable::makeBoolean(leftVal.number >= rightVal.number);
				if (expr->op == "<=") return Variable::makeBoolean(leftVal.number <= rightVal.number);
			}

			if (expr->op == "AND") return Variable::makeBoolean(leftVal.toBoolean() && rightVal.toBoolean());
			if (expr->op == "OR") return Variable::makeBoolean(leftVal.toBoolean() || rightVal.toBoolean());
			if (expr->op == "=") return Variable::makeBoolean(leftVal.toString() == rightVal.toString());
			if (expr->op == "!=") return Variable::makeBoolean(leftVal.toString() != rightVal.toString());

			// detecting indexing like this actually fucking works, and I'm not sure why
			if (expr->op == "[") {
				if (leftVal.type == Variable::LIST && rightVal.type == Variable::NUMBER) {
					int index = (int)rightVal.number;
					if (index < 0 || index >= leftVal.list->size()) {
						std::cerr << "List index out of bounds." << std::endl;
						exit(1);
					}
					return (*leftVal.list)[index];
				} else if (leftVal.type == Variable::STRING && rightVal.type == Variable::NUMBER) {
					int index = (int)rightVal.number;
					if (index < 0 || index >= leftVal.string->size()) {
						std::cerr << "String index out of bounds." << std::endl;
						exit(1);
					}
					return Variable::makeString(std::string(1, (*leftVal.string)[index]));
				} else {
					std::cerr << "Indexing operator requires a list or string on the left and a number on the right." << std::endl;
					exit(1);
				}
			}
			
			std::cerr << "Error in binary expression." << std::endl;
			exit(1);
		}
		case Expr::Unary: {
			Expr* child = expr->children[0];
			Variable val = evalExpr(child, ctx);
			if (expr->op == "NOT") return Variable::makeBoolean(!val.toBoolean());

			if (expr->op == "-") {
				if (val.type != Variable::NUMBER) {
					std::cerr << "'-' requires a number." << std::endl;
					exit(1);
				}
				return Variable::makeNumber(-val.number);
			}

			std::cerr << "Error in unary expression." << std::endl;
			exit(1);
		}
		case Expr::Index: {
			Variable leftVal = evalExpr(expr->children[0], ctx);
			Variable rightVal = evalExpr(expr->children[1], ctx);

			if (rightVal.type != Variable::NUMBER) {
				std::cerr << "Index must be a number." << std::endl;
				exit(1);
			}

			int index = (int)rightVal.number;
			if (leftVal.type == Variable::LIST) {
				int size = (int)leftVal.list->size();
				if (index < 1 || index > size) {
					std::cerr << "List index out of bounds." << std::endl;
					exit(1);
				}
				return (*leftVal.list)[index - 1];
			}
			else if (leftVal.type == Variable::STRING) {
				int size = (int)leftVal.string->size();
				if (index < 1 || index > size) {
					std::cerr << "String index out of bounds." << std::endl;
					exit(1);
				}

				return Variable::makeString(
					std::string(1, (*leftVal.string)[index - 1])
				);
			} else {
				std::cerr << "Indexing requires a list or string." << std::endl;
				exit(1);
			}
		}
		case Expr::List: {
			std::vector<Variable> values;

			for (Expr* child : expr->children) {
				values.push_back(evalExpr(child, ctx));
			}

			return Variable::makeList(values);
		}
		default:
			std::cerr << "Unknown expression type." << std::endl;
			exit(1);
		}
}


static void deleteAST(Expr* expr) {
	for (Expr* child : expr->children)
		deleteAST(child);
	delete expr;
}


Variable evaluateExpr(CompilerContext& ctx, const std::string& expr) {
	// std::cout << "Evaluating Expr: " << expr << std::endl;
	if (cachedExprs.find(expr) != cachedExprs.end()) {
		// std::cout << "Using cached expression.\n";
		return evalExpr(cachedExprs[expr], ctx);
	}

	tokens = tokenize(expr);
	pos = 0;

	Expr* root = parseExpr(0, ctx);

	cachedExprs[expr] = root;

	Variable result = evalExpr(root, ctx);
	return result;
}

void clearExprCache() {
	for (auto& pair : cachedExprs) {
		deleteAST(pair.second);
	}
	cachedExprs.clear();
}