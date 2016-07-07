#ifndef PROGRAM_HPP
#define PROGRAM_HPP

#include "../compiler/value/Block.hpp"
#include "../compiler/value/Function.hpp"
#include "../compiler/semantic/SemanticAnalyser.hpp"

namespace ls {

class LSValue;

class Program {

public:

	std::vector<Function*> functions;
	std::map<std::string, LSValue*> system_vars;
	Value* body;

	void* function;

	Program();
	virtual ~Program();

	void execute();

	void print(std::ostream& os);

	void compile_jit(Compiler&, jit_function_t&, Context&, bool);
};

}

#endif
