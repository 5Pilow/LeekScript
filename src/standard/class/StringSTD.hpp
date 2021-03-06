#ifndef VM_STANDARD_STRINGSTD_HPP_
#define VM_STANDARD_STRINGSTD_HPP_

#include "../Module.hpp"

namespace ls {

class LSString;
class LSNumber;

class StringSTD : public Module {
public:
	StringSTD(Environment& env);
	virtual ~StringSTD();

	#if COMPILER

	static Compiler::value new_string(Compiler& c, std::vector<Compiler::value> args, int);

	static Compiler::value lt(Compiler& c, std::vector<Compiler::value> args, int);
	static Compiler::value div(Compiler& c, std::vector<Compiler::value> args, int);

	static LSString* add_int(LSString* s, int i);
	static LSString* add_int_r(int i, LSString* s);
	static LSString* add_long(LSString* s, long l);
	static LSString* add_bool(LSString* s, bool i);
	static LSString* add_real(LSString* s, double i);
	static Compiler::value add_eq(Compiler& c, std::vector<Compiler::value> args, int);

	static LSString* replace(LSString*, LSString*, LSString*);
	static LSValue* v1_replace(LSString* string, LSString* from, LSString* to);

	static Compiler::value fold_fun(Compiler& c, std::vector<Compiler::value> args, int);

	static LSValue* string_right(LSString* string, int pos);
	static LSValue* string_left(LSString* string, int pos);
	static LSValue* string_right_tmp(LSString* string, int pos);
	static LSValue* string_left_tmp(LSString* string, int pos);

	static Compiler::value plus_mpz_tmp(Compiler& c, std::vector<Compiler::value> args, int);

	static LSMap<int, int>* frequencies(LSString* string);
	static LSValue* chunk(LSString* string, int size);
	static int count(LSString* string, LSString* element);

	#endif
};

}

#endif
