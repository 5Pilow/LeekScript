#include "ArrayFor.hpp"
#include "../semantic/SemanticAnalyzer.hpp"

namespace ls {

ArrayFor::ArrayFor(Environment& env) : Value(env) {
	jumping = true;
}

void ArrayFor::print(std::ostream& os, int indent, PrintOptions options) const {
	os << "[";
	forr->print(os, indent, options);
	os << "]";

	if (options.debug) {
		os << " " << type;
	}
}

Location ArrayFor::location() const {
	return {nullptr, {0, 0, 0}, {0, 0, 0}}; // TODO
}

Hover ArrayFor::hover(SemanticAnalyzer& analyzer, size_t position) const {
	if (forr->location().contains(position)) {
		return forr->hover(analyzer, position);
	}
	return { type, location() };
}

void ArrayFor::pre_analyze(SemanticAnalyzer* analyzer) {
	forr->pre_analyze(analyzer);
}

void ArrayFor::analyze(SemanticAnalyzer* analyzer) {
	auto& env = analyzer->env;
	forr->analyze(analyzer, Type::array(env.void_));
	type = forr->type;
	return_type = forr->return_type;
	throws = forr->throws;
}

#if COMPILER
Compiler::value ArrayFor::compile(Compiler& c) const {
	return forr->compile(c);
}
#endif

std::unique_ptr<Value> ArrayFor::clone(Block* parent) const {
	auto af = std::make_unique<ArrayFor>(type->env);
	af->forr = forr->clone(parent);
	af->end_section = af->forr->end_section;
	return af;
}

}
