#include "Functions.hpp"
#include "../value/LSArray.hpp"
#include "../value/LSString.hpp"

namespace ls {
namespace legacy {

void Functions::add(VM* vm, std::string name, Type return_type, std::vector<Type> args, void* fun) {
	auto f = new LSFunction(fun);
	f->native = true;
	auto type = Type::FUNCTION;
	type.native = true;
	type.setReturnType(return_type);
	for (size_t i = 0; i < args.size(); ++i) {
		type.setArgumentType(i, args.at(i));
	}
	vm->system_vars.insert({name, f});
	vm->add_internal_var(name, type);
}

void Functions::create(VM* vm) {
	add(vm, "debug", {}, {Type::ANY}, (void*) &Functions::v1_debug);
	add(vm, "count", Type::INTEGER, {Type::ANY}, (void*) &Functions::v1_count);
	add(vm, "charAt", Type::STRING, {Type::STRING, Type::INTEGER}, (void*) &Functions::v1_charAt);
	add(vm, "pushAll", {}, {Type::PTR_ARRAY, Type::PTR_ARRAY}, (void*) &Functions::v1_pushAll);
	add(vm, "replace", Type::STRING, {Type::STRING, Type::STRING, Type::STRING}, (void*) &Functions::v1_replace);
}

void Functions::v1_debug(LSValue* v) {
	v->print(*VM::current()->output);
	LSValue::delete_temporary(v);
	*VM::current()->output << std::endl;
}

LSValue* Functions::v1_charAt(LSString* v, int p) {
	auto s = v->charAt(p);
	LSValue::delete_temporary(v);
	return s;
}

LSValue* Functions::v1_replace(LSString* string, LSString* from, LSString* to) {
	std::string str(*string);
	size_t start_pos = 0;
	// Replace \\ by \ (like Java does)
	std::string f = *from;
	while((start_pos = f.find("\\\\", start_pos)) != std::string::npos) {
		f.replace(start_pos, 2, "\\");
		start_pos += 1;
	}
	start_pos = 0;
	std::string t = *to;
	while((start_pos = t.find("\\\\", start_pos)) != std::string::npos) {
		t.replace(start_pos, 2, "\\");
		start_pos += 1;
	}
	start_pos = 0;
	while((start_pos = str.find(f, start_pos)) != std::string::npos) {
		str.replace(start_pos, from->length(), t);
		start_pos += t.size();
	}
	if (string->refs == 0) { delete string; }
	if (from->refs == 0) { delete from; }
	if (to->refs == 0) { delete to; }
	return new LSString(str);
}

int Functions::v1_count(LSArray<LSValue*>* a) {
	int s = a->size();
	LSValue::delete_temporary(a);
	return s;
}

LSValue* Functions::v1_pushAll(LSArray<LSValue*>* a, LSArray<LSValue*>* b) {
	return a->ls_push_all_ptr(b);
}

}
}
