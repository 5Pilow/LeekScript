#ifndef CLASSDECLARATION_HPP
#define CLASSDECLARATION_HPP

#include <vector>
#include "Instruction.hpp"
#include "VariableDeclaration.hpp"
#include "../lexical/Ident.hpp"

namespace ls {

class LSClass;

class ClassDeclaration : public Instruction {
public:

	Token* token;
	std::string name;
	std::vector<std::unique_ptr<VariableDeclaration>> fields;
	Variable* var;

	ClassDeclaration(Environment& env, Token* token);

	virtual void print(std::ostream&, int indent, PrintOptions options) const override;
	virtual Location location() const override;

	virtual void pre_analyze(SemanticAnalyzer* analyzer) override;
	virtual void analyze(SemanticAnalyzer*, const Type* req_type) override;

	#if COMPILER
	virtual Compiler::value compile(Compiler&) const override;
	#endif

	virtual std::unique_ptr<Instruction> clone(Block* parent) const override;
};

}

#endif
