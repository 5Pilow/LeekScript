#include "If.hpp"
#include "../semantic/SemanticAnalyser.hpp"
#include "Number.hpp"
#include "../instruction/Return.hpp"
#include "../instruction/Break.hpp"
#include "../instruction/Continue.hpp"
#include "../../vm/LSValue.hpp"
#include "../../vm/value/LSNull.hpp"

using namespace std;

namespace ls {

If::If(bool ternary) {
	elze = nullptr;
	condition = nullptr;
	then = nullptr;
	type = {};
	this->ternary = ternary;
}

If::~If() {
	delete condition;
	delete then;
	if (elze != nullptr) {
		delete elze;
	}
}

void If::print(ostream& os, int indent, bool debug, bool condensed) const {
	if (ternary) {
		os << "(";
		condition->print(os, indent, debug);
		os << " ? ";
		then->instructions[0]->print(os, indent, debug);
		os << " : ";
		elze->instructions[0]->print(os, indent, debug);
		os << ")";
	} else {
		os << "if ";
		condition->print(os, indent + 1, debug);
		os << " ";
		then->print(os, indent, debug);
		if (elze != nullptr) {
			os << " else ";
			elze->print(os, indent, debug);
		}
	}
	if (debug) {
		os << " " << type;
	}
}

Location If::location() const {
	return condition->location(); // TODO better
}

void If::analyse(SemanticAnalyser* analyser) {

	condition->analyse(analyser);
	then->analyse(analyser);

	type = then->type;

	if (elze != nullptr) {
		elze->analyse(analyser);
		type += elze->type;
	}
	returning = then->returning and (elze != nullptr and elze->returning);
	may_return = then->may_return or (elze != nullptr and elze->may_return);
}

Compiler::value If::compile(Compiler& c) const {

	auto label_then = c.insn_init_label("then");
	auto label_else = c.insn_init_label("else");
	auto label_end = c.insn_init_label("end");
	Compiler::value then_v;
	Compiler::value else_v;

	auto cond = condition->compile(c);
	condition->compile_end(c);
	auto cond_boolean = c.insn_to_bool(cond);
	c.insn_delete_temporary(cond);
	c.insn_if_new(cond_boolean, &label_then, &label_else);

	c.insn_label(&label_then);

	then_v = c.insn_convert(then->compile(c), type.fold());
	then->compile_end(c);

	if (then->instructions.size() && dynamic_cast<Return*>(then->instructions[0]) == nullptr && dynamic_cast<Break*>(then->instructions[0]) == nullptr && dynamic_cast<Continue*>(then->instructions[0]) == nullptr) {
		c.insn_branch(&label_end);
	}
	label_then.block = Compiler::builder.GetInsertBlock();

	c.insn_label(&label_else);

	if (elze != nullptr) {
		else_v = c.insn_convert(elze->compile(c), type.fold());
		elze->compile_end(c);
	}

	c.insn_branch(&label_end);
	label_else.block = Compiler::builder.GetInsertBlock();

	c.insn_label(&label_end);
	
	if (type.is_void()) {
		return {};
	} else {
		return c.insn_phi(type, then_v, label_then, else_v, label_else);
	}
}

Value* If::clone() const {
	auto iff = new If();
	iff->condition = condition->clone();
	iff->then = (Block*) then->clone();
	iff->elze = elze ? (Block*) elze->clone() : nullptr;
	iff->ternary = ternary;
	return iff;
}

}
