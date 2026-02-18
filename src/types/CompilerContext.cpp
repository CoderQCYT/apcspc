#include "CompilerContext.h"


Variable& CompilerContext::resolveVariable(const std::string& name) {
    auto it = variables.find(name);
    if (it != variables.end()) {
        return it->second;
    }
    if (parent != nullptr) {
        return parent->resolveVariable(name);
    }

    std::cerr << "Undefined variable: " << name << std::endl;
    exit(1);
}