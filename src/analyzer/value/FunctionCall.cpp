#include "FunctionCall.hpp"
#include <sstream>
#include <string>
#include "../../standard/Module.hpp"
#include "../Program.hpp"
#include "../../type/Type.hpp"
#include "../lexical/Token.hpp"
#include "../semantic/SemanticAnalyzer.hpp"
#include "../error/Error.hpp"
#include "ObjectAccess.hpp"
#include "VariableValue.hpp"
#include "../semantic/Call.hpp"
#include "../semantic/Callable.hpp"
#include "../semantic/FunctionVersion.hpp"
#include "../semantic/Variable.hpp"
#include "ArrayAccess.hpp"
#include "../resolver/File.hpp"
#if COMPILER
#include "../../compiler/Compiler.hpp"
#endif

namespace ls {

FunctionCall::FunctionCall(Environment& env, Token* t) : Value(env), token(t), callable_version(env) {
	std_func = nullptr;
	this_ptr = nullptr;
	constant = false;
	callable = std::make_unique<Callable>();
	callable->add_version({ "<fc>", env.void_, false });
}

void FunctionCall::print(std::ostream& os, int indent, PrintOptions options) const {

	auto parenthesis = options.condensed && dynamic_cast<const Function*>(function.get());
	if (parenthesis) os << "(";
	function->print(os, indent, options);
	if (parenthesis) os << ")";

	os << "(";
	for (unsigned i = 0; i < arguments.size(); ++i) {
		arguments.at(i)->print(os, indent, options);
		if (i < arguments.size() - 1) {
			os << ", ";
		}
	}
	os << ")";
	if (options.debug) {
		os << " " << type;
	}
}

Location FunctionCall::location() const {
	return {closing_parenthesis->location.file, function->location().start, closing_parenthesis->location.end};
}

void FunctionCall::pre_analyze(SemanticAnalyzer* analyzer) {
	function->pre_analyze(analyzer);

	bool var_updated = false;
	if (auto object_access = dynamic_cast<const ObjectAccess*>(function.get())) {
		if (auto vv = dynamic_cast<VariableValue*>(object_access->object.get())) {
			if (vv->var and vv->var->scope != VarScope::CAPTURE and vv->var->scope != VarScope::INTERNAL) {
				// std::cout << "FC update_var " << vv->var;
				vv->var = analyzer->update_var(vv->var);
				// std::cout << " to " << vv->var << std::endl;
				vv->update_variable = true;
				var_updated = true;
			}
		} else if (auto aa = dynamic_cast<ArrayAccess*>(object_access->object.get())) {
			if (auto vv = dynamic_cast<VariableValue*>(aa->array.get())) {
				vv->var = analyzer->update_var(vv->var);
				vv->update_variable = true;
				var_updated = true;
			} else if (auto aa2 = dynamic_cast<ArrayAccess*>(aa->array.get())) {
				if (auto vv2 = dynamic_cast<VariableValue*>(aa2->array.get())) {
					vv2->var = analyzer->update_var(vv2->var);
					vv2->update_variable = true;
					var_updated = true;
				}
			}
		}
	}

	for (const auto& argument : arguments) {
		argument->pre_analyze(analyzer);
	}
	if (not var_updated) {
		if (arguments.size()) {
			if (auto vv = dynamic_cast<VariableValue*>(arguments[0].get())) {
				if (vv->var and vv->var->scope != VarScope::CAPTURE and vv->var->scope != VarScope::INTERNAL) {
					// std::cout << "FunctionCall update_var " << vv->var << std::endl;
					vv->var = analyzer->update_var(vv->var);
					vv->update_variable = true;
				}
			}
		}
	}
}

Call FunctionCall::get_callable(SemanticAnalyzer*, int argument_count) const {
	std::vector<const Type*> arguments_types;
	for (const auto& argument : arguments) {
		arguments_types.push_back(argument->type);
	}
	auto type = function->version_type(arguments_types);
	callable->versions.front().type = type->return_type();
	return { callable.get(), this };
}

void FunctionCall::analyze(SemanticAnalyzer* analyzer) {
	auto& env = analyzer->env;

	// std::cout << "FC analyse " << std::endl;

	// Analyse the function (can be anything here)
	function->analyze(analyzer);
	throws = function->throws;

	// Analyse arguments
	for (const auto& argument : arguments) {
		argument->analyze(analyzer);
		throws |= argument->throws;
	}

	// Perform a will_take to prepare eventual versions
	std::vector<const Type*> arguments_types;
	for (const auto& argument : arguments) {
		arguments_types.push_back(argument->type);
	}
	function->will_take(analyzer, arguments_types, 1);
	function->set_version(analyzer, arguments_types, 1);

	if (not function->type->can_be_callable()) {
		analyzer->add_error({Error::Type::CANNOT_CALL_VALUE, ErrorLevel::ERROR, location(), function->location(), {function->to_string()}});
	}

	// Retrieve the callable version
	call = function->get_callable(analyzer, arguments_types.size());
	// std::cout << "Function call: " << call << std::endl;
	callable_version = call.resolve(analyzer, arguments_types);
	if (callable_version) {
		// std::cout << "Version: " << callable_version << std::endl;
		type = callable_version.type->return_type();
		throws |= callable_version.template_()->flags & Module::THROWS;
		std::vector<Value*> raw_arguments;
		for (const auto& a : arguments) raw_arguments.push_back(a.get());
		call.apply_mutators(analyzer, callable_version, raw_arguments);

		int offset = call.object ? 1 : 0;
		for (size_t a = 0; a < arguments.size(); ++a) {
			auto argument_type = callable_version.type->argument(a + offset);
			if (argument_type->is_function() or argument_type->is_function_object()) {
				arguments.at(a)->will_take(analyzer, argument_type->arguments(), 1);
				arguments.at(a)->set_version(analyzer, argument_type->arguments(), 1);
			}
		}
		if (call.value) {
			function_type = function->will_take(analyzer, arguments_types, 1);
			function->set_version(analyzer, arguments_types, 1);
			type = function_type->return_type();
			auto vv = dynamic_cast<VariableValue*>(function.get());
			if (vv and vv->var) {
				if (callable_version.template_()->user_fun == analyzer->current_function()) {
					analyzer->current_function()->recursive = true;
					type = analyzer->current_function()->getReturnType(env);
				}
			}
		}
		if (callable_version.template_()->unknown) {
			for (const auto& arg : arguments) {
				if (arg->type->is_function()) {
					arg->must_return_any(analyzer);
				}
			}
		}
		return;
	}
	// Find the function object
	function_object = dynamic_cast<Function*>(function.get());
	if (!function_object) {
		auto vv = dynamic_cast<VariableValue*>(function.get());
		if (vv && vv->var && vv->var->value) {
			if (auto f = dynamic_cast<Function*>(vv->var->value)) {
				function_object = f;
			}
		}
	}

	// Detect standard library functions
	auto oa = dynamic_cast<ObjectAccess*>(function.get());
	if (oa != nullptr) {
		auto arguments_count = arguments_types.size() + (call.object ? 1 : 0);
		if (not call.callables.size() or (not callable_version and call.callables.front()->versions[0].type->arguments().size() == arguments_count)) {
			auto field_name = oa->field->content;
			auto object_type = oa->object->type;
			std::vector<const Type*> arg_types;
			for (const auto& arg : arguments) {
				arg_types.push_back(arg->type->fold());
			}
			std::ostringstream args_string;
			for (unsigned i = 0; i < arg_types.size(); ++i) {
				if (i > 0) args_string << ", ";
				args_string << arg_types[i];
			}
			if (object_type->is_class()) { // String.size("salut")
				std::string clazz = ((VariableValue*) oa->object.get())->name;
				analyzer->add_error({Error::Type::STATIC_METHOD_NOT_FOUND, ErrorLevel::ERROR, location(), oa->field->location, {clazz + "::" + oa->field->content + "(" + args_string.str() + ")"}});
				return;
			} else {  // "salut".size()
				bool has_unknown_argument = false;
				if (!object_type->fold()->is_any() && !has_unknown_argument) {
					std::ostringstream obj_type_ss;
					obj_type_ss << object_type;
					analyzer->add_error({Error::Type::METHOD_NOT_FOUND, ErrorLevel::ERROR, location(), oa->field->location, {obj_type_ss.str() + "." + oa->field->content + "(" + args_string.str() + ")"}});
					return;
				} else {
					is_unknown_method = true;
					object = oa->object.get();
				}
			}
		}
	} else if (call.callables.size() and call.callables.front()->versions.size() and not call.callables.front()->versions[0].user_fun) {
		std::ostringstream args_string;
		for (unsigned i = 0; i < arguments_types.size(); ++i) {
			if (i > 0) args_string << ", ";
			args_string << arguments_types[i];
		}
		analyzer->add_error({Error::Type::METHOD_NOT_FOUND, ErrorLevel::ERROR, location(), function->location(), {function->to_string() + "(" + args_string.str() + ")"}});
		return;
	}

	// Check arguments count
	arg_types.clear();
	auto arguments_count = arguments.size();
	if (this_ptr != nullptr) arguments_count++;
	bool arguments_valid = arguments_count <= function->type->arguments().size();
	auto total_arguments_passed = std::max(arguments.size(), function->type->arguments().size());
	int offset = this_ptr != nullptr ? 1 : 0;
	size_t a = 0;
	for (auto& argument_type : function->type->arguments()) {
		if (a == 0 and this_ptr != nullptr) {
			// OK it's the object for method call
		} else if (a < arguments_count) {
			// OK, the argument is present in the call
			arg_types.push_back(arguments.at(a - offset)->type);
		} else if (function_object && function_object->defaultValues.at(a - offset) != nullptr) {
			// OK, there's no argument in the call but a default value is set.
			arg_types.push_back(function_object->defaultValues.at(a - offset)->type);
		} else {
			// Missing argument
			arguments_valid = false;
			total_arguments_passed--;
		}
		a++;
	}
	if ((function->type->is_function() or function->type->is_function_pointer()) and !arguments_valid) {
		analyzer->add_error({Error::Type::WRONG_ARGUMENT_COUNT,	ErrorLevel::ERROR, location(), location(), {
			function->to_string(),
			std::to_string(function->type->arguments().size()),
			std::to_string(total_arguments_passed)
		}});
		return;
	}
}

const Type* FunctionCall::will_take(SemanticAnalyzer* analyzer, const std::vector<const Type*>& args, int level) {
	// std::cout << "FC " << this << " will_take " << args << std::endl;
	auto ret = function->will_take(analyzer, args, level + 1);

	if (callable_version) {
		// Perform a will_take to prepare eventual versions
		std::vector<const Type*> arguments_types;
		for (const auto& argument : arguments) {
			arguments_types.push_back(argument->type);
		}
		// Retrieve the callable version
		call = function->get_callable(analyzer, arguments_types.size());
		callable_version = call.resolve(analyzer, arguments_types);
		if (callable_version) {
			type = callable_version.template_()->type->return_type();
		}
	}
	return ret;
}

void FunctionCall::set_version(SemanticAnalyzer* analyzer, const std::vector<const Type*>& args, int level) {
	function->set_version(analyzer, args, level + 1);
}

const Type* FunctionCall::version_type(std::vector<const Type*> version) const {
	// std::cout << "FunctionCall " << this << " ::version_type(" << version << ") " << std::endl;
	auto function_type = function->version_type(function->version);
	return function_type->return_type()->function()->version_type(version);
}

Completion FunctionCall::autocomplete(SemanticAnalyzer& analyzer, size_t position) const {

	// std::cout << "FC complete " << position << "opening=" << opening_parenthesis->location.end.raw << " closing= " << closing_parenthesis->location.end.raw << std::endl;

	if (position < opening_parenthesis->location.start.raw) {
		return function->autocomplete(analyzer, position);
	}
	if (position >= opening_parenthesis->location.end.raw and position <= closing_parenthesis->location.start.raw) {
		Completion completion { analyzer.env.void_ };
		Position pos = opening_parenthesis->location.end;
		Location location { nullptr, pos, pos };
		for (const auto& variable : analyzer.current_block()->variables) {
			completion.items.push_back({ variable.first, CompletionType::VARIABLE, variable.second->type, location });
		}
		for (const auto& global : analyzer.program->globals) {
			if (global.second->clazz) {
				for (const auto& method : global.second->clazz->methods) {
					for (const auto& version : method.second.versions) {
						if (version.flags & Module::PRIVATE) continue;
						completion.items.push_back({ method.first, CompletionType::METHOD, version.type, location });
					}
				}
				for (const auto& field : global.second->clazz->static_fields) {
					if (field.second.flags & Module::PRIVATE) continue;
					completion.items.push_back({ field.first, CompletionType::FIELD, field.second.type, location });
				}
			}
		}
		return completion;
	}
	return { analyzer.env.void_ };
}

Hover FunctionCall::hover(SemanticAnalyzer& analyzer, size_t position) const {
	if (function->location().contains(position)) {
		auto hover = function->hover(analyzer, position);
		hover.type = callable_version.type;
		return hover;
	}
	for (const auto& argument : arguments) {
		if (argument->location().contains(position)) {
			auto hover = argument->hover(analyzer, position);
			if (include) {
				hover.defined_file = included_file->path;
				hover.defined_line = 1;
			}
			return hover;
		}
	}
	return { callable_version.type->return_type(), location() };
}

#if COMPILER
Compiler::value FunctionCall::compile(Compiler& c) const {

	// Don't compile include function call
	if (include) return { c.env };

	c.mark_offset(location().start.line);

	// std::cout << "FunctionCall::compile(" << function_type << ")" << std::endl;
	assert(callable_version);
	// std::cout << "callable_version = " << (void*) callable_version.template_() << std::endl;

	if (call.object) {
		callable_version.compile_mutators(c, { call.object });
	} else if (arguments.size()) {
		callable_version.compile_mutators(c, { arguments[0].get() });
	}

	std::vector<LSValueType> types;
	std::vector<Compiler::value> args;
	// Pre-compile the call (compile the potential object first)
	if (call.object) {
		args.push_back(call.pre_compile_call(c));
		types.push_back(args.at(0).t->id());
	}

	int offset = call.object ? 1 : 0;
	auto f = callable_version.type->function();

	for (unsigned i = 0; i < callable_version.type->arguments().size(); ++i) {
		if (i < arguments.size()) {
			types.push_back((LSValueType) callable_version.type->argument(i + offset)->id());
			auto arg = arguments.at(i)->compile(c);
			if (arg.t->is_function_pointer()) {
				args.push_back(c.insn_convert(arg, callable_version.type->argument(i + offset)));
			} else if (arguments.at(i)->type->is_primitive()) {
				args.push_back(c.insn_convert(arg, callable_version.type->argument(i + offset)));
			} else if (arguments.at(i)->type->is_polymorphic() and callable_version.type->argument(i + offset)->is_primitive()) {
				args.push_back(c.insn_convert(arg, callable_version.type->argument(i + offset), true));
			} else {
				args.push_back(arg);
			}
		} else if (f and ((Function*) f)->defaultValues.at(i)) {
			types.push_back((LSValueType) ((Function*) f)->defaultValues.at(i)->type->id());
			args.push_back(((Function*) f)->defaultValues.at(i)->compile(c));
		}
	}
	// Check arguments
	c.insn_check_args(args, types);
	int flags = is_void ? Module::NO_RETURN : 0;
	auto r = call.compile_call(c, callable_version, args, flags);
	// std::cout << "FC compiled type " << r.t << std::endl;
	c.inc_ops(1);
	for (unsigned i = 0; i < callable_version.type->arguments().size(); ++i) {
		if (i < arguments.size()) {
			arguments.at(i)->compile_end(c);
		}
	}
	return r;
}
#endif

std::unique_ptr<Value> FunctionCall::clone(Block* parent) const {
	auto fc = std::make_unique<FunctionCall>(type->env, token);
	fc->function = function->clone(parent);
	for (const auto& a : arguments) {
		fc->arguments.emplace_back(a->clone(parent));
	}
	fc->opening_parenthesis = opening_parenthesis;
	fc->closing_parenthesis = closing_parenthesis;
	return fc;
}

}
