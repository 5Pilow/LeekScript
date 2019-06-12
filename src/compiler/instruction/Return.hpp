#ifndef RETURN_HPP
#define RETURN_HPP

#include "Instruction.hpp"

namespace ls {

class Return : public Instruction {
public:

	Value* expression;

	Return(Value* = nullptr);
	virtual ~Return();

	virtual void print(std::ostream&, int indent, bool debug, bool condensed) const override;
	virtual Location location() const override;

	virtual void pre_analyze(SemanticAnalyzer* analyzer) override;
	virtual void analyze(SemanticAnalyzer*, const Type* req_type) override;
	
	virtual Compiler::value compile(Compiler&) const override;

	virtual Instruction* clone() const override;
};

}

#endif
