#ifndef MODULE_HPP_
#define MODULE_HPP_

#include <string>
#include <vector>
#include "TypeMutator.hpp"
#include "../analyzer/semantic/CallableVersion.hpp"
#include "../analyzer/semantic/Class.hpp"
#if COMPILER
#include "../compiler/Compiler.hpp"
#include "../vm/value/LSClass.hpp"
#endif

namespace ls {

class LSValue;
class CallableVersion;
class CallableVersionTemplate;
class Module;
class VM;
class Type;
class StandardLibrary;

#if COMPILER
#define ADDR(X) (X)
#else
#define ADDR(x) ((void*) 0x0)
#endif

class Template {
public:
	Module* module;
	std::vector<const Type*> templates;
	template<class... Args>
	Template(Module* module, Args... templates) : module(module), templates({templates...}) {}

	void operator_(std::string name, std::initializer_list<CallableVersionTemplate>, int flags = 0);
	void method(std::string name, std::initializer_list<CallableVersionTemplate> methods, int flags = 0);
};

class Module {
public:
	static int THROWS;
	static int LEGACY;
	static int DEFAULT;
	static int EMPTY_VARIABLE;
	static int NO_RETURN;
	static int PRIVATE;
	static int LEGACY_ONLY;

	static bool STORE_ARRAY_SIZE;

	Environment& env;
	std::string name;
	std::unique_ptr<Class> clazz;
	#if COMPILER
	LSClass* lsclass;
	#endif

	Module(Environment& env, std::string name, Class* parent = nullptr);
	virtual ~Module() {}

	void operator_(std::string name, std::initializer_list<CallableVersionTemplate>, int flags = 0, std::vector<const Type*> templates = {});

	template<class... Args>
	Template template_(Args... templates) {
		return { this, templates... };
	}

	void constructor_(std::initializer_list<CallableVersionTemplate> methods, int flags = 0);

	void method(std::string name, std::initializer_list<CallableVersionTemplate> methods, int flags = 0, std::vector<const Type*> templates = {});

	void field(std::string name, const Type* type);
	#if COMPILER
	void field(std::string name, const Type* type, std::function<Compiler::value(Compiler&, Compiler::value)> fun);
	#endif
	void field(std::string name, const Type* type, void* fun);

	#if COMPILER
	void static_field(std::string name, const Type* type, std::function<Compiler::value(Compiler&)> fun, int flags = 0);
	#endif
	void static_field(std::string name, const Type* type, void* addr, int flags = 0);
	void static_field_fun(std::string name, const Type* type, void* fun, int flags = 0);

	void generate_doc(std::ostream& os, std::string translation);
};

}

#endif
