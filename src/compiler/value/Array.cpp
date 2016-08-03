#include "../../compiler/value/Array.hpp"

#include "../../vm/VM.hpp"
#include "../../vm/value/LSInterval.hpp"
#include "../../vm/value/LSMap.hpp"
#include <math.h>

using namespace std;

namespace ls {

Array::Array() {
	type = Type::ARRAY;
}

void Array::addValue(Value* value, Value* key) {

	expressions.push_back(value);
	keys.push_back(key);
	if (key != nullptr) {
		associative = true;
	}
}

Array::~Array() {
	for (auto ex : keys) {
		delete ex;
	}
	for (auto ex : expressions) {
		delete ex;
	}
}

void Array::print(std::ostream& os, int indent, bool debug) const {
	os << "[";
	for (unsigned i = 0; i < expressions.size(); ++i) {
		if (associative && (Value*)keys.at(i) != nullptr) {
			keys.at(i)->print(os, indent, debug);
			os << " : ";
		}
		expressions.at(i)->print(os, indent, debug);
		if (i < expressions.size() - 1) {
			os << ", ";
		}
	}
	os << "]";

	if (debug) {
		os << " " << type;
	}
}

unsigned Array::line() const {
	return 0;
}

void Array::analyse(SemanticAnalyser* analyser, const Type&) {

	constant = true;

	if (interval) {

		type = Type::INTERVAL;

	} else {

		for (Value* key : keys) {
			if (key != nullptr) {
				key->analyse(analyser, Type::UNKNOWN);
			}
		}

		if (expressions.size() > 0) {

			Type element_type = Type::UNKNOWN;

			for (unsigned i = 0; i < expressions.size(); ++i) {

				Value* ex = expressions[i];
				ex->analyse(analyser, Type::UNKNOWN);

				if (ex->constant == false) {
					constant = false;
				}
				if (i == 0) {
					element_type = ex->type;
				} else {
					element_type = Type::get_compatible_type(element_type, ex->type);
				}
			}
			type.setElementType(element_type);

			// Native elements types supported : integer, double and pointer
			supported_type = element_type == Type::INTEGER || element_type == Type::FLOAT ?
				element_type : Type::POINTER;

			// Re-analyze expressions with the supported type
			for (unsigned i = 0; i < expressions.size(); ++i) {
				expressions[i]->analyse(analyser, supported_type);
			}
			type.setElementType(supported_type);
		}
	}

//	cout << "Array type : " << type << endl;
}

void Array::elements_will_take(SemanticAnalyser* analyser, const unsigned pos, const Type& type, int level) {

	for (unsigned i = 0; i < expressions.size(); ++i) {

		Array* arr = dynamic_cast<Array*>(expressions[i]);
		if (arr != nullptr and level > 0) {
			arr->elements_will_take(analyser, pos, type, level - 1);
		} else {
			expressions[i]->will_take(analyser, pos, type);
		}
	}
}

LSArray<LSValue*>* LSArray_create() {
	return new LSArray<LSValue*>();
}

LSArray<int>* LSArray_create_integer() {
	return new LSArray<int>();
}

LSArray<double>* LSArray_create_float() {
	return new LSArray<double>();
}

LSMap<LSValue*>* LSArray_create_map() {
	return new LSMap<LSValue*>();
}

LSInterval* LSArray_create_interval(int a, int b) {
	LSInterval* interval = new LSInterval();
	interval->a = a;
	interval->b = b;
	return interval;
}

void LSArray_push(LSArray<LSValue*>* array, LSValue* value) {
	array->push_clone(value);
	LSValue::delete_val(value);
}

void LSArray_push_integer(LSArray<int>* array, int value) {
	array->push_clone(value);
}

void LSArray_push_float(LSArray<double>* array, double value) {
	array->push_clone(value);
}

void Array_push_map(LSMap<LSValue*>* map, LSValue* key, LSValue* value) {
	map->push_key_clone(key, value);
}

jit_value_t Array::compile(Compiler& c) const {

	if (interval) {

		jit_value_t a = expressions[0]->compile(c);
		jit_value_t b = expressions[1]->compile(c);

		jit_type_t args[3] = {JIT_INTEGER, JIT_INTEGER};
		jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, JIT_POINTER, args, 2, 0);
		jit_value_t args_v[] = {a, b};

		jit_value_t interval = jit_insn_call_native(c.F, "new", (void*) LSArray_create_interval, sig, args_v, 2, JIT_CALL_NOTHROW);

		return interval;
	}

	if (associative) {

		jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, JIT_POINTER, {}, 0, 0);

		jit_value_t array = jit_insn_call_native(c.F, "new", (void*) LSArray_create_map, sig, {}, 0, JIT_CALL_NOTHROW);

		for (unsigned i = 0; i < expressions.size(); ++i) {

			jit_value_t v = expressions[i]->compile(c);
			jit_value_t k = keys[i]->compile(c);

			jit_type_t args[3] = {JIT_POINTER, JIT_POINTER, JIT_POINTER};
			jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, jit_type_void, args, 3, 0);
			jit_value_t args_v[] = {array, k, v};
			jit_insn_call_native(c.F, "push", (void*) Array_push_map, sig, args_v, 3, JIT_CALL_NOTHROW);
		}

		return array;
	}

	jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, JIT_POINTER, {}, 0, 0);

	void* create = type.getElementType() == Type::INTEGER ?
			(void*) LSArray_create_integer :
			type.getElementType() == Type::FLOAT ? (void*) LSArray_create_float :
			(void*) LSArray_create;

	void* push = type.getElementType() == Type::INTEGER ?
			(void*) LSArray_push_integer :
			type.getElementType() == Type::FLOAT ? (void*) LSArray_push_float :
			(void*) LSArray_push;

	jit_type_t elem_type = type.getElementType() == Type::INTEGER ? JIT_INTEGER :
			type.getElementType() == Type::FLOAT ? JIT_FLOAT : JIT_POINTER;

	jit_value_t array = jit_insn_call_native(c.F, "new", create, sig, {}, 0, JIT_CALL_NOTHROW);

	for (Value* val : expressions) {

		jit_value_t v = val->compile(c);

		if (supported_type.must_manage_memory()) {
			VM::inc_refs(c.F, v);
		}

		jit_type_t args[2] = {JIT_POINTER, elem_type};
		jit_type_t sig = jit_type_create_signature(jit_abi_cdecl, jit_type_void, args, 2, 0);
		jit_value_t args_v[] = {array, v};
		jit_insn_call_native(c.F, "push", push, sig, args_v, 2, JIT_CALL_NOTHROW);
	}

	// size of the array + 1 operations
	VM::inc_ops(c.F, expressions.size() + 1);

	return array;
}

}
