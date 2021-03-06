#ifndef ENVIRONMENT_HPP
#define ENVIRONMENT_HPP

#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include "../type/Function_type.hpp"
#include "../standard/StandardLibrary.hpp"
#include "../analyzer/semantic/Completion.hpp"
#if COMPILER
#include "../compiler/Compiler.hpp"
#include "../vm/VM.hpp"
#endif

namespace ls {

class Type;
class OutputStream;
class Program;

class Environment {
	friend Type;
private:
	#if COMPILER
	Compiler compiler;
	VM vm;
	#endif

	std::vector<std::unique_ptr<const Type>> placeholder_types;
	std::map<std::set<const Type*>, std::unique_ptr<const Type>> compound_types;
	std::map<const Type*, std::unique_ptr<const Type>> tmp_compound_types;
	std::vector<std::unique_ptr<const Type>> raw_function_types;
	std::map<std::pair<const Type*, std::vector<const Type*>>, std::unique_ptr<const Type>> function_types;
	std::map<std::pair<const Type*, std::vector<const Type*>>, std::unique_ptr<const Type>> function_object_types;
	std::map<std::pair<const Type*, std::vector<const Type*>>, std::unique_ptr<const Type>> closure_types;
	std::unordered_map<const Type*, std::unique_ptr<const Type>> array_types;
	std::unordered_map<const Type*, const Type*> const_array_types;
	std::unordered_map<const Type*, const Type*> tmp_array_types;
	std::map<std::vector<const Type*>, std::unique_ptr<const Type>> fixed_array_types;
	std::map<std::vector<const Type*>, const Type*> tmp_fixed_array_types;
	std::unordered_map<const Type*, std::unique_ptr<const Type>> set_types;
	std::unordered_map<const Type*, const Type*> const_set_types;
	std::unordered_map<const Type*, const Type*> tmp_set_types;
	std::map<std::pair<const Type*, const Type*>, std::unique_ptr<const Type>> map_types;
	std::map<std::pair<const Type*, const Type*>, const Type*> const_map_types;
	std::map<std::pair<const Type*, const Type*>, const Type*> tmp_map_types;
	std::unordered_map<const Type*, const Type*> pointer_types;
	std::unordered_map<const Type*, std::unique_ptr<const Type>> base_pointer_types;
	std::unordered_map<const Type*, std::unique_ptr<const Type>> temporary_types;
	std::unordered_map<const Type*, const Type*> not_temporary_types;
	std::unordered_map<const Type*, std::unique_ptr<const Type>> const_types;
	std::unordered_map<const Type*, const Type*> not_const_types;
	std::unordered_map<std::string, std::unique_ptr<const Type>> class_types;
	std::unordered_map<std::string, std::unique_ptr<const Type>> structure_types;
	std::map<std::pair<const Type*, const Type*>, std::unique_ptr<const Type>> meta_add_types;
	std::map<std::pair<const Type*, const Type*>, std::unique_ptr<const Type>> meta_concat_types;
	std::map<std::pair<const Type*, const Type*>, std::unique_ptr<const Type>> meta_mul_types;
	std::map<std::pair<const Type*, const Type*>, std::unique_ptr<const Type>> meta_base_of_types;
	std::unordered_map<const Type*, std::unique_ptr<const Type>> meta_temporary_types;
	std::unordered_map<const Type*, std::unique_ptr<const Type>> meta_element_types;
	std::unordered_map<const Type*, std::unique_ptr<const Type>> meta_not_temporary_types;
	std::unordered_map<const Type*, std::unique_ptr<const Type>> meta_not_void_types;
	std::vector<std::unique_ptr<const Type>> template_types;

public:
	bool legacy = false;
	OutputStream* output = nullptr;
	int operation_limit = -1;

    const Type* const void_;
	const Type* const boolean;
	const Type* const const_boolean;
	const Type* const number;
	const Type* const const_number;
	const Type* const i8;
	const Type* const i8_ptr;
	const Type* const integer;
	const Type* const const_integer;
	const Type* const long_;
	const Type* const const_long;
	const Type* const mpz;
	const Type* const tmp_mpz;
	const Type* const const_mpz;
	const Type* const mpz_ptr;
	const Type* const tmp_mpz_ptr;
	const Type* const const_mpz_ptr;

	const Type* const never;
	const Type* const any;
	const Type* const tmp_any;
	const Type* const const_any;
	const Type* const null;
	const Type* const string;
	const Type* const tmp_string;
	const Type* const const_string;
	const Type* const real;
	const Type* const const_real;
	const Type* const interval;
	const Type* const const_interval;
	const Type* const tmp_interval;
	const Type* const object;
	const Type* const tmp_object;
	const Type* const array;
	const Type* const set;

	const TypeMutator* const change_value_mutator;
	const TypeMutator* const convert_mutator;
	const TypeMutator* const convert_mutator_array_size;

	#if COMPILER
	std::unique_ptr<LSClass> value_class;
	std::unique_ptr<LSClass> null_class;
	std::unique_ptr<LSClass> boolean_class;
	std::unique_ptr<LSClass> number_class;
	std::unique_ptr<LSClass> string_class;
	std::unique_ptr<LSClass> set_class;
	std::unique_ptr<LSClass> array_class;
	std::unique_ptr<LSClass> map_class;
	std::unique_ptr<LSClass> legacy_array_class;
	std::unique_ptr<LSClass> interval_class;
	std::unique_ptr<LSClass> object_class;
	std::unique_ptr<LSClass> function_class;
	std::unique_ptr<LSClass> class_class;
	llvm::Type* mpz_type;
	#endif

	StandardLibrary std;

	Environment();
    Environment(bool legacy);
	virtual ~Environment();

	/**
	 * Analyze a `Program`.
	 */
	void analyze(Program& program, bool format = false, bool debug = false, bool sections = false);

	/**
	 * Autocomplete a `Program` at a position
	 */
	Completion autocomplete(Program& program, size_t position);

	/**
	 * Hover a `Program` at a position in a certain file
	 */
	Hover hover(Program& program, File* file, size_t position);

	#if COMPILER
	/**
	 * Compile a `Program`.
	 */
	void compile(Program& program, bool format = false, bool debug = false, bool ops = false, bool assembly = false, bool pseudo_code = false, bool optimized_ir = false, bool execute_ir = false, bool execute_bitcode = false);

	/**
	 * Execute a `Program`.
	 */
	void execute(Program& program, bool format = false, bool debug = false, bool ops = true, bool assembly = false, bool pseudo_code = false, bool optimized_ir = false, bool execute_ir = false, bool execute_bitcode = false);
	#endif

	const Type* template_(std::string name);
	const Type* clazz(const std::string name = "?");
	const Type* const_class(const std::string name = "?");
	const Type* generate_new_placeholder_type();
    void clear_placeholder_types();
};

}

#endif