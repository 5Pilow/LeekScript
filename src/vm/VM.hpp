#ifndef VM_HPP
#define VM_HPP

#include <vector>
#include <string>
#include <gmp.h>
#include <gmpxx.h>
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "../analyzer/error/Error.hpp"
#include "../compiler/Compiler.hpp"
#include "Exception.hpp"
#include "OutputStream.hpp"
#include "../analyzer/semantic/Call.hpp"

#define OPERATION_LIMIT 10000000

namespace ls {

class Type;
class Module;
class Program;
class LSValue;
class Variable;
class LSNull;
class LSBoolean;
class LSFunction;
class Callable;
class Call;
class Class;
class StandardLibrary;
class Environment;

class VM {
public:

	static const unsigned long int DEFAULT_OPERATION_LIMIT;
	static OutputStream* default_output;

	static void static_init();

	Environment& env;
	StandardLibrary& std;
	std::vector<std::unique_ptr<Module>> modules;
	std::vector<LSValue*> function_created;
	std::vector<Class*> class_created;
	std::unordered_map<std::string, Compiler::value> internals;
	unsigned int operations = 0;
	bool enable_operations = true;
	unsigned int operation_limit;
	OutputStream* output = default_output;
	long mpz_created = 0;
	long mpz_deleted = 0;
	std::string file_name;
	bool legacy;
	Context* context = nullptr;

	VM(Environment& env, StandardLibrary& std);
	~VM();

	void execute(Program& program, bool format = false, bool debug = false, bool ops = true, bool assembly = false, bool pseudo_code = false, bool optimized_ir = false, bool execute_ir = false, bool execute_bitcode = false);

	/** Add a module **/
	void add_module(std::unique_ptr<Module> m);

	void* resolve_symbol(std::string name);

	void add_operations(int operations);
};

}

#endif
