#include "../../compiler/value/VariableValue.hpp"

#include "../../vm/VM.hpp"
#include "math.h"
#include "../../compiler/semantic/SemanticAnalyser.hpp"
#include "../../compiler/value/Function.hpp"

using namespace std;

namespace ls {

VariableValue::VariableValue(Token* name) {
	this->name = name;
	this->var = nullptr;
	constant = false;
}

VariableValue::~VariableValue() {}

void VariableValue::print(ostream& os) const {
	os << name->content;
}

void VariableValue::analyse(SemanticAnalyser* analyser, const Type) {

	var = analyser->get_var(name);
	type = var->type;
	attr_types = var->attr_types;

//	cout << "VV " << name->content << " : " << type << endl;
//	cout << "var scope : " << (int)var->scope << endl;
//	for (auto t : attr_types)
//		cout << t.first << " : " << t.second << endl;
}

extern map<string, jit_value_t> internals;

jit_value_t VariableValue::compile_jit(Compiler& c, jit_function_t& F, Type req_type) const {

//	cout << "compile vv " << name->content << " : " << type << endl;
//	cout << "req type : " << req_type << endl;

	if (var->scope == VarScope::INTERNAL) {

//		cout << "internal" << endl;

		jit_value_t v = internals[name->content];
		if (var->type.nature != Nature::POINTER and req_type.nature == Nature::POINTER) {
			return VM::value_to_pointer(F, v, var->type);
		}
		return v;

	} else if (var->scope == VarScope::LOCAL) {

//		cout << "get local var " << name->content << endl;

		jit_value_t v = c.get_var(name->content).value;
		if (var->type.nature != Nature::POINTER and req_type.nature == Nature::POINTER) {
			return VM::value_to_pointer(F, v, var->type);
		}
		return v;

	} else { // var->scope == VarScope::PARAMETER

		jit_value_t v = jit_value_get_param(F, var->index);
		if (var->type.nature != Nature::POINTER and req_type.nature == Nature::POINTER) {
			return VM::value_to_pointer(F, v, var->type);
		}
		return v;
	}
}

jit_value_t VariableValue::compile_jit_l(Compiler& c, jit_function_t& F, Type) const {

	return compile_jit(c, F, type);
}

}
