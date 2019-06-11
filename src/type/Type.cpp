#include <sstream>
#include <numeric>
#include <algorithm>
#include <memory>
#include "Type.hpp"
#include "../constants.h"
#include "../colors.h"
#include "../../lib/utf8.h"
#include "Void_type.hpp"
#include "Any_type.hpp"
#include "Array_type.hpp"
#include "Set_type.hpp"
#include "Function_type.hpp"
#include "Class_type.hpp"
#include "Object_type.hpp"
#include "Class_type.hpp"
#include "Interval_type.hpp"
#include "String_type.hpp"
#include "Number_type.hpp"
#include "Long_type.hpp"
#include "Bool_type.hpp"
#include "Mpz_type.hpp"
#include "Real_type.hpp"
#include "Map_type.hpp"
#include "Null_type.hpp"
#include "Integer_type.hpp"
#include "I8_type.hpp"
#include "Placeholder_type.hpp"
#include "Struct_type.hpp"
#include "Pointer_type.hpp"
#include "Template_type.hpp"
#include "Never_type.hpp"
#include "Compound_type.hpp"
#include "Meta_mul_type.hpp"
#include "Meta_baseof_type.hpp"
#include "../compiler/value/Function.hpp"
#include "../compiler/value/Value.hpp"

namespace ls {

std::vector<const Type*> Type::placeholder_types;
void Type::clear_placeholder_types() {
	placeholder_types.clear();
}
unsigned int Type::placeholder_counter = 0;

const std::vector<const Type*> Type::empty_types;
std::map<std::set<const Type*>, const Type*> Type::compound_types;
std::map<std::pair<const Type*, std::vector<const Type*>>, const Type*> Type::function_types;
std::map<std::pair<const Type*, std::vector<const Type*>>, const Type*> Type::closure_types;
std::map<const Type*, const Type*> Type::array_types;
std::map<const Type*, const Type*> Type::const_array_types;
std::map<const Type*, const Type*> Type::tmp_array_types;
std::map<const Type*, const Type*> Type::set_types;
std::map<const Type*, const Type*> Type::const_set_types;
std::map<const Type*, const Type*> Type::tmp_set_types;
std::map<std::pair<const Type*, const Type*>, const Type*> Type::map_types;
std::map<std::pair<const Type*, const Type*>, const Type*> Type::const_map_types;
std::map<std::pair<const Type*, const Type*>, const Type*> Type::tmp_map_types;
std::map<const Type*, const Type*> Type::pointer_types;
std::map<const Type*, const Type*> Type::temporary_types;
std::map<const Type*, const Type*> Type::not_temporary_types;
std::map<const Type*, const Type*> Type::const_types;
std::map<const Type*, const Type*> Type::not_const_types;
std::map<std::string, const Type*> Type::class_types;
std::map<std::string, const Type*> Type::structure_types;

const Type* const Type::void_ = new Void_type {};
const Type* const Type::boolean = new Bool_type {};
const Type* const Type::const_boolean = Type::boolean->add_constant();
const Type* const Type::number = new Number_type {};
const Type* const Type::const_number = Type::number->add_constant();
const Type* const Type::i8 = new I8_type {};
const Type* const Type::integer = new Integer_type {};
const Type* const Type::const_integer = Type::integer->add_constant();
const Type* const Type::long_ = new Long_type {};
const Type* const Type::const_long = Type::long_->add_constant();
const Type* const Type::mpz = new Mpz_type {};
const Type* const Type::tmp_mpz = Type::mpz->add_temporary();
const Type* const Type::const_mpz = Type::mpz->add_constant();
const Type* const Type::mpz_ptr = Type::mpz->pointer();
const Type* const Type::tmp_mpz_ptr = Type::mpz_ptr->add_temporary();
const Type* const Type::const_mpz_ptr = Type::mpz_ptr->add_constant();

const Type* const Type::never = new Never_type {};
const Type* const Type::any = new Any_type {};
const Type* const Type::const_any = Type::any->add_constant();
const Type* const Type::null = new Null_type {};
const Type* const Type::string = new String_type {};
const Type* const Type::tmp_string = Type::string->add_temporary();
const Type* const Type::const_string = Type::string->add_constant();
const Type* const Type::real = new Real_type {};
const Type* const Type::const_real = Type::real->add_constant();
const Type* const Type::interval = new Interval_type {};
const Type* const Type::const_interval = Type::interval->add_constant();
const Type* const Type::tmp_interval = Type::interval->add_temporary();
const Type* const Type::object = new Object_type {};
const Type* const Type::tmp_object = Type::object->add_temporary();

Type::Type(bool native) : native(native) {
	folded = this;
}

bool Type::must_manage_memory() const {
	if (is_void()) { return false; }
	return is_polymorphic() and not native;
}

const Type* Type::operator + (const Type* type) const {
	if (is_void()) return type;
	if (type->is_void()) return this;
	return Type::compound({this, type});
}
const Type* Type::operator * (const Type* t2) const {
	auto a = this->fold();
	auto b = t2->fold();
	if (a == void_) return b;
	if (b == void_ or a == b) return a;
	if (a->is_polymorphic() and b->is_primitive()) {
		return any;
	}
	if (b->is_polymorphic() and a->is_primitive()) {
		return any;
	}
	if (a->is_bool() or b->is_bool()) {
		return any;
	}
	auto d1 = a->distance(b);
	auto d2 = b->distance(a);
	if (d1 >= 0 and d1 < 100000 and d2 >= 0 and d2 < 100000) {
		if (d1 < d2) return b;
		else return a;
	}
	return any;
}

const Type* Type::fold() const {
	return folded;
}

void Type::toJson(std::ostream& os) const {
	os << "{\"type\":\"" << getJsonName() << "\"";

	if (is_function()) {
		os << ",\"args\":[";
		for (unsigned t = 0; t < arguments().size(); ++t) {
			if (t > 0) os << ",";
			argument(t)->toJson(os);
		}
		os << "]";
		os << ",\"return\":";
		return_type()->toJson(os);
	}
	os << "}";
}

std::string Type::to_string() const {
	std::ostringstream oss;
	oss << this;
	return oss.str();
}

const Type* Type::add_temporary() const {
	if (temporary) return this;
	if (constant) return not_constant()->add_temporary();
	auto i = temporary_types.find(this);
	if (i != temporary_types.end()) return i->second;
	auto type = this->clone();
	type->temporary = true;
	if (this != folded) {
		type->folded = type->folded->add_temporary();
	} else {
		type->folded = type;
	}
	temporary_types.insert({ this, type });
	not_temporary_types.insert({ type, this });
	return type;
}
const Type* Type::not_temporary() const {
	if (not temporary) return this;
	auto i = not_temporary_types.find(this);
	if (i != not_temporary_types.end()) return i->second;
	auto type = this->clone();
	type->temporary = false;
	if (this != folded) {
		type->folded = type->folded->not_temporary();
	} else {
		type->folded = type;
	}
	not_temporary_types.insert({ this, type });
	temporary_types.insert({ type, this });
	return type;
}
const Type* Type::add_constant() const {
	if (constant) return this;
	if (temporary) return not_temporary()->add_constant();
	auto i = const_types.find(this);
	if (i != const_types.end()) return i->second;
	auto type = this->clone();
	type->constant = true;
	if (this != folded) {
		type->folded = type->folded->add_constant();
	} else {
		type->folded = type;
	}
	const_types.insert({ this, type });
	not_const_types.insert({ type, this });
	return type;
}
const Type* Type::not_constant() const {
	if (not constant) return this;
	auto i = not_const_types.find(this);
	if (i != not_const_types.end()) return i->second;
	auto type = this->clone();
	type->constant = false;
	if (this != folded) {
		type->folded = type->folded->not_constant();
	} else {
		type->folded = type;
	}
	not_const_types.insert({ this, type });
	const_types.insert({ type, this });
	return type;
}

const Type* Type::pointer() const {
	auto i = pointer_types.find(this);
	if (i != pointer_types.end()) return i->second;
	if (temporary) {
		auto type = this->not_temporary()->pointer()->add_temporary();
		pointer_types.insert({ this, type });
		return type;
	} else if (constant) {
		auto type = this->not_constant()->pointer()->add_constant();
		pointer_types.insert({ this, type });
		return type;
	} else {
		auto type = new Pointer_type(this);
		pointer_types.insert({ this, type });
		return type;
	}
}

template <class T>
bool Type::is_type() const {
	if (dynamic_cast<const T*>(this) != nullptr) return true;
	if (auto t = dynamic_cast<const Template_type*>(this)) {
		return t->_implementation->is_type<T>();
	}
	if (auto c = dynamic_cast<const Compound_type*>(this)) {
		return c->all([&](const Type* t) {
			return dynamic_cast<const T*>(t) != nullptr;
		});
	}
	return false;
}
template <class T> bool Type::can_be_type() const {
	if (dynamic_cast<const T*>(this) != nullptr) return true;
	if (auto t = dynamic_cast<const Template_type*>(this)) {
		return t->_implementation->is_type<T>();
	}
	if (auto c = dynamic_cast<const Compound_type*>(this)) {
		return c->some([&](const Type* t) {
			return dynamic_cast<const T*>(t) != nullptr;
		});
	}
	return false;
}
bool Type::is_any() const { return is_type<Any_type>(); }
bool Type::is_bool() const { return is_type<Bool_type>(); }
bool Type::can_be_bool() const { return can_be_type<Bool_type>(); }
bool Type::is_number() const { return castable(Type::number, true); }
bool Type::can_be_number() const {
	if (auto c = dynamic_cast<const Compound_type*>(this)) {
		return c->some([&](const Type* type) {
			return type->distance(Type::number) >= 0;
		});
	}
	return distance(Type::number) >= 0;
}
bool Type::can_be_container() const {
	if (auto c = dynamic_cast<const Compound_type*>(this)) {
		return c->some([&](const Type* type) {
			return type->container();
		});
	}
	return is_any() or container();
}
bool Type::can_be_callable() const {
	if (auto c = dynamic_cast<const Compound_type*>(this)) {
		return c->some([&](const Type* type) {
			return type->callable();
		});
	}
	return is_any() or callable();
}
bool Type::can_be_numeric() const {
	return is_any() or can_be_bool() or can_be_number();
}
bool Type::is_real() const { return folded == Type::real; }
bool Type::is_integer() const { return folded == Type::integer; }
bool Type::is_long() const { return folded == Type::long_; }
bool Type::is_mpz() const { return folded == Type::mpz or folded == Type::tmp_mpz or folded == Type::const_mpz; }
bool Type::is_mpz_ptr() const { return folded == Type::mpz_ptr or folded == Type::tmp_mpz_ptr or folded == Type::const_mpz_ptr; }
bool Type::is_string() const { return folded == Type::string or folded == Type::tmp_string or folded == Type::const_string; }
bool Type::is_array() const { return is_type<Array_type>(); }
bool Type::is_set() const { return is_type<Set_type>(); }
bool Type::is_interval() const { return folded == Type::interval or folded == Type::tmp_interval or folded == Type::const_interval; }
bool Type::is_map() const { return is_type<Map_type>(); }
bool Type::is_function() const { return is_type<Function_type>(); }
bool Type::is_object() const { return folded == Type::object; }
bool Type::is_never() const { return folded == Type::never; }
bool Type::is_null() const { return folded == Type::null; }
bool Type::is_class() const { return is_type<Class_type>(); }
bool Type::is_pointer() const { return is_type<Pointer_type>(); }
bool Type::is_struct() const { return is_type<Struct_type>(); }
bool Type::is_closure() const {
	if (auto f = dynamic_cast<const Function_type*>(folded)) {
		return f->closure();
	}
	return false;
}
bool Type::is_polymorphic() const {
	// TODO extends all polymorphic types from Polymorphic_type (Any_type) to improve check
	return dynamic_cast<const String_type*>(folded) != nullptr
		or dynamic_cast<const Array_type*>(folded) != nullptr
		or dynamic_cast<const Set_type*>(folded) != nullptr
		or dynamic_cast<const Map_type*>(folded) != nullptr
		or dynamic_cast<const Interval_type*>(folded) != nullptr
		or dynamic_cast<const Any_type*>(folded) != nullptr
		or dynamic_cast<const Function_type*>(folded) != nullptr
		or dynamic_cast<const Class_type*>(folded) != nullptr
		or dynamic_cast<const Object_type*>(folded) != nullptr
		or dynamic_cast<const Null_type*>(folded) != nullptr;
}
bool Type::is_primitive() const {
	return dynamic_cast<const Integer_type*>(folded) != nullptr
		or dynamic_cast<const Mpz_type*>(folded) != nullptr 
		or dynamic_cast<const Long_type*>(folded) != nullptr 
		or dynamic_cast<const Real_type*>(folded) != nullptr 
		or dynamic_cast<const Bool_type*>(folded) != nullptr
		or (dynamic_cast<const Pointer_type*>(folded) != nullptr and ((Pointer_type*) folded)->pointed()->is_mpz());
}
bool Type::is_void() const {
	return this == Type::void_;
}
bool Type::is_template() const { return is_type<Template_type>(); }

bool Type::castable(const Type* type, bool strictCast) const {
	auto d = distance(type);
	return d >= 0 and (!strictCast or d < 100000);
}
bool Type::strictCastable(const Type* type) const {
	auto d = distance(type);
	return d >= 0 and d < 100;
}

const Type* Type::generate_new_placeholder_type() {
	u_int32_t character = 0x03B1 + placeholder_counter;
	char buff[5];
	u8_toutf8(buff, 5, &character, 1);
	auto type = new Placeholder_type(std::string { buff });
	placeholder_counter++;
	Type::placeholder_types.push_back(type);
	return type;
}

const Type* Type::array(const Type* element) {
	auto i = array_types.find(element);
	if (i != array_types.end()) return i->second;
	auto type = new Array_type(element);
	array_types.insert({element, type});
	return type;
}
const Type* Type::const_array(const Type* element) {
	auto i = const_array_types.find(element);
	if (i != const_array_types.end()) return i->second;
	auto type = array(element)->add_constant();
	const_array_types.insert({element, type});
	return type;
}
const Type* Type::tmp_array(const Type* element) {
	auto i = tmp_array_types.find(element);
	if (i != tmp_array_types.end()) return i->second;
	auto type = array(element)->add_temporary();
	tmp_array_types.insert({element, type});
	return type;
}
const Type* Type::set(const Type* element) {
	auto i = set_types.find(element);
	if (i != set_types.end()) return i->second;
	auto type = new Set_type(element);
	set_types.insert({element, type});
	return type;
}
const Type* Type::const_set(const Type* element) {
	auto i = const_set_types.find(element);
	if (i != const_set_types.end()) return i->second;
	auto type = set(element)->add_constant();
	const_set_types.insert({element, type});
	return type;
}
const Type* Type::tmp_set(const Type* element) {
	auto i = tmp_set_types.find(element);
	if (i != tmp_set_types.end()) return i->second;
	auto type = set(element)->add_temporary();
	tmp_set_types.insert({element, type});
	return type;
}
const Type* Type::map(const Type* key, const Type* element) {
	auto i = map_types.find({key, element});
	if (i != map_types.end()) return i->second;
	auto type = new Map_type(key, element);
	map_types.insert({{key, element}, type});
	return type;
}
const Type* Type::const_map(const Type* key, const Type* element) {
	auto i = const_map_types.find({key, element});
	if (i != const_map_types.end()) return i->second;
	auto type = map(key, element)->add_constant();
	const_map_types.insert({{key, element}, type});
	return type;
}
const Type* Type::tmp_map(const Type* key, const Type* element) {
	auto i = tmp_map_types.find({key, element});
	if (i != tmp_map_types.end()) return i->second;
	auto type = map(key, element)->add_temporary();
	tmp_map_types.insert({{key, element}, type});
	return type;
}
const Type* Type::fun(const Type* return_type, std::vector<const Type*> arguments, const Value* function) {
	if (function == nullptr) {
		std::pair<const Type*, std::vector<const Type*>> key { return_type, arguments };
		auto i = function_types.find(key);
		if (i != function_types.end()) return i->second;
		auto type = new Function_type(return_type, arguments);
		type->constant = true;
		function_types.insert({ key, type });
		return type;
	} else {
		auto t = new Function_type(return_type, arguments, false, function);
		t->constant = true;
		return t;
	}
}
const Type* Type::closure(const Type* return_type, std::vector<const Type*> arguments, const Value* function) {
	if (function == nullptr) {
		std::pair<const Type*, std::vector<const Type*>> key { return_type, arguments };
		auto i = closure_types.find(key);
		if (i != closure_types.end()) return i->second;
		auto type = new Function_type(return_type, arguments, true);
		type->constant = true;
		closure_types.insert({ key, type });
		return type;
	} else {
		auto t = new Function_type(return_type, arguments, true, function);
		t->constant = true;
		return t;
	}
}
const Type* Type::structure(const std::string name, std::initializer_list<const Type*> types) {
	auto i = structure_types.find(name);
	if (i != structure_types.end()) return i->second;
	auto type = new Struct_type(name, types);
	structure_types.insert({ name, type });
	return type;
}
const Type* Type::clazz(const std::string name) {
	auto i = class_types.find(name);
	if (i != class_types.end()) return i->second;
	auto type = new Class_type(name);
	class_types.insert({ name, type });
	return type;
}
const Type* Type::const_class(const std::string name) {
	return clazz(name)->add_constant();
}
const Type* Type::template_(std::string name) {
	return new Template_type(name);
}
const Type* Type::compound(std::initializer_list<const Type*> types) {
	if (types.size() == 1) return *types.begin();
	std::set<const Type*> base;
	auto folded = Type::void_;
	for (const auto& t : types) {
		if (auto c = dynamic_cast<const Compound_type*>(t)) {
			for (const auto& bt : c->types) base.insert(bt);
		} else {
			base.insert(t);
		}
		folded = folded->operator * (t);
	}
	if (base.size() == 1) return *types.begin();
	auto i = compound_types.find(base);
	if (i != compound_types.end()) return i->second;
	auto type = new Compound_type { base, folded };
	compound_types.insert({ base, type });
	return type;
}

const Type* Type::meta_mul(const Type* t1, const Type* t2) {
	return new Meta_mul_type(t1, t2);
}
const Type* Type::meta_base_of(const Type* type, const Type* base) {
	return new Meta_baseof_type(type, base);
}

std::ostream& operator << (std::ostream& os, const Type* type) {
	if (type->constant and not type->is_function()) {
		os << BLUE_BOLD << "const:" << END_COLOR;
	}
	type->print(os);
	if (type->temporary) {
		os << BLUE_BOLD << "&&" << END_COLOR;
	} else if (type->reference) {
		os << BLUE_BOLD << "&" << END_COLOR;
	}
	return os;
}

std::ostream& operator << (std::ostream& os, const std::vector<const Type*>& types) {
	os << "[";
	for (unsigned i = 0; i < types.size(); ++i) {
		if (i > 0) os << ", ";
		os << types[i];
	}
	os << "]";
	return os;
}

std::ostream& operator << (std::ostream& os, const llvm::Type* type) {
	std::string str;
	llvm::raw_string_ostream rso(str);
	type->print(rso);
	os << rso.str();
	return os;
}

}
