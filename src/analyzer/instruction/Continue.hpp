#ifndef CONTINUE_HPP
#define CONTINUE_HPP

#include "Instruction.hpp"

namespace ls {

class Continue : public Instruction {
public:

	int deepness;
	Token* token = nullptr;

	Continue(Environment& env);

	virtual void print(std::ostream&, int indent, PrintOptions options) const override;
	virtual Location location() const override;

	virtual void analyze(SemanticAnalyzer*, const Type* req_type) override;

	#if COMPILER
	virtual Compiler::value compile(Compiler&) const override;
	#endif

	virtual std::unique_ptr<Instruction> clone(Block* parent) const override;
};

}

#endif
