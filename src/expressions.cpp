#include "expressions.h"
#include "util.h"

#include "types/CompilerContext.h"
#include "types/Variable.h"
#include "types/ExecResult.h"

#include <sstream>
#include <tuple>

enum class TokenType: char {
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
};

struct Token {
	TokenType type;
	std::string value;
};

enum class ExprType: char {
	None,
	Literal,
	Identifier,
	Unary,
	Binary,
	Index,
	Call,
	List
};

struct Expr {
	ExprType type = ExprType::None;
	TokenType op; // for operators

	Variable value; // for literals
	std::string variable; // for identifiers
	std::vector<Expr*> children = {};
};

static std::vector<Token> tokens;
static std::unordered_map<std::string, Token> cachedTokens;
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

static const Token peek() {
	if (pos < tokens.size()) return tokens[pos];
	return Token{ TokenType::End, "" };
}
static const Token advance() {
	const Token& t = peek();
	if (pos < tokens.size()) ++pos;
	return t;
}
static void expect(TokenType type) {
	Token t = advance();
	if (t.type != type) return;
}

static std::pair<int, int> bindingPower(const Token& t) {
	switch (t.type) {
	case TokenType::Assignment:	return { 3, 2 };
	case TokenType::Or:
	case TokenType::And:		return { 5, 6 };
	case TokenType::Equal:
	case TokenType::NotEqual:
	case TokenType::Greater:
	case TokenType::Less:
	case TokenType::GreaterEqual:
	case TokenType::LessEqual:	return { 7, 8 };
	case TokenType::Add:
	case TokenType::Subtract:	return { 10, 11 };
	case TokenType::Not:		return { 15, 16 };
	case TokenType::Multiply:
	case TokenType::Divide:		
	case TokenType::Modulo:		return { 20, 21 };
	case TokenType::LBracket:	return { 100, 121 };
	case TokenType::LParen:		return { 120, 121 };
	default:					return { -1, -1 };
	}
}

static std::vector<Token> tokenize(const std::string& _expr) {
	if (cachedTokens.find(_expr) != cachedTokens.end())
		return { cachedTokens[_expr] };

	std::vector<Token> out;

	std::string expr = trim(_expr);
	for (size_t i = 0; i < expr.size(); ++i) {
		const char c = expr[i];

		if (c > 0)
			if (std::isspace(c)) continue;

		// boolean literals
		if (expr.compare(i, 4, "true") == 0 && (i + 4 >= expr.size() || !isIdentityPart(expr[i + 4]))) {
			out.emplace_back(TokenType::True);
			i += 3;
			continue;
		} else if (expr.compare(i, 5, "false") == 0 && (i + 5 >= expr.size() || !isIdentityPart(expr[i + 5]))) {
			out.emplace_back(TokenType::False);
			i += 4;
			continue;
		}

		// numbers (integers and decimals)
		if (std::isdigit((unsigned char)c) ||
			(c == '.' && i + 1 < expr.size() && std::isdigit((unsigned char)expr[i + 1]))) {

			std::string number;
			number.reserve(16);
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

			out.emplace_back(TokenType::Number, number );
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
						throw std::runtime_error("Invalid escape sequence.");
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
							throw std::runtime_error("Invalid \\x escape.");
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
						throw std::runtime_error("Unknown escape sequence: \\" + esc);
					}
				}
				else {
					str += current;
				}

				++i;
			}

			if (i >= expr.size() || expr[i] != '"') {
				throw std::runtime_error("Unterminated string literal.");
			}

			out.emplace_back(TokenType::String, str);
			continue;
		}

		
		// slightly longer UTF-8 operators, e.g. equality (≠, ≥, ≤)
		if (c == (char)-30 and expr[i+1] == (char)-119 and expr[i+2] == (char)-96) {
			out.emplace_back(TokenType::NotEqual);
			i += 2;
			continue;
		}
		else if (c == (char)-30 and expr[i+1] == (char)-119 and expr[i+2] == (char)-91) {
			out.emplace_back(TokenType::GreaterEqual);
			i += 2;
			continue;
		}
		else if (c == (char)-30 and expr[i+1] == (char)-119 and expr[i+2] == (char)-92) {
			out.emplace_back(TokenType::LessEqual);
			i += 2;
			continue;
		}
		else if (c == (char)-30 and expr[i+1] == (char)-122 and expr[i+2] == (char)-112) {
			out.emplace_back(TokenType::Assignment);
			i += 2;
			continue;
		// now handle normal ASCII operators because I'm not an asshole
		}
		else if (c == '!' and expr[i+1] == '=') {
			out.emplace_back(TokenType::NotEqual);
			i += 1;
			continue;
		} 
		else if (c == '>' and expr[i+1] == '=') {
			out.emplace_back(TokenType::GreaterEqual);
			i += 1;
			continue;
		}
		else if (c == '<' and expr[i+1] == '=') {
			out.emplace_back(TokenType::LessEqual);
			i += 1;
			continue;
		}
		else if (c == '<' and expr[i + 1] == '-') {
			out.emplace_back(TokenType::Assignment);
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

			if (ident == "MOD")		 out.emplace_back(TokenType::Modulo);
			else if (ident == "AND") out.emplace_back(TokenType::And);
			else if (ident == "OR")  out.emplace_back(TokenType::Or);
			else if (ident == "NOT") out.emplace_back(TokenType::Not);
			else					 out.emplace_back(TokenType::Identifier, ident);
			continue;
		}


		// basic operators
		switch (c) {
			case '+': out.emplace_back(TokenType::Add);		 break;
			case '-': out.emplace_back(TokenType::Subtract); break;
			case '*': out.emplace_back(TokenType::Multiply); break;
			case '/': out.emplace_back(TokenType::Divide);   break;
			case '(': out.emplace_back(TokenType::LParen);   break;
			case ')': out.emplace_back(TokenType::RParen);   break;
			case '[': out.emplace_back(TokenType::LBracket); break;
			case ']': out.emplace_back(TokenType::RBracket); break;
			case ',': out.emplace_back(TokenType::Comma);	 break;
			case '=': out.emplace_back(TokenType::Equal);	 break;
			case '>': out.emplace_back(TokenType::Greater);  break;
			case '<': out.emplace_back(TokenType::Less);	 break;
		}
	}

	return out;
}

static Expr* parseExpr(int min_bp, CompilerContext& ctx);
static Variable evalExpr(Expr* expr, CompilerContext& ctx);


static Expr* nud(const Token& token, CompilerContext& ctx) {
	switch (token.type) {
		case TokenType::True: {
			Expr* expr = new Expr;
			expr->type = ExprType::Literal;
			expr->value = Variable::makeBoolean(true);
			return expr;
		}
		case TokenType::False: {
			Expr* expr = new Expr;
			expr->type = ExprType::Literal;
			expr->value = Variable::makeBoolean(false);
			return expr;
		}
		case TokenType::Number: {
			Expr* expr = new Expr;
			expr->type = ExprType::Literal;
			expr->value = Variable::makeNumber(std::stod(token.value));
			return expr;
		}
		case TokenType::String: {
			Expr* expr = new Expr;
			expr->type = ExprType::Literal;
			expr->value.type = Variable::STRING;
			expr->value = Variable::makeString(token.value);
			return expr;
		}
		case TokenType::Identifier: {
			if (peek().type == TokenType::LParen) {
				advance(); // '('

				Expr* call = new Expr;
				call->type = ExprType::Call;
				call->variable = token.value;

				if (peek().type != TokenType::RParen) {
					while (true) {
						call->children.emplace_back(parseExpr(0, ctx));
						if (peek().type == TokenType::Comma) {
							advance();
							continue;
						}
						break;
					}
				}

				expect(TokenType::RParen);
				return call;
			}
			Expr* expr = new Expr;
			expr->type = ExprType::Identifier;
			expr->variable = token.value;
			return expr;
		}
		case TokenType::Subtract:
		case TokenType::Not: {
			Expr* expr = new Expr;
			expr->type = ExprType::Unary;
			expr->op = token.type;
			expr->children.emplace_back(parseExpr(15, ctx));
			return expr;
		}
		case TokenType::LParen: {
			Expr* expr = parseExpr(0, ctx);
			expect(TokenType::RParen);
			return expr;
		}
		case TokenType::LBracket: {
			Expr* expr = new Expr;
			expr->type = ExprType::List;

			while (peek().type != TokenType::RBracket) {
				expr->children.emplace_back(parseExpr(0, ctx));
				if (peek().type == TokenType::Comma)
					advance();
			}

			expect(TokenType::RBracket);
			return expr;
		}
		default:
			throw std::runtime_error("Unexpected token.");
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

		advance();  // consume operator

		// Function call
		if (op.type == TokenType::LParen) {
			Expr* call = new Expr;
			call->type = ExprType::Call;
			call->children.emplace_back(left);

			if (peek().type != TokenType::RParen) {
				while (true) {
					call->children.emplace_back(parseExpr(0, ctx));
					if (peek().type == TokenType::Comma)
						advance();
					else
						break;
				}
			}

			expect(TokenType::RParen);
			left = call;
			continue;
		}

		// Indexing
		if (op.type == TokenType::LBracket) {
			Expr* index = new Expr;
			index->type = ExprType::Index;
			index->children.emplace_back(left);
			index->children.emplace_back(parseExpr(0, ctx));
			expect(TokenType::RBracket);
			left = index;
			continue;
		}

		// Standard binary operator
		Expr* right = parseExpr(rbp, ctx);

		Expr* expr = new Expr;
		expr->type = ExprType::Binary;
		expr->op = op.type;
		expr->children.emplace_back(left);
		expr->children.emplace_back(right);

		left = expr;
	}

	return left;
}


static Variable evalExpr(Expr* expr, CompilerContext& ctx) {
	switch (expr->type) {
		case ExprType::Literal: return expr->value;
		case ExprType::Identifier:
			return ctx.resolveVariable(expr->variable);
		case ExprType::Call: {
			std::vector<Variable> args;
			args.reserve(expr->children.size());
			for (Expr* child : expr->children) {
				args.emplace_back(evalExpr(child, ctx));
			}
			ExecResult result = callProcedure(ctx, expr->variable, args);
			if (result.signal == ExecSignal::Error) {
				throw std::runtime_error("Error calling procedure " + expr->variable + ": " + result.variable.toString());
			}
			if (result.signal == ExecSignal::Return) {
				return result.variable;
			}
			return Variable::makeNone(ctx);
		}
		case ExprType::Binary: {
			Expr* leftExpr = expr->children[0];
			Expr* rightExpr = expr->children[1];

			if (expr->op == TokenType::Assignment) {

				// Indexed assignment
				if (leftExpr->type == ExprType::Index) {

					Expr* baseExpr = leftExpr->children[0];
					Expr* indexExpr = leftExpr->children[1];

					if (baseExpr->type != ExprType::Identifier) {
						throw std::runtime_error("Indexed assignment requires a variable.");
					}

					Variable& baseVal = ctx.resolveVariable(baseExpr->variable);
					Variable  indexVal = evalExpr(indexExpr, ctx);

					if (indexVal.type != Variable::NUMBER) {
						throw std::runtime_error("Index must be a number.");
					}

					int index = static_cast<int>(indexVal.number);

					// List assignment
					if (baseVal.type == Variable::LIST) {

						if (index < 1 || index > baseVal.list->size()) {
							throw std::runtime_error("List index out of bounds.");
						}

						Variable rightVal = evalExpr(rightExpr, ctx);
						(*baseVal.list)[index - 1] = rightVal;
						return rightVal;
					}

					// String assignment
					if (baseVal.type == Variable::STRING) { // Technically not part of the reference sheet, but this is needed.

						if (index < 1 || index > baseVal.string->size()) {
							throw std::runtime_error("String index out of bounds.");
						}

						Variable rightVal = evalExpr(rightExpr, ctx);

						std::string newStr = *baseVal.string;
						newStr[index - 1] = rightVal.toString()[0];
						baseVal.string->assign(newStr);

						return rightVal;
					}

					throw std::runtime_error("Index assignment requires a list or string.");
				}

				// Normal variable assignment
				if (leftExpr->type != ExprType::Identifier) {
					throw std::runtime_error("Left-hand side of assignment must be a variable.");
				}

				Variable rightVal = evalExpr(rightExpr, ctx);
				ctx.variables[leftExpr->variable] = Variable::copyFrom(rightVal);
				return rightVal;
			}

			Variable leftVal = evalExpr(leftExpr, ctx);
			Variable rightVal = evalExpr(rightExpr, ctx);

			if (leftVal.worksAs(Variable::NUMBER) && rightVal.worksAs(Variable::NUMBER)) {
				const double leftNum = leftVal.toNumber();
				const double rightNum = rightVal.toNumber();
				switch (expr->op) {
					case TokenType::Add:
						return Variable::makeNumber(leftNum + rightNum);
					case TokenType::Subtract:
						return Variable::makeNumber(leftNum - rightNum);
					case TokenType::Multiply:
						return Variable::makeNumber(leftNum * rightNum);
					case TokenType::Divide:
						return Variable::makeNumber(leftNum / rightNum);
					case TokenType::Modulo:
						return Variable::makeNumber((const int)leftNum % (const int)rightNum);
					case TokenType::Greater:
						return Variable::makeBoolean(leftNum > rightNum);
					case TokenType::Less:
						return Variable::makeBoolean(leftNum < rightNum);
					case TokenType::GreaterEqual:
						return Variable::makeBoolean(leftNum >= rightNum);
					case TokenType::LessEqual:
						return Variable::makeBoolean(leftNum <= rightNum);
				}
			}

			switch (expr->op) {
				case TokenType::And:
					return Variable::makeBoolean(leftVal.toBoolean() && rightVal.toBoolean());
				case TokenType::Or:
					return Variable::makeBoolean(leftVal.toBoolean() || rightVal.toBoolean());
				case TokenType::Equal:
					return Variable::makeBoolean(leftVal.toString() == rightVal.toString());
				case TokenType::NotEqual:
					return Variable::makeBoolean(leftVal.toString() != rightVal.toString());
			}
			
			throw std::runtime_error("Error in binary expression.");
		}
		case ExprType::Unary: {
			Expr* child = expr->children[0];
			Variable val = evalExpr(child, ctx);

			switch (expr->op) {
				case TokenType::Not:
					return Variable::makeBoolean(!val.toBoolean());
				case TokenType::Subtract:
					if (val.type != Variable::NUMBER) {
						throw std::runtime_error("'-' requires a number.");
					}
					return Variable::makeNumber(-val.number);
				default:
					throw std::runtime_error("Error in unary expression.");
			}
		}
		case ExprType::Index: {
			Variable leftVal = evalExpr(expr->children[0], ctx);
			Variable rightVal = evalExpr(expr->children[1], ctx);

			if (rightVal.type != Variable::NUMBER) {
				throw std::runtime_error("Index must be a number.");
			}

			int index = (int)rightVal.number;
			if (leftVal.type == Variable::LIST) {
				int size = (int)leftVal.list->size();
				if (index < 1 || index > size) {
					throw std::runtime_error("List index out of bounds.");
				}
				return (*leftVal.list)[index - 1];
			}
			else if (leftVal.type == Variable::STRING) {
				int size = (int)leftVal.string->size();
				if (index < 1 || index > size) {
					throw std::runtime_error("String index out of bounds.");
				}

				return Variable::makeString(
					std::string(1, (*leftVal.string)[index - 1])
				);
			} else {
				throw std::runtime_error("Indexing requires a list or string.");
			}
		}
		case ExprType::List: {
			std::vector<Variable> values;
			values.reserve(expr->children.size());

			for (Expr* child : expr->children) {
				values.emplace_back(evalExpr(child, ctx));
			}

			return Variable::makeList(values);
		}
		default:
			throw std::runtime_error("Unknown expression type.");
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