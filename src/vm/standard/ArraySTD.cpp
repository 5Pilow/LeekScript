#include <algorithm>
#include "ArraySTD.hpp"
#include "../value/LSNumber.hpp"
#include "../value/LSArray.hpp"
#include "ValueSTD.hpp"
#include "../TypeMutator.hpp"
#include "../../type/Type.hpp"
#include "../../compiler/semantic/Variable.hpp"

namespace ls {

ArraySTD::ArraySTD(VM* vm) : Module(vm, "Array") {

	LSArray<LSValue*>::clazz = clazz.get();
	LSArray<int>::clazz = clazz.get();
	LSArray<double>::clazz = clazz.get();

	/*
	 * Constructor
	 */
	constructor_({
		{Type::tmp_array(Type::integer), {Type::integer}, (void*) LSArray<int>::constructor},
		{Type::tmp_array(Type::real), {Type::integer}, (void*) LSArray<double>::constructor},
		{Type::tmp_array(Type::any), {Type::integer}, (void*) LSArray<LSValue*>::constructor},
	});

	/*
	 * Operators
	 */
	operator_("in", {
		{Type::const_array(), Type::const_any, Type::boolean, in, THROWS},
	});
	operator_("+", {
		{Type::const_array(), Type::const_any, Type::array(Type::any), op_add},
	});

	auto pqT = Type::template_("T");
	auto pqE = Type::template_("E");
	template_(pqT, pqE).
	operator_("+=", {
		{Type::array(pqT), pqE, Type::array(Type::meta_mul(pqT, Type::meta_not_temporary(pqE))), array_add_eq, 0, {}, true},
	});

	auto ttE = Type::template_("E");
	auto ttR = Type::template_("R");
	template_(ttE, ttR).
	operator_("~~", {
		{Type::const_array(ttE), Type::fun(ttR, {ttE}), Type::tmp_array(ttR), map},
	});

	/*
	 * Methods
	 */
	method("copy", {
		{Type::tmp_array(), {Type::const_array()}, ValueSTD::copy},
		{Type::tmp_array(Type::real), {Type::const_array(Type::real)}, ValueSTD::copy},
		{Type::tmp_array(Type::integer), {Type::const_array(Type::integer)}, ValueSTD::copy},
	});
	method("average", {
		{Type::real, {Type::const_array()}, (void*) &LSArray<LSValue*>::ls_average},
		{Type::real, {Type::const_array(Type::real)}, (void*) &LSArray<double>::ls_average},
		{Type::real, {Type::const_array(Type::integer)}, (void*) &LSArray<int>::ls_average},
	});

	method("chunk", {
		{Type::array(Type::array()), {Type::const_array()}, (void*) chunk_1_ptr},
		{Type::array(Type::array(Type::real)), {Type::const_array(Type::real)}, (void*) chunk_1_float},
		{Type::array(Type::array(Type::integer)), {Type::const_array(Type::integer)}, (void*) chunk_1_int},
		{Type::array(Type::array()), {Type::const_array(), Type::const_integer}, (void*) &LSArray<LSValue*>::ls_chunk},
		{Type::array(Type::array(Type::real)), {Type::const_array(Type::real), Type::const_integer}, (void*) &LSArray<double>::ls_chunk},
		{Type::array(Type::array(Type::integer)), {Type::const_array(Type::integer), Type::const_integer}, (void*) &LSArray<int>::ls_chunk},
    });

	// TODO should return array<never>
	method("clear", {
		{Type::array(), {Type::const_array()}, (void*) &LSArray<LSValue*>::ls_clear},
		{Type::array(Type::real), {Type::const_array(Type::real)}, (void*) &LSArray<double>::ls_clear},
		{Type::array(Type::integer), {Type::const_array(Type::integer)}, (void*) &LSArray<int>::ls_clear},
	});

	method("contains", {
		{Type::boolean, {Type::const_array(Type::any), Type::const_any}, (void*) &LSArray<LSValue*>::ls_contains},
		{Type::boolean, {Type::const_array(Type::real), Type::const_real}, (void*) &LSArray<double>::ls_contains},
		{Type::boolean, {Type::const_array(Type::integer), Type::const_integer}, (void*) &LSArray<int>::ls_contains},
	});

	auto iter_ptr = &LSArray<LSValue*>::ls_iter<LSFunction*>;
	auto iE = Type::template_("E");
	template_(iE).
	method("iter", {
		{Type::void_, {Type::const_array(iE), Type::fun(Type::void_, {iE})}, (void*) iter_ptr},
		{Type::void_, {Type::const_array(iE), Type::fun(Type::void_, {iE})}, iter},
	});

	method("max", {
		{Type::any, {Type::const_array()}, (void*) &LSArray<LSValue*>::ls_max, THROWS},
		{Type::real, {Type::const_array(Type::real)}, (void*) &LSArray<double>::ls_max, THROWS},
		{Type::integer, {Type::const_array(Type::integer)}, (void*) &LSArray<int>::ls_max, THROWS}
	});

	method("min", {
		{Type::any, {Type::const_array()}, (void*) &LSArray<LSValue*>::ls_min, THROWS},
		{Type::real, {Type::const_array(Type::real)}, (void*) &LSArray<double>::ls_min, THROWS},
		{Type::integer, {Type::const_array(Type::integer)}, (void*) &LSArray<int>::ls_min, THROWS}
	});

	auto map_fun = &LSArray<LSValue*>::ls_map<LSFunction*, LSValue*>;
	auto E = Type::template_("E");
	auto R = Type::template_("R");
	template_(E, R).
	method("map", {
		{Type::tmp_array(R), {Type::const_array(E), Type::fun(R, {E})}, (void*) map_fun},
		{Type::tmp_array(R), {Type::const_array(E), Type::fun(R, {E})}, map},
	});

	method("unique", {
		{Type::array(), {Type::array()}, (void*) &LSArray<LSValue*>::ls_unique},
		{Type::array(Type::real), {Type::array(Type::real)}, (void*) &LSArray<double>::ls_unique},
		{Type::array(Type::integer), {Type::array(Type::integer)}, (void*) &LSArray<int>::ls_unique},
	});

	auto sT = Type::template_("T");
	template_(sT).
	method("sort", {
		{Type::tmp_array(), {Type::array()}, (void*) &LSArray<LSValue*>::ls_sort},
		{Type::tmp_array(Type::real), {Type::array(Type::real)}, (void*) &LSArray<double>::ls_sort},
		{Type::tmp_array(Type::integer), {Type::array(Type::integer)}, (void*) &LSArray<int>::ls_sort},
		{Type::tmp_array(sT), {Type::array(sT), Type::fun_object(Type::boolean, {sT, sT})}, sort}
	});

	auto map2_fun_type = (const Type*) Type::fun_object(Type::any, {Type::any, Type::any});
	auto map2_fun_type_int = (const Type*) Type::fun_object(Type::any, {Type::any, Type::integer});
	auto map2_ptr_ptr = &LSArray<LSValue*>::ls_map2<LSFunction*, LSValue*, LSValue*>;
	auto map2_ptr_int = &LSArray<LSValue*>::ls_map2<LSFunction*, LSValue*, int>;
	method("map2", {
		{Type::tmp_array(), {Type::const_array(), Type::const_array(), map2_fun_type}, (void*) map2_ptr_ptr},
		{Type::tmp_array(), {Type::const_array(), Type::const_array(Type::integer), map2_fun_type_int}, (void*) map2_ptr_int},
	});

	auto pred_fun_type = Type::fun(Type::any, {Type::any});
	auto pred_fun_type_float = Type::fun(Type::any, {Type::real});
	auto pred_fun_type_int = Type::fun(Type::any, {Type::integer});
	auto fiT = Type::template_("T");
	template_(fiT).
	method("filter", {
		{Type::tmp_array(fiT), {Type::const_array(fiT), Type::fun(Type::boolean, {fiT})}, filter},
	});

	method("isEmpty", {
		{Type::boolean, {Type::array()}, (void*) &LSArray<LSValue*>::ls_empty},
		{Type::boolean, {Type::array(Type::real)}, (void*) &LSArray<double>::ls_empty},
		{Type::boolean, {Type::array(Type::integer)}, (void*) &LSArray<int>::ls_empty},
	});

	auto perm_int_int = &LSArray<int>::is_permutation<int>;
	auto perm_int_real = &LSArray<int>::is_permutation<double>;
	auto perm_int_ptr = &LSArray<int>::is_permutation<LSValue*>;
	auto perm_real_int = &LSArray<double>::is_permutation<int>;
	auto perm_real_real = &LSArray<double>::is_permutation<double>;
	auto perm_real_ptr = &LSArray<double>::is_permutation<LSValue*>;
	auto perm_ptr_int = &LSArray<LSValue*>::is_permutation<int>;
	auto perm_ptr_real = &LSArray<LSValue*>::is_permutation<double>;
	auto perm_ptr_ptr = &LSArray<LSValue*>::is_permutation<LSValue*>;
	method("isPermutation", {
		{Type::boolean, {Type::array(), Type::array()}, (void*) perm_ptr_ptr},
		{Type::boolean, {Type::array(), Type::array(Type::real)}, (void*) perm_ptr_real},
		{Type::boolean, {Type::array(), Type::array(Type::integer)}, (void*) perm_ptr_int},
		{Type::boolean, {Type::array(Type::real), Type::array(Type::any)}, (void*) perm_real_ptr},
		{Type::boolean, {Type::array(Type::real), Type::array(Type::real)}, (void*) perm_real_real},
		{Type::boolean, {Type::array(Type::real), Type::array(Type::integer)}, (void*) perm_real_int},
		{Type::boolean, {Type::array(Type::integer), Type::array(Type::any)}, (void*) perm_int_ptr},
		{Type::boolean, {Type::array(Type::integer), Type::array(Type::real)}, (void*) perm_int_real},
		{Type::boolean, {Type::array(Type::integer), Type::array(Type::integer)}, (void*) perm_int_int},
	});

	method("partition", {
		{Type::tmp_array(), {Type::array(), pred_fun_type}, partition},
		{Type::tmp_array(), {Type::array(Type::real), pred_fun_type_float}, partition},
		{Type::tmp_array(), {Type::array(Type::integer), pred_fun_type_int}, partition},
	});

	auto fT = Type::template_("T");
	template_(fT).
	method("first", {
		{fT, {Type::const_array(fT)}, first}
	});

	auto lT = Type::template_("T");
	template_(lT).
	method("last", {
		{lT, {Type::const_array(lT)}, last}
	});

	auto flT = Type::template_("T");
	auto flR = Type::template_("R");
	template_(flT, flR).
	method("foldLeft", {
		{flR, {Type::const_array(flT), Type::fun(flR, {flR, flT}), flR}, fold_left},
	});

	auto frT = Type::template_("T");
	auto frR = Type::template_("R");
	template_(frT, frR).
	method("foldRight", {
		{frR, {Type::const_array(frT), Type::fun(frR, {frT, frR}), frR}, fold_right},
	});

	method("pop", {
		{Type::any, {Type::array()}, (void*) &LSArray<LSValue*>::ls_pop, THROWS},
		{Type::real, {Type::array(Type::real)}, (void*) &LSArray<double>::ls_pop, THROWS},
		{Type::integer, {Type::array(Type::integer)}, (void*) &LSArray<int>::ls_pop, THROWS},
	});

	method("product", {
		{Type::real, {Type::array(Type::real)}, (void*) &LSArray<double>::ls_product},
		{Type::integer, {Type::array(Type::integer)}, (void*) &LSArray<int>::ls_product}
	});

	auto pT = Type::template_("T");
	auto pE = Type::template_("E");
	template_(pT, pE).
	method("push", {
		{Type::array(Type::any), {Type::array(), Type::const_any}, (void*) &LSArray<LSValue*>::ls_push, 0, {new WillStoreMutator()}},
		{Type::array(pT), {Type::array(pT), pE}, push, 0, {new WillStoreMutator()}},
	});

	// void (LSArray<int>::*push_int)(int&&) = &LSArray<int>::push_back;
	method("vpush", {
		{Type::void_, {Type::array(Type::integer), Type::integer}, (void*) &LSArray<int>::ls_push},
		{Type::void_, {Type::array(Type::real), Type::real}, (void*) &LSArray<double>::ls_push},
		{Type::void_, {Type::array(Type::any), Type::any}, (void*) &LSArray<LSValue*>::push_inc},
	});

	method("pushAll", {
		{Type::array(Type::any), {Type::array(), Type::array()}, (void*) &LSArray<LSValue*>::ls_push_all_ptr},
		{Type::array(Type::any), {Type::array(), Type::array(Type::real)}, (void*) &LSArray<LSValue*>::ls_push_all_flo},
		{Type::array(Type::any), {Type::array(), Type::array(Type::integer)}, (void*) &LSArray<LSValue*>::ls_push_all_int},
		{Type::array(Type::any), {Type::array(Type::real), Type::array()}, (void*) &LSArray<LSValue*>::ls_push_all_ptr, 0, {new WillStoreMutator()}},
		{Type::array(Type::real), {Type::array(Type::real), Type::array(Type::real)}, (void*) &LSArray<double>::ls_push_all_flo},
		{Type::array(Type::real), {Type::array(Type::real), Type::array(Type::integer)}, (void*) &LSArray<double>::ls_push_all_int},
		{Type::array(Type::any), {Type::array(Type::integer), Type::array()}, (void*) &LSArray<LSValue*>::ls_push_all_ptr, 0, {new WillStoreMutator()}},
		{Type::array(Type::integer), {Type::array(Type::integer), Type::array(Type::real)}, (void*) &LSArray<int>::ls_push_all_flo},
		{Type::array(Type::integer), {Type::array(Type::integer), Type::array(Type::integer)}, (void*) &LSArray<int>::ls_push_all_int},
	});

	method("join", {
		{Type::string, {Type::const_array(), Type::const_string}, (void*) &LSArray<LSValue*>::ls_join},
		{Type::string, {Type::const_array(Type::real), Type::const_string}, (void*) &LSArray<double>::ls_join},
		{Type::string, {Type::const_array(Type::integer), Type::const_string}, (void*) &LSArray<int>::ls_join},
	});

	method("json", {
		{Type::string, {Type::array()}, (void*) &LSValue::ls_json},
	});

	auto T = Type::template_("T");
	template_(T).
	method("fill", {
		{Type::array(T), {Type::array(), T}, fill, 0, {new WillStoreMutator()}},
		{Type::array(T), {Type::array(), T, Type::const_integer}, fill2, 0, {new WillStoreMutator()}},
	});

	method("insert", {
		{Type::array(Type::any), {Type::array(), Type::any, Type::integer}, (void*) &LSArray<LSValue*>::ls_insert},
		{Type::array(Type::real), {Type::array(Type::real), Type::real, Type::integer}, (void*) &LSArray<double>::ls_insert},
		{Type::array(Type::integer), {Type::array(Type::integer), Type::integer, Type::integer}, (void*) &LSArray<int>::ls_insert},
	});

	method("random", {
		{Type::tmp_array(), {Type::const_array(), Type::const_integer}, (void*) &LSArray<LSValue*>::ls_random},
		{Type::tmp_array(Type::real), {Type::const_array(Type::real), Type::const_integer}, (void*) &LSArray<double>::ls_random},
		{Type::tmp_array(Type::integer), {Type::const_array(Type::integer), Type::const_integer}, (void*) &LSArray<int>::ls_random},
	});

	method("remove", {
		{Type::any, {Type::array(), Type::integer}, (void*)&LSArray<LSValue*>::ls_remove},
		{Type::real, {Type::array(Type::real), Type::integer}, (void*)&LSArray<double>::ls_remove},
		{Type::integer, {Type::array(Type::integer), Type::integer}, (void*)&LSArray<int>::ls_remove},
	});

	method("removeElement", {
		{Type::boolean, {Type::array(), Type::const_any}, remove_element_any},
		{Type::boolean, {Type::array(Type::real), Type::const_any}, remove_element_real},
		{Type::boolean, {Type::array(Type::real), Type::const_real}, remove_element_real},
		{Type::boolean, {Type::array(Type::integer), Type::const_any}, remove_element_int},
		{Type::boolean, {Type::array(Type::integer), Type::integer}, remove_element_int},
	});

	method("reverse", {
		{Type::tmp_array(), {Type::const_array()}, (void*) &LSArray<LSValue*>::ls_reverse},
		{Type::tmp_array(Type::real), {Type::const_array(Type::real)}, (void*) &LSArray<double>::ls_reverse},
		{Type::tmp_array(Type::integer), {Type::const_array(Type::integer)}, (void*) &LSArray<int>::ls_reverse},
	});

	method("shuffle", {
		{Type::array(Type::any), {Type::array()}, (void*) &LSArray<LSValue*>::ls_shuffle},
		{Type::array(Type::real), {Type::array(Type::real)}, (void*) &LSArray<double>::ls_shuffle},
		{Type::array(Type::integer), {Type::array(Type::integer)}, (void*) &LSArray<int>::ls_shuffle},
	});

	method("search", {
		{Type::integer, {Type::const_array(), Type::const_any, Type::const_integer}, (void*) &LSArray<LSValue*>::ls_search},
		{Type::integer, {Type::const_array(Type::real), Type::const_real, Type::const_integer}, (void*) &LSArray<double>::ls_search},
		{Type::integer, {Type::const_array(Type::integer), Type::const_integer, Type::const_integer}, (void*) &LSArray<int>::ls_search},
	});

	method("size", {
		{Type::any, {Type::const_any}, (void*) &LSArray<LSValue*>::ls_size_ptr},
		{Type::integer, {Type::const_any}, size},
		{Type::integer, {Type::const_array(Type::real)}, size},
		{Type::integer, {Type::const_array(Type::integer)}, size}
	});

	method("sum", {
		{Type::any, {Type::const_array()}, (void*) &LSArray<LSValue*>::ls_sum},
		{Type::real, {Type::const_array(Type::real)}, (void*) &LSArray<double>::ls_sum},
		{Type::integer, {Type::const_array(Type::integer)}, (void*) &LSArray<int>::ls_sum},
	});

	method("subArray", {
		{Type::tmp_array(), {Type::const_array(), Type::const_integer, Type::const_integer}, (void*) &sub},
		{Type::tmp_array(Type::real), {Type::const_array(Type::real), Type::const_integer, Type::const_integer}, (void*) &sub},
		{Type::tmp_array(Type::integer), {Type::const_array(Type::integer), Type::const_integer, Type::const_integer}, (void*) &sub},
	});

	/** V1 **/
	method("count", {
		{Type::integer, {Type::any}, (void*) &LSArray<LSValue*>::ls_size}
	});
	method("inArray", {
		{Type::boolean, {Type::array(), Type::any}, (void*) &LSArray<LSValue*>::ls_contains},
		{Type::boolean, {Type::array(Type::real), Type::real}, (void*) &LSArray<double>::ls_contains},
		{Type::boolean, {Type::array(Type::integer), Type::integer}, (void*) &LSArray<int>::ls_contains}
	});

	/** Internal **/
	method("convert_key", {
		{Type::integer, {Type::const_any, Type::const_any}, (void*) &convert_key}
	});
	method("in", {
		{Type::boolean, {Type::const_array(Type::any), Type::const_any}, (void*) &LSArray<LSValue*>::in},
		{Type::boolean, {Type::const_array(Type::real), Type::const_any}, (void*) &LSArray<double>::in},
		{Type::boolean, {Type::const_array(Type::integer), Type::integer}, (void*) &LSArray<int>::in_i},
	});
	method("isize", {
		{Type::integer, {Type::array(Type::any)}, (void*) &LSArray<LSValue*>::int_size},
		{Type::integer, {Type::array(Type::real)}, (void*) &LSArray<double>::int_size},
		{Type::integer, {Type::array(Type::integer)}, (void*) &LSArray<int>::int_size},
	});
	method("to_bool", {
		{Type::boolean, {Type::array()}, (void*) &LSArray<int>::to_bool}
	});
	auto sort_fun_int = &LSArray<int>::ls_sort_fun<LSFunction*>;
	auto sort_fun_real = &LSArray<double>::ls_sort_fun<LSFunction*>;
	auto sort_fun_any = &LSArray<LSValue*>::ls_sort_fun<LSFunction*>;
	method("sort_fun", {
		{Type::array(), {Type::array(), (const Type*) Type::fun_object(Type::void_, {})}, (void*) sort_fun_any},
		{Type::array(), {Type::array(), (const Type*) Type::fun_object(Type::void_, {})}, (void*) sort_fun_real},
		{Type::array(), {Type::array(), (const Type*) Type::fun_object(Type::void_, {})}, (void*) sort_fun_int},
	});

	method("fill_fun", {
		{Type::array(), {Type::array(), Type::any, Type::integer}, (void*) &LSArray<LSValue*>::ls_fill},
		{Type::array(), {Type::array(), Type::real, Type::integer}, (void*) &LSArray<double>::ls_fill},
		{Type::array(), {Type::array(), Type::integer, Type::integer}, (void*) &LSArray<int>::ls_fill},
	});
	method("remove_element_fun", {
		{Type::boolean, {Type::array(), Type::any}, (void*) &LSArray<LSValue*>::ls_remove_element},
		{Type::boolean, {Type::array(), Type::any}, (void*) &LSArray<double>::ls_remove_element},
		{Type::boolean, {Type::array(), Type::any}, (void*) &LSArray<int>::ls_remove_element},
	});
	method("int_to_any", {
		{Type::array(Type::any), {Type::array(Type::integer)}, (void*) &LSArray<int>::to_any_array}
	});
	method("real_to_any", {
		{Type::array(Type::any), {Type::array(Type::real)}, (void*) &LSArray<double>::to_any_array}
	});
	method("int_to_real", {
		{Type::array(Type::any), {Type::array(Type::real)}, (void*) &LSArray<int>::to_real_array}
	});
}

Compiler::value ArraySTD::in(Compiler& c, std::vector<Compiler::value> args, bool) {
	const auto& type = args[0].t->element()->fold();
	auto f = [&]() {
		if (type->is_integer()) return "Array.in.2";
		if (type->is_real()) return "Array.in.1";
		return "Array.in";
	}();
	if (args[1].t->castable(type)) {
		auto v = c.insn_convert(args[1], type);
		auto r = c.insn_call(Type::boolean, {args[0], v}, f);
		if (args[1].t->is_polymorphic() and type->is_primitive()) {
			c.insn_delete_temporary(args[1]);
		}
		return r;
	} else {
		c.insn_delete_temporary(args[0]);
		c.insn_delete_temporary(args[1]);
		return c.new_bool(false);
	}
}

Compiler::value ArraySTD::op_add(Compiler& c, std::vector<Compiler::value> args, bool) {
	return c.insn_call(Type::array(args[0].t->element()), {args[0], c.insn_to_any(args[1])}, "Value.operator+");
}

Compiler::value ArraySTD::array_add_eq(Compiler& c, std::vector<Compiler::value> args, bool) {
	args[1] = c.insn_to_any(args[1]);
	return c.insn_call(Type::any, args, "Value.operator+=");
}

Compiler::value ArraySTD::size(Compiler& c, std::vector<Compiler::value> args, bool) {
	auto res = c.insn_array_size(args[0]);
	c.insn_delete_temporary(args[0]);
	return res;
}

LSArray<LSValue*>* ArraySTD::chunk_1_ptr(LSArray<LSValue*>* array) {
	return array->ls_chunk(1);
}

LSArray<LSValue*>* ArraySTD::chunk_1_int(LSArray<int>* array) {
	return array->ls_chunk(1);
}

LSArray<LSValue*>* ArraySTD::chunk_1_float(LSArray<double>* array) {
	return array->ls_chunk(1);
}

LSValue* ArraySTD::sub(LSArray<LSValue*>* array, int begin, int end) {
	LSValue* r = array->range(begin, end);
	if (array->refs == 0) delete array;
	return r;
}

Compiler::value ArraySTD::fill(Compiler& c, std::vector<Compiler::value> args, bool) {
	auto fun = [&]() {
		if (args[0].t->element()->fold()->is_integer()) return "Array.fill_fun.2";
		if (args[0].t->element()->fold()->is_real()) return "Array.fill_fun.1";
		return "Array.fill_fun";
	}();
	return c.insn_call(args[0].t, {args[0], c.insn_convert(args[1], args[0].t->element()->fold()), c.insn_array_size(args[0])}, fun);
}
Compiler::value ArraySTD::fill2(Compiler& c, std::vector<Compiler::value> args, bool) {
	auto fun = [&]() {
		if (args[0].t->element()->fold()->is_integer()) return "Array.fill_fun.2";
		if (args[0].t->element()->fold()->is_real()) return "Array.fill_fun.1";
		return "Array.fill_fun";
	}();
	return c.insn_call(args[0].t, {args[0], c.insn_convert(args[1], args[0].t->element()->fold()), c.to_int(args[2])}, fun);
}

Compiler::value ArraySTD::fold_left(Compiler& c, std::vector<Compiler::value> args, bool) {
	auto array = args[0];
	auto function = args[1];
	auto result = Variable::new_temporary("r", args[2].t);
	result->create_entry(c);
	c.add_temporary_variable(result);
	c.insn_store(result->val, c.insn_move_inc(args[2]));
	auto v = Variable::new_temporary("v", args[0].t->element());
	v->create_entry(c);
	c.add_temporary_variable(v);
	c.insn_foreach(array, Type::void_, v, nullptr, [&](Compiler::value v, Compiler::value k) -> Compiler::value {
		auto r = c.insn_call(function, {c.insn_load(result->val), v});
		c.insn_delete(c.insn_load(result->val));
		c.insn_store(result->val, c.insn_move_inc(r));
		return {};
	});
	return c.insn_load(result->val);
}

Compiler::value ArraySTD::fold_right(Compiler& c, std::vector<Compiler::value> args, bool) {
	auto function = args[1];
	auto result = Variable::new_temporary("r", args[2].t);
	result->create_entry(c);
	c.add_temporary_variable(result);
	c.insn_store(result->val, c.insn_move(args[2]));
	auto v = Variable::new_temporary("v", args[0].t->element());
	v->create_entry(c);
	c.insn_foreach(args[0], Type::void_, v, nullptr, [&](Compiler::value v, Compiler::value k) -> Compiler::value {
		c.insn_store(result->val, c.insn_call(function, {v, c.insn_load(result->val)}));
		return {};
	}, true);
	return c.insn_load(result->val);
}

Compiler::value ArraySTD::iter(Compiler& c, std::vector<Compiler::value> args, bool) {
	auto function = args[1];
	auto v = Variable::new_temporary("v", args[0].t->element());
	v->create_entry(c);
	c.add_temporary_variable(v);
	c.insn_foreach(args[0], Type::void_, v, nullptr, [&](Compiler::value v, Compiler::value k) -> Compiler::value {
		return c.insn_call(function, {v});
	});
	return {};
}

Compiler::value ArraySTD::remove_element_any(Compiler& c, std::vector<Compiler::value> args, bool) {
	return c.insn_call(Type::boolean, {args[0], c.insn_to_any(args[1])}, "Array.remove_element_fun");
}
Compiler::value ArraySTD::remove_element_real(Compiler& c, std::vector<Compiler::value> args, bool) {
	if (args[1].t->castable(Type::real)) {
		return c.insn_call(Type::boolean, {args[0], c.to_real(args[1])}, "Array.remove_element_fun.1");
	} else {
		c.insn_delete_temporary(args[0]);
		c.insn_delete_temporary(args[1]);
		return c.new_bool(false);
	}
}
Compiler::value ArraySTD::remove_element_int(Compiler& c, std::vector<Compiler::value> args, bool) {
	if (args[1].t->castable(Type::integer)) {
		return c.insn_call(Type::boolean, {args[0], c.to_int(args[1])}, "Array.remove_element_fun.2");
	} else {
		c.insn_delete_temporary(args[0]);
		c.insn_delete_temporary(args[1]);
		return c.new_bool(false);
	}
}

Compiler::value ArraySTD::partition(Compiler& c, std::vector<Compiler::value> args, bool) {
	auto array = args[0];
	auto function = args[1];
	auto array_true = c.new_array(array.t->element(), {});
	auto array_false = c.new_array(array.t->element(), {});
	auto v = Variable::new_temporary("v", array.t->element());
	v->create_entry(c);
	c.insn_foreach(array, Type::void_, v, nullptr, [&](Compiler::value v, Compiler::value k) -> Compiler::value {
		auto r = c.insn_call(function, {v});
		c.insn_if(r, [&]() {
			c.insn_push_array(array_true, v);
		}, [&]() {
			c.insn_push_array(array_false, v);
		});
		c.insn_delete_temporary(r);
		return {};
	});
	return c.new_array(Type::array(array.t->element()), {array_true, array_false});
}

Compiler::value ArraySTD::map(Compiler& c, std::vector<Compiler::value> args, bool no_return) {
	auto array = args[0];
	auto function = args[1];
	auto result = no_return ? Compiler::value() : c.new_array(function.t->return_type(), {});
	auto v = Variable::new_temporary("v", array.t->element());
	v->create_entry(c);
	c.insn_foreach(array, Type::void_, v, nullptr, [&](Compiler::value v, Compiler::value k) -> Compiler::value {
		auto x = c.clone(v);
		c.insn_inc_refs(x);
		auto r = c.insn_call(function, {x});
		if (no_return) {
			if (not r.t->is_void()) c.insn_delete_temporary(r);
		} else {
			c.insn_push_array(result, r.t->is_void() ? c.new_null() : r);
		}
		c.insn_delete(x);
		return {};
	});
	return no_return ? Compiler::value() : result;
}


Compiler::value ArraySTD::first(Compiler& c, std::vector<Compiler::value> args, bool) {
	auto array = args[0];
	auto array_size = c.insn_array_size(array);
	c.insn_if(c.insn_ge(c.new_integer(0), array_size), [&]() {
		c.insn_delete_temporary(array);
		c.insn_throw_object(vm::Exception::ARRAY_OUT_OF_BOUNDS);
	});
	auto e = c.insn_move(c.insn_load(c.insn_array_at(array, c.new_integer(0))));
	c.insn_delete_temporary(array);
	return e;
}
Compiler::value ArraySTD::last(Compiler& c, std::vector<Compiler::value> args, bool) {
	auto array = args[0];
	auto array_size = c.insn_array_size(array);
	c.insn_if(c.insn_ge(c.new_integer(0), array_size), [&]() {
		c.insn_delete_temporary(array);
		c.insn_throw_object(vm::Exception::ARRAY_OUT_OF_BOUNDS);
	});
	auto k = c.insn_sub(array_size, c.new_integer(1));
	auto e = c.insn_move(c.insn_load(c.insn_array_at(array, k)));
	c.insn_delete_temporary(array);
	return e;
}

Compiler::value ArraySTD::sort(Compiler& c, std::vector<Compiler::value> args, bool) {
	const auto& array = args[0];
	const auto& fun = args[1];
	auto f = [&]() {
		if (args[0].t->element()->fold()->is_integer()) {
			return "Array.sort_fun.2";
		} else if (args[0].t->element()->fold()->is_real()) {
			return "Array.sort_fun.1";
		} else {
			return "Array.sort_fun";
		}
	}();
	return c.insn_call(array.t, {array, fun}, f);
}

Compiler::value ArraySTD::push(Compiler& c, std::vector<Compiler::value> args, bool no_return) {
	auto fun = [&]() {
		if (args[0].t->element()->fold()->is_integer()) return "Array.vpush";
		if (args[0].t->element()->fold()->is_real()) return "Array.vpush.1";
		args[1] = c.insn_convert(args[1], Type::any);
		return "Array.vpush.2";
	}();
	c.insn_call(Type::void_, args, fun);
	return args[0];
}

Compiler::value ArraySTD::filter(Compiler& c, std::vector<Compiler::value> args, bool) {
	auto function = args[1];
	auto result = c.new_array(args[0].t->element(), {});
	auto v = Variable::new_temporary("v", args[0].t->element());
	v->create_entry(c);
	c.insn_foreach(args[0], Type::void_, v, nullptr, [&](Compiler::value v, Compiler::value k) -> Compiler::value {
		auto r = c.insn_call(function, {v});
		c.insn_if(r, [&]() {
			c.insn_push_array(result, c.clone(v));
		});
		return {};
	});
	return result;
}

int ArraySTD::convert_key(LSValue* array, LSValue* key_pointer) {
	auto n = dynamic_cast<LSNumber*>(key_pointer);
	if (!n) {
		LSValue::delete_temporary(array);
		LSValue::delete_temporary(key_pointer);
		throw vm::ExceptionObj(vm::Exception::ARRAY_KEY_IS_NOT_NUMBER);
	}
	int key_int = n->value;
	LSValue::delete_temporary(key_pointer);
	return key_int;
}

}
