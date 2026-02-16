#pragma once
#include <string>

struct Variable;
struct CompilerContext;
Variable evaluateExpr(CompilerContext& ctx, const std::string& expr);
void clearExprCache();