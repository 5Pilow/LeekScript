#ifndef VARIABLEVALUE_HPP
#define VARIABLEVALUE_HPP

#include <memory>
#include "LeftValue.hpp"
#include "../semantic/SemanticAnalyzer.hpp"
#if COMPILER
#include "../../compiler/Compiler.hpp"
#endif

namespace ls {

class Value;
class Variable;
enum class VarScope;

class VariableValue : public LeftValue {
public:

	std::string name;
	Token* token;
	Variable* var;
	int capture_index = 0;
	VarScope scope;
	bool class_method = false;
	bool update_variable = false;
	const Type* previous_type = nullptr;
	bool static_field = false;
	#if COMPILER
	std::function<Compiler::value(Compiler&)> static_access_function = nullptr;
	#endif

	VariableValue(Environment& env, Token* token);

	virtual bool isLeftValue() const override;

	virtual void print(std::ostream&, int indent, PrintOptions options) const override;
	virtual Location location() const override;

	virtual void pre_analyze(SemanticAnalyzer*) override;
	virtual Call get_callable(SemanticAnalyzer*, int argument_count) const override;
	virtual void analyze(SemanticAnalyzer*) override;
	virtual const Type* will_take(SemanticAnalyzer* analyzer, const std::vector<const Type*>&, int level) override;
	void set_version(SemanticAnalyzer*, const std::vector<const Type*>& args, int level) override;
	virtual bool will_store(SemanticAnalyzer* analyzer, const Type* type) override;
	virtual bool elements_will_store(SemanticAnalyzer* analyzer, const Type* type, int level) override;
	virtual void change_value(SemanticAnalyzer*, Value* value) override;
	virtual const Type* version_type(std::vector<const Type*>) const override;
	virtual Completion autocomplete(SemanticAnalyzer& analyzer, size_t position) const override;
	virtual Hover hover(SemanticAnalyzer& analyzer, size_t position) const override;

	#if COMPILER
	virtual Compiler::value compile(Compiler&) const override;
	virtual Compiler::value compile_version(Compiler&, std::vector<const Type*> version) const override;
	virtual Compiler::value compile_l(Compiler&) const override;
	#endif

	virtual std::unique_ptr<Value> clone(Block* parent) const override;
};

}

#endif
