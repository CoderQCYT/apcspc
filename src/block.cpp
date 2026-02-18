#include "block.h"
#include "expressions.h"
#include "util.h"

#include "types/CompilerContext.h"
#include "types/Variable.h"
#include "types/ExecResult.h"

const std::string ASSIGNMENT_ARROW = "←"; // eheheheheh

static size_t findEndingBlock(const std::vector<std::string>& lines, size_t start) {
	size_t end = start;
	int blockDepth = 0;	
	for (; end < lines.size(); ++end) {
		std::string blockLine = trim(lines[end]);
		if (blockLine == "{") blockDepth++;
		else if (blockLine == "}") {
			blockDepth--;
			if (blockDepth == 0) break;
		}
	}
	return end;
}

// the actual shit
ExecResult runBlock(CompilerContext& ctx, const std::vector<std::string>& lines) {
	for (size_t i = 0; i < lines.size(); ++i) {
		const std::string& line = lines[i];

		if (line == "{" || line == "}") continue;

		// Special things
		if (line == "ENABLE_QC_EXTENSIONS") {
			ctx.qcExtensionsEnabled = true;
			continue;
		}

		if (line == "DISABLE_QC_EXTENSIONS") {
			ctx.qcExtensionsEnabled = false;
			continue;
		}

		// IF(condition)
		// {
		//  ... block ...
		// }
		// ELSE
		// {	
		//  ... block ...
		// }
		if (line.starts_with("IF")) {
			const std::string condition = trim(line.substr(2));
			const Variable condVal = evaluateExpr(ctx, condition);

			const size_t blockStart = i + 1;
			const size_t blockEnd = findEndingBlock(lines, blockStart);

			if (blockEnd == lines.size()) {
				std::cerr << "Syntax error: Invalid IF block.\n";
				continue;
			}

			const size_t elseStart = blockEnd + 1;
			bool hasElse = false;
			size_t elseEnd = blockEnd;

			if (elseStart < lines.size() && trim(lines[elseStart]) == "ELSE") {
				hasElse = true;
				elseEnd = findEndingBlock(lines, elseStart + 1);

				if (elseEnd == lines.size()) {
					std::cerr << "Syntax error: Invalid ELSE block.\n";
					continue;
				}
			}

			if (condVal.toBoolean()) {
				// Run IF block
				const std::vector<std::string> block(
					lines.begin() + blockStart,
					lines.begin() + blockEnd
				);

				const ExecResult& r = runBlock(ctx, block);
				if (r.signal != ExecSignal::None)
					return r;

				// Skip entire IF/ELSE structure
				i = hasElse ? elseEnd : blockEnd;
			}
			else if (hasElse) {
				// Run ELSE block
				const std::vector<std::string> elseBlock(
					lines.begin() + elseStart + 1,
					lines.begin() + elseEnd
				);

				const ExecResult& r = runBlock(ctx, elseBlock);
				if (r.signal != ExecSignal::None)
					return r;

				i = elseEnd;
			}
			else {
				i = blockEnd;
			}

			continue;
		}


		// REPEAT UNTIL condition
		// {
		//  ... block ...
		// }
		if (line.starts_with("REPEAT UNTIL")) {
			const std::string condition = trim(line.substr(12));
			
			const size_t blockStart = i + 1;
			const size_t blockEnd = findEndingBlock(lines, blockStart);
			if (blockEnd == lines.size()) {
				std::cerr << "Syntax error: Invalid block for REPEAT UNTIL." << std::endl;
				continue;
			}
			const std::vector<std::string> block(lines.begin() + blockStart, lines.begin() + blockEnd);
			while (!evaluateExpr(ctx, condition).toBoolean()) {
				ExecResult r = runBlock(ctx, block);
				if (r.signal != ExecSignal::None)
					return r;
			}
			i = blockEnd;
			continue;
		}

		// REPEAT n TIMES
		// {
		//  ... block ...
		// }
		if (line.starts_with("REPEAT")) {
			const size_t timesPos = line.find("TIMES");
			if (timesPos == std::string::npos) {
				std::cerr << "Syntax error: REPEAT must be followed by a number and TIMES" << std::endl;
				continue;
			}

			const size_t blockStart = i + 1;
			const size_t blockEnd = findEndingBlock(lines, blockStart);
			if (blockEnd == lines.size()) {
				std::cerr << "Syntax error: Invalid block for REPEAT." << std::endl;
				continue;
			}
			const std::vector<std::string> block(lines.begin() + blockStart, lines.begin() + blockEnd);
			const std::string countString = trim(line.substr(6, timesPos - 6));
			for (size_t count = 0; count < evaluateExpr(ctx, countString).number; ++count) {
				ExecResult r = runBlock(ctx, block);
				if (r.signal != ExecSignal::None)
					return r;
			}

			i = blockEnd;
			continue;
		}
		
		// PROCEDURE procName(param1, param2, ...)
		// {
		//  ... block ...
		// }
		if (line.starts_with("PROCEDURE")) {
			const size_t nameStart = 9;
			const size_t paramsStart = line.find('(', nameStart);
			const std::string procName = trim(line.substr(nameStart, paramsStart - nameStart));
			const size_t paramsEnd = line.find_last_of(')');
			std::vector<std::string> parameters;

			if (paramsStart != std::string::npos && paramsEnd != std::string::npos && paramsEnd > paramsStart) {
				std::string paramsString = line.substr(paramsStart + 1, paramsEnd - paramsStart - 1);
				size_t pos = 0;
				while (pos < paramsString.size()) {
					size_t commaPos = paramsString.find(',', pos);
					if (commaPos == std::string::npos) commaPos = paramsString.size();
					parameters.push_back(trim(paramsString.substr(pos, commaPos - pos)));
					pos = commaPos + 1;
				}
			}

			const size_t blockStart = i + 1;
			const size_t blockEnd = findEndingBlock(lines, blockStart);
			Procedure proc{ parameters, std::vector<std::string>(lines.begin() + blockStart, lines.begin() + blockEnd) };
			ctx.variables[procName] = Variable::makeProcedure(proc);
			i = blockEnd;
			continue;
		}

		// FOR EACH item IN aList
		// {
		//  ... block ...
		// }
		if (line.starts_with("FOR EACH")) {
			const size_t inPos = line.find("IN", 8);
			if (inPos == std::string::npos) {
				std::cerr << "Syntax error: FOR EACH must be followed by an item name, IN, and then a list." << std::endl;
				continue;
			}

			const std::string itemName = trim(line.substr(8, inPos - 8));
			const std::string listExpr = trim(line.substr(inPos + 2));
			const Variable listVal = evaluateExpr(ctx, listExpr);
			if (listVal.type != Variable::LIST) {
				std::cerr << "Type error: FOR EACH expects a list expression after IN." << std::endl;
				continue;
			}

			const size_t blockStart = i + 1;
			const size_t blockEnd = findEndingBlock(lines, blockStart);
			if (blockEnd == lines.size()) {
				std::cerr << "Syntax error: Invalid block for FOR EACH." << std::endl;
				continue;
			}
			for (const Variable& item : *listVal.list) {
				ctx.variables[itemName] = item;
				std::vector<std::string> block(lines.begin() + blockStart, lines.begin() + blockEnd);
				ExecResult r = runBlock(ctx, block);
				if (r.signal != ExecSignal::None)
					return r;
			}

			i = blockEnd;
			continue;
		}

		// RETURN(expression)
		if (line.starts_with("RETURN")) {
			std::string expr = trim(line.substr(6));
			return ExecResult::ret(evaluateExpr(ctx, expr));
		}

		// if it's not an assignment, just evaluate it as an expression (e.g. procedure call)
		evaluateExpr(ctx, line);
		continue;
	}

	return ExecResult::normal();
}