#ifndef EXPRESSION_HPP
#define EXPRESSION_HPP

#include <vector>
#include "../../compiler/lexical/Operator.hpp"
#include "../../compiler/value/Value.hpp"

namespace ls {

class Callable;
class CallableVersion;

class Expression : public Value {
public:

	Value* v1;
	Value* v2;
	std::shared_ptr<Operator> op;
	int operations;
	const CallableVersion* callable_version = nullptr;

	Expression();
	Expression(Value*);
	virtual ~Expression();

	void append(std::shared_ptr<Operator>, Value*);

	void print(std::ostream&, int indent, bool debug, bool condensed) const override;
	virtual Location location() const override;

	virtual void pre_analyze(SemanticAnalyzer*) override;
	virtual void analyze(SemanticAnalyzer*) override;

	virtual Compiler::value compile(Compiler&) const override;

	virtual Value* clone() const override;
};

}

#endif
