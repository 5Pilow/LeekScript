#include <sstream>
#include <chrono>

#include "VM.hpp"

#include "../compiler/lexical/LexicalAnalyser.hpp"
#include "../compiler/syntaxic/SyntaxicAnalyser.hpp"
#include "Context.hpp"
#include "../compiler/semantic/SemanticAnalyser.hpp"
#include "../compiler/semantic/SemanticException.hpp"
#include "value/LSNumber.hpp"
#include "value/LSArray.hpp"
#include "Program.hpp"

using namespace std;

namespace ls {

VM::VM() {}

VM::~VM() {}

unsigned int VM::operations = 0;
const bool VM::enable_operations = true;
const unsigned int VM::operation_limit = 2000000000;

map<string, jit_value_t> internals;

void VM::add_module(Module* m) {
	modules.push_back(m);
}

Program* VM::compile(const std::string code) {

	LexicalAnalyser lex;
	vector<Token> tokens = lex.analyse(code);

	SyntaxicAnalyser syn;
	Program* program = syn.analyse(tokens);

	if (syn.getErrors().size() > 0) {
		for (auto error : syn.getErrors()) {
			cout << "Line " << error->token->line << " : " <<  error->message << endl;
		}
		return nullptr;
	}

	Compiler c;
	Context context { "{}" };

	try {
		SemanticAnalyser sem;
		sem.analyse(program, &context, modules);
	} catch (SemanticException& e) {
		cout << "Line " << e.token->line << " : " << e.message() << endl;
		return nullptr;
	}

	internals.clear();

	jit_init();
	jit_context_t jit_context = jit_context_create();
	jit_context_build_start(jit_context);

	jit_type_t params[0] = {};
	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, JIT_INTEGER_LONG, params, 0, 1);
	jit_function_t F = jit_function_create(jit_context, signature);

	program->compile_jit(c, F, context, false);

	jit_function_compile(F);
	jit_context_build_end(jit_context);

	program->function = jit_function_to_closure(F);
	return program;
}

string VM::execute(const std::string code, std::string ctx, ExecMode mode) {

	LSValue::obj_count = 0;
	LSValue::obj_deleted = 0;

	auto compile_start = chrono::high_resolution_clock::now();

	LexicalAnalyser lex;
	vector<Token> tokens = lex.analyse(code);

	SyntaxicAnalyser syn;
	Program* program = syn.analyse(tokens);

	if (syn.getErrors().size() > 0) {
		if (mode == ExecMode::COMMAND_JSON) {

			cout << "{\"success\":false,\"errors\":[";
			for (auto error : syn.getErrors()) {
				cout << "{\"line\":" << error->token->line << ",\"message\":\"" << error->message << "\"}";
			}
			cout << "]}" << endl;
			return ctx;

		} else {
			for (auto error : syn.getErrors()) {
				cout << "Line " << error->token->line << " : " <<  error->message << endl;
			}
			return ctx;
		}
	}

	Compiler c;
	Context context { ctx };

	try {
		SemanticAnalyser sem;
		sem.analyse(program, &context, modules);

	} catch (SemanticException& e) {

		if (mode == ExecMode::COMMAND_JSON) {
			cout << "{\"success\":false,\"errors\":[{\"line\":" << e.token->line << ",\"message\":\"" << e.message() << "\"}]}" << endl;
		} else if (mode == ExecMode::TEST) {

			delete program;
			throw e;
//			return std::to_string(e.type);
		} else {
			cout << "Line " << e.token->line << " : " << e.message() << endl;
		}
		if (mode == ExecMode::TEST) {
			return "<error>";
		}
		return ctx;
	}

	// Compilation
	internals.clear();

	jit_init();
	jit_context_t jit_context = jit_context_create();
	jit_context_build_start(jit_context);

	jit_type_t params[0] = {};
	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, JIT_INTEGER_LONG, params, 0, 0);
	jit_function_t F = jit_function_create(jit_context, signature);
	jit_insn_uses_catcher(F);

	bool toplevel = mode != ExecMode::NORMAL && mode != ExecMode::TEST;
	program->compile_jit(c, F, context, toplevel);

	// catch (ex) {
	jit_value_t ex = jit_insn_start_catcher(F);
	VM::print_int(F, ex);
	jit_insn_return(F, JIT_CREATE_CONST_POINTER(F, LSNull::null_var));

	jit_function_compile(F);
	jit_context_build_end(jit_context);

	typedef LSValue* (*FF)();
	FF fun = (FF) jit_function_to_closure(F);

	auto compile_end = chrono::high_resolution_clock::now();

	/*
	 * Execute
	 */
	operations = 0;

	auto exe_start = chrono::high_resolution_clock::now();
	LSValue* res = fun();
	auto exe_end = chrono::high_resolution_clock::now();

	long exe_time_ns = chrono::duration_cast<chrono::nanoseconds>(exe_end - exe_start).count();
	long compile_time_ns = chrono::duration_cast<chrono::nanoseconds>(compile_end - compile_start).count();

	double exe_time_ms = (((double) exe_time_ns / 1000) / 1000);
	double compile_time_ms = (((double) compile_time_ns / 1000) / 1000);

	/*
	 * Return results
	 */
	string result;

	if (mode == ExecMode::COMMAND_JSON || mode == ExecMode::TOP_LEVEL) {

		LSArray<LSValue*>* res_array = (LSArray<LSValue*>*) res;

		ostringstream oss;
		res_array->operator[] (0)->print(oss);
		result = oss.str();

		string ctx = "{";

		unsigned i = 0;
/*
		for (auto g : globals) {
			if (globals_ref[g.first]) continue;
			LSValue* v = res_array->operator[] (i + 1);
			ctx += "\"" + g.first + "\":" + v->to_json();
			if (i < globals.size() - 1) ctx += ",";
			i++;
		}
		*/
		ctx += "}";
		delete res_array;

		if (mode == ExecMode::TOP_LEVEL) {
			cout << result << endl;
			cout << "(" << VM::operations << " ops, " << compile_time_ms << " ms + " << exe_time_ms << " ms)" << endl;
			result = ctx;
		} else {
			cout << "{\"success\":true,\"ops\":" << VM::operations << ",\"time\":" << exe_time_ns << ",\"ctx\":" << ctx << ",\"res\":\""
					<< result << "\"}" << endl;
			result = ctx;
		}

	} else if (mode == ExecMode::FILE_JSON) {

		LSArray<LSValue*>* res_array = (LSArray<LSValue*>*) res;

		ostringstream oss;
		res_array->operator[] (0)->print(oss);
		result = oss.str();

		LSValue::delete_val(res);

		cout << "{\"success\":true,\"ops\":" << VM::operations << ",\"time\":" << exe_time_ns
			 << ",\"ctx\":" << ctx << ",\"res\":\"" << result << "\"}" << endl;


	} else if (mode == ExecMode::NORMAL) {

		ostringstream oss;
		res->print(oss);
		LSValue::delete_val(res);
		string res_string = oss.str();

		cout << res_string << endl;
		cout << "(" << VM::operations << " ops, " << compile_time_ms << "ms + " << exe_time_ms << " ms)" << endl;

		result = ctx;

	} else if (mode == ExecMode::TEST) {

		ostringstream oss;
		res->print(oss);
		result = oss.str();

		LSValue::delete_val(res);

	} else if (mode == ExecMode::TEST_OPS) {

		LSValue::delete_val(res);
		result = to_string(VM::operations);
	}

	/*
	 * Cleaning
	 */
	delete program;

	if (ls::LSValue::obj_deleted != ls::LSValue::obj_count) {
		//cout << "/!\\ " << LSValue::obj_deleted << " / " << LSValue::obj_count << " (" << (LSValue::obj_count - LSValue::obj_deleted) << " leaked)" << endl;
	}

	return result;
}

LSValue* create_null_object(int) {
	return LSNull::null_var;
}
LSValue* create_number_object_int(int n) {
	return LSNumber::get(n);
}
LSValue* create_number_object_long(long n) {
	return LSNumber::get(n);
}
LSValue* create_bool_object(bool n) {
	return new LSBoolean(n);
}
LSValue* create_func_object(void* f) {
	return new LSFunction(f);
}
LSValue* create_float_object(double n) {
	return LSNumber::get(n);
}

void* get_conv_fun(Type type) {
	if (type.raw_type == RawType::NULLL) {
		return (void*) &create_null_object;
	}
	if (type.raw_type == RawType::INTEGER) {
		return (void*) &create_number_object_int;
	}
	if (type.raw_type == RawType::LONG) {
		return (void*) &create_number_object_long;
	}
	if (type.raw_type == RawType::FLOAT) {
		return (void*) &create_float_object;
	}
	if (type.raw_type == RawType::BOOLEAN) {
		return (void*) &create_bool_object;
	}
	if (type.raw_type == RawType::FUNCTION) {
		return (void*) &create_func_object;
	}
	return (void*) &create_number_object_int;
}

jit_value_t VM::value_to_pointer(jit_function_t& F, jit_value_t& v, Type type) {

	void* fun = get_conv_fun(type);

	bool floatt = jit_type_get_kind(jit_value_get_type(v)) == JIT_TYPE_FLOAT64;
	if (floatt) {
		fun = (void*) &create_float_object;
	}

	jit_type_t args_types[1] = {
		(type.raw_type == RawType::FUNCTION) ? JIT_POINTER :
		(type.raw_type == RawType::LONG) ? JIT_INTEGER_LONG :
		(type.raw_type == RawType::FLOAT) ?	JIT_FLOAT :
			JIT_INTEGER
	};
	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, JIT_POINTER, args_types, 1, 0);
	return jit_insn_call_native(F, "convert", (void*) fun, sig, &v, 1, JIT_CALL_NOTHROW);
}

int boolean_to_value(LSBoolean* b) {
	return b->value;
}

jit_value_t VM::pointer_to_value(jit_function_t& F, jit_value_t& v, Type type) {

	if (type == Type::BOOLEAN) {
		jit_type_t args_types[1] = {JIT_POINTER};
		jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, JIT_INTEGER, args_types, 1, 0);
		return jit_insn_call_native(F, "convert", (void*) boolean_to_value, sig, &v, 1, JIT_CALL_NOTHROW);
	}
	return JIT_CREATE_CONST(F, JIT_INTEGER, 0);
}

/*
bool VM::get_number(jit_function_t& F, jit_value_t& val) {

	// x & (1 << 31) == 0

	jit_value_t is_int = jit_insn_eq(F,
		jit_insn_and(F, val, jit_value_create_nint_constant(F, jit_type_int, 2147483648)),
		jit_value_create_nint_constant(F, jit_type_int, 0)
	);

	jit_value_t res;

	jit_label_t label_else;
	jit_insn_branch_if_not(F, is_int, &label_else);

	return false;
}*/

LSArray<LSValue*>* new_array() {
	return new LSArray<LSValue*>();
}

void push_array_value(LSArray<LSValue*>* array, int value) {
	array->push_clone(LSNumber::get(value));
}

void push_array_pointer(LSArray<LSValue*>* array, LSValue* value) {
	array->push_clone(value);
}

jit_value_t VM::new_array(jit_function_t& F) {
	jit_type_t args_t[0] = {};
	jit_value_t args[0] = {};
	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, jit_type_int, args_t, 0, 0);
	return jit_insn_call_native(F, "new", (void*) new_array, sig, args, 0, JIT_CALL_NOTHROW);
}

void VM::push_array_value(jit_function_t& F, jit_value_t& array, jit_value_t& value) {
	jit_type_t args[2] = {JIT_INTEGER, JIT_INTEGER};
	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, jit_type_void, args, 2, 0);
	jit_value_t args_v[] = {array, value};
	jit_insn_call_native(F, "push", (void*) push_array_value, sig, args_v, 2, JIT_CALL_NOTHROW);
}

void VM::push_array_pointer(jit_function_t& F, jit_value_t& array, jit_value_t& value) {
	jit_type_t args[2] = {JIT_INTEGER, JIT_INTEGER};
	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, jit_type_void, args, 2, 0);
	jit_value_t args_v[] = {array, value};
	jit_insn_call_native(F, "push", (void*) push_array_pointer, sig, args_v, 2, JIT_CALL_NOTHROW);
}

int VM_get_refs(LSValue* val) {
	return val->refs;
}

jit_value_t VM::get_refs(jit_function_t& F, jit_value_t& obj) {
	jit_type_t args[1] = {JIT_POINTER};
	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, JIT_POINTER, args, 1, 0);
	return jit_insn_call_native(F, "get_refs", (void*) VM_get_refs, sig, &obj, 1, JIT_CALL_NOTHROW);
}

void VM_inc_refs(LSValue* val) {
//	val->print(cout);
//	cout << " inc refs" << endl;
	val->refs++;
}

void VM::inc_refs(jit_function_t& F, jit_value_t& obj) {
	jit_type_t args[1] = {JIT_POINTER};
	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, jit_type_void, args, 1, 0);
	jit_insn_call_native(F, "inc_refs", (void*) VM_inc_refs, sig, &obj, 1, JIT_CALL_NOTHROW);
}

void VM_delete(LSValue* ptr) {
	LSValue::delete_val(ptr);
}

void VM::delete_obj(jit_function_t& F, jit_value_t& obj) {
	jit_type_t args[1] = {JIT_POINTER};
	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, jit_type_void, args, 1, 0);
	jit_insn_call_native(F, "delete", (void*) VM_delete, sig, &obj, 1, JIT_CALL_NOTHROW);
//	jit_insn_store(F, obj, JIT_CREATE_CONST(F, JIT_INTEGER, 0));
}

void VM_delete_temporary(LSValue* val) {
	if (val->refs == 0) {
//		cout << "delete temporary" << endl;
		delete val;
	}
}

void VM::delete_temporary(jit_function_t& F, jit_value_t& obj) {
	jit_type_t args[1] = {JIT_POINTER};
	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, jit_type_void, args, 1, 0);
	jit_insn_call_native(F, "delete_temporary", (void*) VM_delete_temporary, sig, &obj, 1, JIT_CALL_NOTHROW);
}

void VM_operation_exception() {
	throw vm_operation_exception();
}

void VM::inc_ops(jit_function_t& F, int add) {

	if (!enable_operations) return;

	// Variable counter pointer
	jit_value_t jit_ops_ptr = jit_value_create_long_constant(F, jit_type_void_ptr, (long int) &VM::operations);

	// Increment counter
	jit_value_t jit_ops = jit_insn_load_relative(F, jit_ops_ptr, 0, jit_type_uint);
	jit_insn_store_relative(F, jit_ops_ptr, 0, jit_insn_add(F, jit_ops, jit_value_create_nint_constant(F, jit_type_uint, add)));

	// Compare to the limit
	jit_value_t compare = jit_insn_gt(F, jit_ops, jit_value_create_nint_constant(F, jit_type_uint, VM::operation_limit));
	jit_label_t label_end = jit_label_undefined;
	jit_insn_branch_if_not(F, compare, &label_end);

	// If greater than the limit, throw exception
//	jit_type_t args[1] = {JIT_INTEGER};
//	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, jit_type_void, args, 1, 0);
//	jit_insn_call_native(F, "throw_exception", (void*) VM_operation_exception, sig, &jit_ops, 1, JIT_CALL_NOTHROW);
	jit_insn_throw(F, jit_value_create_nint_constant(F, jit_type_int, 12));

	// End
	jit_insn_label(F, &label_end);
}

void VM_print_int(int val) {
//	cout << val << endl;
	cout << "Execution ended, too much operations: " << VM::operations << " (" << val << ")" << endl;
}

void VM::print_int(jit_function_t& F, jit_value_t& val) {
	jit_type_t args[1] = {JIT_INTEGER};
	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, jit_type_void, args, 1, 0);
	jit_insn_call_native(F, "print_int", (void*) VM_print_int, sig, &val, 1, JIT_CALL_NOTHROW);
}

}
