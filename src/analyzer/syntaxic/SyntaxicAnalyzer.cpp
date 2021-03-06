#include "SyntaxicAnalyzer.hpp"
#include <string>
#include <math.h>
#include "../value/Function.hpp"
#include "../instruction/Instruction.hpp"
#include "../value/Block.hpp"
#include "../instruction/Break.hpp"
#include "../instruction/Foreach.hpp"
#include "../instruction/For.hpp"
#include "../value/If.hpp"
#include "../value/Match.hpp"
#include "../instruction/Continue.hpp"
#include "../instruction/ExpressionInstruction.hpp"
#include "../instruction/ClassDeclaration.hpp"
#include "../instruction/VariableDeclaration.hpp"
#include "../instruction/Return.hpp"
#include "../instruction/Throw.hpp"
#include "../instruction/While.hpp"
#include "../value/AbsoluteValue.hpp"
#include "../value/Array.hpp"
#include "../value/Interval.hpp"
#include "../value/Map.hpp"
#include "../value/Set.hpp"
#include "../value/ArrayAccess.hpp"
#include "../value/Boolean.hpp"
#include "../value/FunctionCall.hpp"
#include "../value/Nulll.hpp"
#include "../value/Number.hpp"
#include "../value/Object.hpp"
#include "../value/ObjectAccess.hpp"
#include "../value/PostfixExpression.hpp"
#include "../value/PrefixExpression.hpp"
#include "../value/String.hpp"
#include "../value/VariableValue.hpp"
#include "../value/ArrayFor.hpp"
#include "../Program.hpp"
#include "../lexical/Token.hpp"
#include "../../util/Util.hpp"
#include "../lexical/LexicalAnalyzer.hpp"
#include "../resolver/Resolver.hpp"
#include "../error/Error.hpp"
#include "../semantic/FunctionVersion.hpp"
#include "../value/Phi.hpp"

namespace ls {

SyntaxicAnalyzer::SyntaxicAnalyzer(Environment& env, Resolver* resolver, File* root_file) : resolver(resolver), env(env) {
	time = 0;
	nt = nullptr;
	t = nullptr;
	i = 0;
	this->root_file = root_file;
	this->root_file->included_files.clear();
}

Block* SyntaxicAnalyzer::analyze(File* file) {
	this->file = file;

	// Call the lexical analyzer to parse tokens, only if the file was never read before
	if (not file->tokens_read) {
		file->tokens = lexical.analyze(file);
		file->tokens_read = true;
	}

	this->t = &file->tokens.at(0);
	this->nt = file->tokens.size() > 1 ? &file->tokens.at(1) : nullptr;
	this->i = 0;

	return eatMain(file);
}

Block* SyntaxicAnalyzer::eatMain(File* file) {

	auto block = new Block(env, true);
	file->waiting = nullptr;

	while (true) {
		if (t->type == TokenType::FINISHED) {
			eat();
			break;
		} else if (t->type == TokenType::SEMICOLON) {
			eat();
		} else {
			auto ins = eatInstruction(block);
			// Include instruction
			if (auto ei = dynamic_cast<ExpressionInstruction*>(ins)) {
				if (auto fc = dynamic_cast<FunctionCall*>(ei->value.get())) {
					if (auto vv = dynamic_cast<VariableValue*>(fc->function.get())) {
						if (fc->arguments.size() == 1) {
							auto str = dynamic_cast<String*>(fc->arguments.at(0).get());
							if (vv->name == "include" and str) {
								auto included_file = resolver->resolve(str->token->content, file->context);
								if (included_file) {
									// Not already included?
									if (included_file != root_file and std::find(root_file->included_files.begin(), root_file->included_files.end(), included_file) == root_file->included_files.end()) {
										// std::cout << "include " << included_file->path << std::endl;
										included_file->includers_files[file->path] = file;

										// The included file is waiting for the file we are currently analyzing, so it's not waiting anymore
										// if (included_file->waiting == root_file) {
										// 	included_file->waiting = nullptr;
										// }
										if (included_file->waiting) { // The file exists but is not loaded yet, it will be loaded later
											// std::cout << "Include " << included_file->path << " waiting, cancel..." << std::endl;
											// file->waiting = included_file;
											// included_file->waiters.push_back(file);
											delete ins;
											return block;
										}
										// std::cout << "include " << included_file->path << std::endl;
										file->included_files.push_back(included_file);
										root_file->included_files.push_back(included_file);
										root_file->program->included_files.insert(included_file);

										// Add the include() function call in the file for inspection
										ins->include = true;
										fc->include = true;
										fc->included_file = included_file;
										block->add_instruction(ins);

										// std::cout << "included file " << included_file->code << std::endl;
										auto included_block = SyntaxicAnalyzer(env, resolver, root_file).analyze(included_file);
										for (auto& instruction : included_block->instructions) {
											instruction->included = true;
											block->add_instruction(instruction.get());
											instruction.release();
										}
									} else {
										// std::cout << "file " << included_file->path << " already included" << std::endl;
									}
									continue;
								} else {
									auto location = fc->arguments.at(0)->location();
									root_file->errors.push_back(Error(Error::Type::NO_SUCH_FILE, ErrorLevel::ERROR, location, location, { str->token->content }));
								}
							}
						}
					}
				}
			}
			if (ins) {
				block->add_instruction(ins);
			}
		}
	}
	return block;
}

/*
 * Detects whether a opening brace is an object or a block
 *
 * {} => object
 * {a: 12} => object
 * { 12 } => block
 */
bool SyntaxicAnalyzer::isObject() {

	if (nt != nullptr and nt->type == TokenType::CLOSING_BRACE) {
		return true;
	}

	auto nnt = nextTokenAt(2);
	if (nt != nullptr and nt->type == TokenType::IDENT and nnt != nullptr and nnt->type == TokenType::COLON) {
		return true;
	}
	return false;
}

Value* SyntaxicAnalyzer::eatBlockOrObject(Block* block, Section* before, Section* after) {
	if (isObject()) {
		return eatObject(block);
	} else {
		return eatBlock(block, false, false, before, after);
	}
}

Block* SyntaxicAnalyzer::eatBlock(Block* parent, bool is_function_block, bool single_instruction, Section* before, Section* after) {

	auto block = blockInit(parent, before, is_function_block);

	bool brace = false;
	if (t->type == TokenType::OPEN_BRACE) {
		brace = true;
		block->opening_brace = eat_get();
	}

	if (single_instruction) {
		// std::cout << "eat single instruction " << (int)t->type << std::endl;
		block->add_instruction(eatInstruction(block));
	} else {
		while (true) {
			if (t->type == TokenType::CLOSING_BRACE) {
				block->closing_brace = eat_get();
				break;
			} else if (t->type == TokenType::FINISHED || t->type == TokenType::ELSE || t->type == TokenType::END || t->type == TokenType::IN) {
				if (brace) {
					file->errors.push_back(Error(Error::Type::BLOCK_NOT_CLOSED, ErrorLevel::ERROR, t, {}));
				}
				break;
			} else if (t->type == TokenType::SEMICOLON) {
				eat();
			} else {
				Instruction* ins = eatInstruction(block);
				if (ins) block->add_instruction(ins);
			}
		}
	}
	blockEnd(block, after);
	return block;
}

Block* SyntaxicAnalyzer::newBlock(Block* parent, Value* value, Section* after) {
	auto block = blockInit(parent, nullptr, false);
	block->add_instruction(new ExpressionInstruction(env, std::unique_ptr<Value>(value)));
	blockEnd(block, after);
	return block;
}
Block* SyntaxicAnalyzer::newEmptyBlock(Block* parent, Section* after) {
	auto block = blockInit(parent, nullptr, false);
	blockEnd(block, after);
	return block;
}

Block* SyntaxicAnalyzer::blockInit(Block* parent, Section* before, bool is_function_block) {

	auto block = new Block(env, is_function_block);

	auto real_before = before ? before : (parent ? parent->sections.back().get() : nullptr);

	if (real_before) {
		auto start_section = block->sections.front().get();
		real_before->add_successor(start_section);
		start_section->add_predecessor(real_before);
	}

	return block;
}

void SyntaxicAnalyzer::blockEnd(Block* block, Section* after) {
	if (after) {
		if (block->sections.back()->successors.size() == 0 and not block->returning) {
			// std::cout << block->sections.back()->name << " add successor " << after->name << std::endl;
			block->sections.back()->add_successor(after);
			after->add_predecessor(block->sections.back().get());
		}
	} else {
		// auto end_section = new Section(env, "end_block");
		// block->sections.back()->add_successor(end_section);
		// end_section->add_predecessor(block->sections.back());
		// block->end_section = end_section;
	}
}

Object* SyntaxicAnalyzer::eatObject(Block* block) {

	auto o = new Object(env);

	o->opening_brace = eat_get(TokenType::OPEN_BRACE);

	while (t->type == TokenType::IDENT) {

		o->keys.push_back(eatIdent());
		eat(TokenType::COLON);
		o->values.emplace_back(eatExpression(block));

		if (t->type == TokenType::COMMA) {
			eat();
		}
	}
	o->closing_brace = eat_get(TokenType::CLOSING_BRACE);

	return o;
}

Instruction* SyntaxicAnalyzer::eatInstruction(Block* block) {

	switch (t->type) {

		case TokenType::LET:
		case TokenType::VAR:
		case TokenType::GLOBAL:
			return eatVariableDeclaration(block);

		case TokenType::NUMBER:
		case TokenType::PI:
		case TokenType::TRUE:
		case TokenType::FALSE:
		case TokenType::STRING:
		case TokenType::IDENT:
		case TokenType::OPEN_BRACKET:
		case TokenType::OPEN_PARENTHESIS:
		case TokenType::OPEN_BRACE:
		case TokenType::NULLL:
		case TokenType::ARROW:
		case TokenType::IF:
		case TokenType::MINUS:
		case TokenType::NOT:
		case TokenType::MINUS_MINUS:
		case TokenType::PLUS_PLUS:
		case TokenType::NEW:
		case TokenType::AROBASE:
		case TokenType::PLUS:
		case TokenType::TIMES:
		case TokenType::DIVIDE:
		case TokenType::INT_DIV:
		case TokenType::POWER:
		case TokenType::MODULO:
		case TokenType::PIPE:
		case TokenType::TILDE:
		case TokenType::LOWER:
		case TokenType::GREATER:
		case TokenType::LOWER_EQUALS:
		case TokenType::GREATER_EQUALS:
		case TokenType::OR: {
			auto expression = std::unique_ptr<Value>(eatExpression(block));
			if (expression) {
				return new ExpressionInstruction(env, std::move(expression));
			} else {
				// No expression : () for example
				return nullptr;
			}
		}
		case TokenType::MATCH:
			return new ExpressionInstruction(env, std::unique_ptr<Value>(eatMatch(block, false)));

		case TokenType::FUNCTION:
			return eatFunctionDeclaration(block);

		case TokenType::RETURN: {
			auto token = eat_get();
			if (t->type == TokenType::FINISHED or t->type == TokenType::CLOSING_BRACE
				or t->type == TokenType::ELSE or t->type == TokenType::END or t->type == TokenType::SEMICOLON) {
				return new Return(env, token);
			} else {
				return new Return(env, token, std::unique_ptr<Value>(eatExpression(block)));
			}
		}
		case TokenType::THROW: {
			auto throw_token = eat_get();
			if (t->type == TokenType::FINISHED or t->type == TokenType::CLOSING_BRACE
				or t->type == TokenType::ELSE or t->type == TokenType::END) {
				return new Throw(env, throw_token);
			} else {
				return new Throw(env, throw_token, std::unique_ptr<Value>(eatExpression(block)));
			}
		}
		case TokenType::BREAK:
			return eatBreak(block);

		case TokenType::CONTINUE:
			return eatContinue(block);

		case TokenType::CLASS:
			return eatClassDeclaration(block);

		case TokenType::FOR:
			return eatFor(block);

		case TokenType::WHILE:
			return eatWhile(block);

		default:
			// std::cout << "Unexpected token : " << (int)t->type << " (" << t->content << ")" << std::endl;
			file->errors.push_back(Error(Error::Type::UNEXPECTED_TOKEN, ErrorLevel::ERROR, t, {t->content}));
			eat();
			return nullptr;
	}
}

VariableDeclaration* SyntaxicAnalyzer::eatVariableDeclaration(Block* block) {

	auto vd = new VariableDeclaration(env);

	if (t->type == TokenType::GLOBAL) {
		vd->keyword = eat_get(TokenType::GLOBAL);
		vd->global = true;
	} else if (t->type == TokenType::LET) {
		vd->keyword = eat_get(TokenType::LET);
		vd->constant = true;
	} else {
		vd->keyword = eat_get(TokenType::VAR);
	}

	auto ident = eatIdent();
	vd->variables.push_back(ident);
	if (t->type == TokenType::EQUAL) {
		eat(TokenType::EQUAL);
		auto expression = eatExpression(block);
		vd->jumping |= expression->jumping;
		if (expression->jumping) {
			assert(expression->end_section);
		}
		vd->end_section = expression->end_section;
		vd->expressions.emplace_back(expression);
	} else {
		vd->expressions.push_back(nullptr);
	}

	while (t->type == TokenType::COMMA) {
		eat();
		ident = eatIdent();
		vd->variables.push_back(ident);
		if (t->type == TokenType::EQUAL) {
			eat(TokenType::EQUAL);
			auto expression = eatExpression(block);
			vd->jumping |= expression->jumping;
			vd->end_section = expression->end_section;
			vd->expressions.emplace_back(expression);
		} else {
			vd->expressions.push_back(nullptr);
		}
	}
	return vd;
}

Function* SyntaxicAnalyzer::eatFunction(Block* block, Token* token) {

	if (t->type == TokenType::FUNCTION) {
		token = eat_get();
	}

	auto f = new Function(env, token);

	eat(TokenType::OPEN_PARENTHESIS);

	while (t->type != TokenType::FINISHED && t->type != TokenType::CLOSING_PARENTHESIS) {

		bool reference = false;
		if (t->type == TokenType::AROBASE) {
			reference = true;
			eat();
		}

		auto ident = eatIdent();

		Value* defaultValue = nullptr;
		if (t->type == TokenType::EQUAL) {
			eat();
			defaultValue = eatExpression(block);
		}

		f->addArgument(ident, defaultValue, reference);

		if (t->type == TokenType::COMMA) {
			eat();
		}
	}
	eat(TokenType::CLOSING_PARENTHESIS);

	bool braces = false;
	if (t->type == TokenType::OPEN_BRACE) {
		braces = true;
	}

	f->body.reset(eatBlock(nullptr, true));

	if (!braces) {
		eat(TokenType::END);
	}

	return f;
}

VariableDeclaration* SyntaxicAnalyzer::eatFunctionDeclaration(Block* block) {

	auto token = eat_get(TokenType::FUNCTION);

	auto vd = new VariableDeclaration(env);
	vd->keyword = token;
	vd->global = true;
	vd->function = true;

	vd->variables.emplace_back(eatIdent());
	vd->expressions.emplace_back(eatFunction(block, token));

	return vd;
}

bool SyntaxicAnalyzer::beginingOfExpression(TokenType type) {

	return type == TokenType::NUMBER or type == TokenType::IDENT
		or type == TokenType::AROBASE or type == TokenType::OPEN_BRACKET
		or type == TokenType::OPEN_BRACE or type == TokenType::OPEN_PARENTHESIS
		or type == TokenType::STRING or type == TokenType::PI or type == TokenType::TRUE
		or type == TokenType::FALSE or type == TokenType::NULLL;
}

int SyntaxicAnalyzer::findNextClosingParenthesis() {
	int p = i;
	int level = 1;
	while (level > 0) {
		auto t = file->tokens.at(p++).type;
		if (t == TokenType::FINISHED) return -1;
		if (t == TokenType::OPEN_PARENTHESIS) level++;
		if (t == TokenType::CLOSING_PARENTHESIS) level--;
	}
	return p - 1;
}

int SyntaxicAnalyzer::findNextArrow() {
	int p = i;
	while (true) {
		auto t = file->tokens.at(p++).type;
		if (t == TokenType::FINISHED) return -1;
		if (t == TokenType::ARROW) break;
	}
	return p - 1;
}

int SyntaxicAnalyzer::findNextColon() {
	int p = i;
	while (true) {
		auto t = file->tokens.at(p++).type;
		if (t == TokenType::FINISHED) return -1;
		if (t == TokenType::COLON) break;
	}
	return p - 1;
}

void SyntaxicAnalyzer::splitCurrentOrInTwoPipes() {
	file->tokens.erase(file->tokens.begin() + i);
	file->tokens.insert(file->tokens.begin() + i, { TokenType::PIPE, file, t->location.end.raw, t->location.start.line, t->location.end.column, "|" });
	file->tokens.insert(file->tokens.begin() + i + 1, { TokenType::PIPE, file, t->location.end.raw + 1, t->location.start.line, t->location.end.column + 1, "|" });
	t = &file->tokens.at(i);
	nt = &file->tokens.at(i + 1);
}

Value* SyntaxicAnalyzer::eatSimpleExpression(Block* block, bool pipe_opened, bool set_opened, bool comma_list, Value* initial) {

	Value* e = nullptr;

	if (initial == nullptr) {
		if (t->type == TokenType::OPEN_PARENTHESIS) {

			e = eatLambdaOrParenthesisExpression(block, pipe_opened, set_opened, comma_list);

		} else if (t->type == TokenType::PIPE or (t->type == TokenType::OR and t->content == "||")) {

			// If we start a instruction with a ||, split it into two |, and it's an absolute value expression
			if (t->type == TokenType::OR) {
				splitCurrentOrInTwoPipes();
			}

			auto open_pipe = eat_get();
			auto av = new AbsoluteValue(env);
			av->open_pipe = open_pipe;
			av->expression = std::unique_ptr<Value>(eatExpression(block, true));

			// We want a closing pipe, if there's a || operator, we split it
			if (t->type == TokenType::OR and t->content == "||") {
				splitCurrentOrInTwoPipes();
			}
			av->close_pipe = eat_get(TokenType::PIPE);
			e = new Expression(env, av);

		} else {

			// Opérateurs unaires préfixe
			if (t->type == TokenType::NEW || t->type == TokenType::MINUS ||
				t->type == TokenType::PLUS || t->type == TokenType::NOT ||
				t->type == TokenType::MINUS_MINUS || t->type == TokenType::PLUS_PLUS
				|| t->type == TokenType::TILDE || t->type == TokenType::AROBASE) {

				if (t->type == TokenType::MINUS && nt != nullptr && t->location.start.line == nt->location.start.line) {
					auto minus = eat_get();
					if (beginingOfExpression(t->type)) {

						auto op = new Operator(minus);
						auto ex = eatExpression(block, pipe_opened, set_opened);
						auto expr = dynamic_cast<Expression*>(ex);

						if (expr and expr->op->priority >= op->priority) {
							auto pexp = new PrefixExpression(env, std::shared_ptr<Operator>(op), std::move(expr->v1));
							expr->v1.reset(pexp);
							e = expr;
						} else {
							auto pe = new PrefixExpression(env, std::shared_ptr<Operator>(op), std::unique_ptr<Value>(ex));
							e = pe;
						}
					} else {
						// No expression after the -, so it's the variable '-'
						e = new VariableValue(env, minus);
					}

				} else if (t->type == TokenType::PLUS) {

					auto plus = eat_get(); // eat the +

					if (beginingOfExpression(t->type)) {
						e = eatExpression(block, pipe_opened);
					} else {
						// No expression after the +, so it's the variable '+'
						e = new VariableValue(env, plus);
					}
				} else if (t->type == TokenType::TILDE) {

					auto tilde = eat_get(); // eat the ~

					if (beginingOfExpression(t->type)) {
						auto op = std::make_shared<Operator>(tilde);
						auto ex = new PrefixExpression(env, op, std::unique_ptr<Value>(eatSimpleExpression(block)));
						e = new Expression(env, ex);
					} else {
						// No expression after the ~, so it's the variable '~'
						e = new VariableValue(env, tilde);
					}
				} else {
					auto op = std::make_shared<Operator>(eat_get());
					auto ex = new PrefixExpression(env, op, std::unique_ptr<Value>(eatSimpleExpression(block)));
					e = new Expression(env, ex);
				}
			} else {
				e = eatValue(block, comma_list);
			}
		}
	} else {
		e = initial;
	}

	while (t->type == TokenType::OPEN_BRACKET || t->type == TokenType::OPEN_PARENTHESIS || t->type == TokenType::DOT) {

		if (t->location.start.column != last_character + last_size)
			break;

		switch (t->type) {
			case TokenType::OPEN_BRACKET: {

				auto aa = new ArrayAccess(env);
				aa->open_bracket = eat_get(TokenType::OPEN_BRACKET);

				aa->array = std::unique_ptr<Value>(e);
				if (t->type == TokenType::CLOSING_BRACKET) {
					aa->key = nullptr;
				} else {
					aa->key = std::unique_ptr<Value>(eatExpression(block));
				}
				if (t->type == TokenType::COLON) {
					eat();
					aa->key2 = std::unique_ptr<Value>(eatExpression(block));
				}
				aa->close_bracket = eat_get(TokenType::CLOSING_BRACKET);
				e = aa;
				break;
			}
			case TokenType::OPEN_PARENTHESIS: {

				auto par = eat_get(TokenType::OPEN_PARENTHESIS);

				auto fc = new FunctionCall(env, par);
				fc->opening_parenthesis = par;
				fc->function = std::unique_ptr<Value>(e);

				if (t->type != TokenType::CLOSING_PARENTHESIS) {
					fc->arguments.emplace_back(eatExpression(block, false, false, nullptr, true));
					while (t->type != TokenType::CLOSING_PARENTHESIS && t->type != TokenType::FINISHED) {
						if (t->type == TokenType::COMMA) {
							eat();
						}
						fc->arguments.emplace_back(eatExpression(block, false, false, nullptr, true));
					}
				}
				fc->closing_parenthesis = eat_get(TokenType::CLOSING_PARENTHESIS);

				e = fc;
				break;
			}
			case TokenType::DOT: {

				ObjectAccess* oa;
				auto dot = eat_get(TokenType::DOT);

				if (t->type == TokenType::NEW || t->type == TokenType::CLASS) {
					oa = new ObjectAccess(env, eat_get());
				} else if (t->type == TokenType::RETURN) {
					oa = new ObjectAccess(env, eat_get());
				} else if (t->type == TokenType::IDENT) {
					oa = new ObjectAccess(env, eatIdent());
				} else {
					oa = new ObjectAccess(env, nullptr);
				}
				oa->dot = dot;
				oa->object = std::unique_ptr<Value>(e);
				e = oa;
				break;
			}
			default: {}
		}
	}

	// Opérateurs unaires postfixes
	if (t->type == TokenType::MINUS_MINUS || t->type == TokenType::PLUS_PLUS) {

		if (last_line == t->location.start.line) {

			auto op = eat_get();
			auto ex = new PostfixExpression(env);

			ex->operatorr = std::make_shared<Operator>(op);
			ex->expression = std::unique_ptr<Value>(e);
			e = ex;
		}
	}

	return e;
}

Value* SyntaxicAnalyzer::eatExpression(Block* block, bool pipe_opened, bool set_opened, Value* initial, bool comma_list) {

	Expression* ex = nullptr;
	Value* e = (initial != nullptr) ? initial : eatSimpleExpression(block, pipe_opened, set_opened, comma_list);

	// Opérateurs binaires
	while (t->type == TokenType::PLUS || t->type == TokenType::MINUS ||
		   t->type == TokenType::TIMES || t->type == TokenType::DIVIDE ||
		   t->type == TokenType::INT_DIV || t->type == TokenType::INT_DIV_EQUAL ||
		   t->type == TokenType::MODULO || t->type == TokenType::AND ||
		   (!pipe_opened and t->type == TokenType::OR and t->content == "||") || (t->type == TokenType::OR and t->content == "or")
		   || t->type == TokenType::XOR ||
		   t->type == TokenType::EQUAL || t->type == TokenType::POWER ||
		   t->type == TokenType::DOUBLE_EQUAL || t->type == TokenType::DIFFERENT ||
		   t->type == TokenType::TRIPLE_EQUAL || t->type == TokenType::TRIPLE_DIFFERENT ||
		   (!set_opened && t->type == TokenType::GREATER) || t->type == TokenType::LOWER ||
		   t->type == TokenType::GREATER_EQUALS || t->type == TokenType::LOWER_EQUALS ||
		   t->type == TokenType::TIMES_EQUAL || t->type == TokenType::PLUS_EQUAL ||
		   t->type == TokenType::MINUS_EQUAL || t->type == TokenType::DIVIDE_EQUAL ||
		   t->type == TokenType::MODULO_EQUAL || t->type == TokenType::POWER_EQUAL ||
		   t->type == TokenType::SWAP || t->type == TokenType::TILDE ||
		   t->type == TokenType::TILDE_TILDE || t->type == TokenType::TILDE_EQUAL ||
		   t->type == TokenType::TILDE_TILDE_EQUAL || t->type == TokenType::IN ||
		   t->type == TokenType::INSTANCEOF ||
		   t->type == TokenType::BIT_AND || t->type == TokenType::BIT_AND_EQUALS ||
		   (!pipe_opened and t->type == TokenType::PIPE) || t->type == TokenType::BIT_OR_EQUALS ||
		   t->type == TokenType::BIT_XOR || t->type == TokenType::BIT_XOR_EQUALS ||
		   t->type == TokenType::BIT_SHIFT_LEFT ||	t->type == TokenType::BIT_SHIFT_LEFT_EQUALS ||
		   t->type == TokenType::BIT_SHIFT_RIGHT || t->type == TokenType::BIT_SHIFT_RIGHT_EQUALS ||
		   t->type == TokenType::BIT_SHIFT_RIGHT_UNSIGNED || t->type == TokenType::BIT_SHIFT_RIGHT_UNSIGNED_EQUALS ||
		   t->type == TokenType::BIT_ROTATE_LEFT || t->type == TokenType::BIT_ROTATE_LEFT_EQUALS ||
		   t->type == TokenType::BIT_ROTATE_RIGHT || t->type == TokenType::BIT_ROTATE_RIGHT_EQUALS ||
		   t->type == TokenType::DOUBLE_QUESTION_MARK || t->type == TokenType::CATCH_ELSE || t->type == TokenType::DOUBLE_MODULO || t->type == TokenType::DOUBLE_MODULO_EQUALS
		   ) {

		if (t->type == TokenType::MINUS && t->location.start.line != last_line && nt != nullptr && t->location.start.line == nt->location.start.line)
			break;

		auto op = new Operator(eat_get());

		if (ex == nullptr) {
			if (auto exx = dynamic_cast<Expression*>(e)) {
				ex = exx;
			} else {
				ex = new Expression(env);
				ex->v1.reset(e);
			}
		}
		auto ex2 = eatSimpleExpression(block);
		if (ex2) {
			ex->append(std::shared_ptr<Operator>(op), ex2);
		}
	}
	if (ex != nullptr) {
		e = ex;
	}

	// Ternary
	if (t->type == TokenType::QUESTION_MARK) {
		eat();
		auto ternary = new If(env, true);
		ternary->condition = std::unique_ptr<Value>(e);

		ternary->end_section = new Section(env, "end_if");

		auto then = eatBlock(block, false, true, nullptr, ternary->end_section);
		ternary->then = std::unique_ptr<Block>(then);

		eat(TokenType::COLON);

		auto elze = eatBlock(block, false, true, nullptr, ternary->end_section);
		ternary->elze = std::unique_ptr<Block>(elze);

		return ternary;
	}

	return e;
}

Value* SyntaxicAnalyzer::eatValue(Block* block, bool comma_list) {

	switch (t->type) {

		case TokenType::PLUS:
		case TokenType::MINUS:
		case TokenType::MODULO:
		case TokenType::TIMES:
		case TokenType::DIVIDE:
		case TokenType::POWER:
		case TokenType::TERNARY:
		case TokenType::INT_DIV:
		case TokenType::TILDE:
		case TokenType::GREATER:
		case TokenType::LOWER_EQUALS:
		case TokenType::GREATER_EQUALS:
		{
			return new VariableValue(env, eat_get());
		}

		case TokenType::NUMBER:
		{
			auto n_token = eat_get();
			auto n = new Number(env, n_token->content, n_token);

			if (t->type == TokenType::STAR) {
				n->pointer = true;
				eat();
			}
			return n;
		}

		case TokenType::PI: {
			std::stringstream stream;
			stream << std::fixed << std::setprecision(19) << M_PI;
			return new Number(env, stream.str(), eat_get());
		}
		case TokenType::STRING:
		{
			return new String(env, eat_get());
		}

		case TokenType::TRUE:
		case TokenType::FALSE:
		{
			return new Boolean(env, eat_get());
		}

		case TokenType::NULLL:
			return new Nulll(env, eat_get());

		case TokenType::AROBASE:
		case TokenType::IDENT:
			return eatLambdaOrParenthesisExpression(block, false, false, comma_list);
			break;

		case TokenType::OPEN_BRACKET:
			return eatArrayOrMap(block);

		case TokenType::LOWER:
			return eatSetOrLowerOperator(block);

		case TokenType::OPEN_BRACE:
			return eatBlockOrObject(block);

		case TokenType::IF:
			return eatIf(block);

		case TokenType::MATCH:
			return eatMatch(block, true);

		case TokenType::FUNCTION:
			return eatFunction(block, nullptr);

		case TokenType::ARROW:
		{
			auto token = eat_get(TokenType::ARROW);
			Function* l = new Function(env, token);
			l->lambda = true;
			l->body = std::make_unique<Block>(env, true);
			l->body->add_instruction(new ExpressionInstruction(env, std::unique_ptr<Value>(eatExpression(l->body.get()))));
			return l;
		}

		default:
			break;
	}

	file->errors.push_back(Error(Error::EXPECTED_VALUE, ErrorLevel::ERROR, t, {t->content}));
	eat();
	return nullptr;
}

/*
 * Starting from:
 *   <ident>, <open_parenthesis>, <arobase>
 * Can return:
 *   <variable_value>, <expression>, <lambda>
 */
Value* SyntaxicAnalyzer::eatLambdaOrParenthesisExpression(Block* block, bool pipe_opened, bool set_opened, bool comma_list) {
	bool parenthesis = false;
	if (t->type == TokenType::OPEN_PARENTHESIS) {
		eat();
		parenthesis = true;
	}
	if (parenthesis and t->type != TokenType::IDENT) {
		if (t->type == TokenType::CLOSING_PARENTHESIS) {
			return eatValue(block); // error, expected a value got ')', it's wrong
		}
		// (...)
		auto ex = eatExpression(block);
		ex->parenthesis = true;
		eat(TokenType::CLOSING_PARENTHESIS);
		return ex;
	}
	auto ident = eatIdent();
	// var
	if (t->type == TokenType::EQUAL) {
		// var =
		auto eq = eat_get();
		auto ex = eatExpression(block);
		// var = <ex>
		if (parenthesis and t->type == TokenType::CLOSING_PARENTHESIS) {
			// (var = <ex>)
			eat();
			if (t->type == TokenType::ARROW) {
				// (var = <ex>) ->  [lambda]
				return eatLambdaContinue(block, false, ident, ex, comma_list);
			} else {
				// (var = <ex>) <token ?>	[expression]
				Expression* e = new Expression(env);
				e->parenthesis = true;
				e->v1.reset(new VariableValue(env, ident));
				e->op = std::make_shared<Operator>(eq);
				e->v2.reset(ex);
				return e;
			}
		} else if (t->type == TokenType::COMMA or t->type == TokenType::ARROW) {
			// var = <ex> ,|->  [lambda]
			return eatLambdaContinue(block, parenthesis, ident, ex, comma_list);
		} else {
			// var = <ex> <?>
			Expression* e = new Expression(env);
			e->v1.reset(new VariableValue(env, ident));
			e->op = std::make_shared<Operator>(eq);
			e->v2.reset(ex);
			return eatExpression(block, pipe_opened, set_opened, e);
		}
	} else if (t->type == TokenType::ARROW) {
		// var ->
		return eatLambdaContinue(block, parenthesis, ident, nullptr, comma_list);
	} else if (t->type == TokenType::COMMA) {
		// var,  [lambda]
		if (!parenthesis && comma_list) {
			return new VariableValue(env, ident);
		}
		int p = findNextClosingParenthesis();
		int a = findNextArrow();
		int c = findNextColon();
		if (parenthesis or (a != -1 and (a < p or p == -1) and (a < c or c == -1))) {
			return eatLambdaContinue(block, parenthesis, ident, nullptr, comma_list);
		} else {
			return new VariableValue(env, ident);
		}
	} else {
		if (parenthesis) {
			if (t->type == TokenType::CLOSING_PARENTHESIS) {
				// (var)
				eat();
				if (t->type == TokenType::ARROW) {
					return eatLambdaContinue(block, false, ident, nullptr, comma_list);
				}
				return new VariableValue(env, ident);
			} else {
				// ( var + ... )
				auto v = new VariableValue(env, ident);
				auto exx = eatSimpleExpression(block, false, false, false, v);
				auto ex = eatExpression(block, pipe_opened, set_opened, exx);
				ex->parenthesis = true;
				eat(TokenType::CLOSING_PARENTHESIS);
				return ex;
			}
		}
		// var <?>  [expression]
		return new VariableValue(env, ident);
	}
}

/*
 * Continue to eat a lambda starting from a comma or the arrow
 */
Value* SyntaxicAnalyzer::eatLambdaContinue(Block* block, bool parenthesis, Ident ident, Value* expression, bool comma_list) {
	auto l = new Function(env, nullptr);
	l->lambda = true;
	// Add first argument
	l->addArgument(ident.token, expression, false);
	// Add other arguments
	while (t->type == TokenType::COMMA) {
		eat();
		auto ident = eatIdent();
		Value* defaultValue = nullptr;
		if (t->type == TokenType::EQUAL) {
			eat();
			defaultValue = eatExpression(block);
		}
		l->addArgument(ident, defaultValue, false);
	}
	if (t->type == TokenType::CLOSING_PARENTHESIS) {
		eat();
		parenthesis = false;
	}
	auto token = eat_get(TokenType::ARROW);
	l->token = token;
	l->body = std::make_unique<Block>(env, true);
	l->body->add_instruction(new ExpressionInstruction(env, std::unique_ptr<Value>(eatExpression(l->body.get(), false, false, nullptr, comma_list))));
	if (parenthesis) {
		eat(TokenType::CLOSING_PARENTHESIS);
	}
	return l;
}

Value* SyntaxicAnalyzer::eatArrayOrMap(Block* block) {

	auto opening_bracket = eat_get(TokenType::OPEN_BRACKET);

	// Empty array
	if (t->type == TokenType::CLOSING_BRACKET) {
		auto array = new Array(env);
		array->opening_bracket = opening_bracket;
		array->closing_bracket = eat_get();
		return array;
	}

	// Empty map
	if (t->type == TokenType::COLON) {
		eat();
		auto map = new Map(env);
		map->opening_bracket = opening_bracket;
		map->closing_bracket = eat_get();
		return map;
	}

	// Array For
	if (t->type == TokenType::FOR) {
		ArrayFor* arrayFor = new ArrayFor(env);
		arrayFor->forr = std::unique_ptr<Instruction>(eatFor(block));
		eat(TokenType::CLOSING_BRACKET);
		arrayFor->end_section = arrayFor->forr->end_section;
		return arrayFor;
	}

	Value* value = eatExpression(block, false, false, nullptr, true);

	// eatInterval
	if (t->type == TokenType::TWO_DOTS) {

		Interval* interval = new Interval(env);
		interval->opening_bracket = opening_bracket;
		interval->start = std::unique_ptr<Value>(value);
		eat();
		interval->end = std::unique_ptr<Value>(eatExpression(block));

		interval->closing_bracket = eat_get(TokenType::CLOSING_BRACKET);
		return interval;
	}

	// eatMap
	if (t->type == TokenType::COLON) {

		auto map = new Map(env);
		map->opening_bracket = opening_bracket;
		map->keys.emplace_back(value);
		eat();
		map->values.emplace_back(eatExpression(block));

		while (t->type != TokenType::CLOSING_BRACKET && t->type != TokenType::FINISHED) {
			if (t->type == TokenType::COMMA)
				eat();
			map->keys.emplace_back(eatExpression(block));
			eat(TokenType::COLON);
			map->values.emplace_back(eatExpression(block));
		}
		map->closing_bracket = eat_get(TokenType::CLOSING_BRACKET);
		return map;
	}

	// eatArray
	auto array = new Array(env);
	array->opening_bracket = opening_bracket;
	array->expressions.emplace_back(value);
	if (t->type == TokenType::COMMA) {
		eat();
	}
	while (t->type != TokenType::CLOSING_BRACKET && t->type != TokenType::FINISHED) {
		array->expressions.emplace_back(eatExpression(block, false, false, nullptr, true));
		if (t->type == TokenType::COMMA)
			eat();
	}
	array->closing_bracket = eat_get(TokenType::CLOSING_BRACKET);
	return array;
}

Value* SyntaxicAnalyzer::eatSetOrLowerOperator(Block* block) {

	auto lower = eat_get();

	if (t->type == TokenType::CLOSING_PARENTHESIS or t->type == TokenType::COMMA or t->type == TokenType::SEMICOLON or t->type == TokenType::DOT) {
		return new VariableValue(env, lower);
	}

	auto set = new Set(env);

	if (t->type == TokenType::GREATER) {
		eat();
		return set;
	}

	set->expressions.emplace_back(eatExpression(block, false, true));

	while (t->type != TokenType::GREATER && t->type != TokenType::FINISHED) {
		if (t->type == TokenType::COMMA) eat();
		set->expressions.emplace_back(eatExpression(block, false, true));
	}
	eat(TokenType::GREATER);

	return set;
}

Value* SyntaxicAnalyzer::eatIf(Block* block) {

	auto iff = new If(env);

	iff->token = eat_get(TokenType::IF);

	iff->condition = std::unique_ptr<Value>(eatExpression(block));

	iff->end_section = new Section(env, "end_if");

	bool braces = false;
	bool then = false;
	if (t->type == TokenType::OPEN_BRACE) {
		braces = true;
	} else if (t->type == TokenType::THEN) {
		eat(TokenType::THEN);
		then = true;
	}

	if (then or braces) {
		Value* v = eatBlockOrObject(block, nullptr, iff->end_section);
		if (dynamic_cast<Block*>(v)) {
			iff->then = std::unique_ptr<Block>((Block*) v);
		} else {
			auto then = newBlock(block, v, iff->end_section);
			iff->then = std::unique_ptr<Block>(then);
		}
	} else {
		auto then = eatBlock(block, false, true, nullptr, iff->end_section);
		iff->then = std::unique_ptr<Block>(then);
	}

	if (t->type == TokenType::ELSE) {
		eat();

		bool bracesElse = false;
		if (t->type == TokenType::OPEN_BRACE) {
			bracesElse = true;
		}

		if (then or bracesElse) {
			Value* v = eatBlockOrObject(block, nullptr, iff->end_section);
			if (dynamic_cast<Block*>(v)) {
				iff->elze = std::unique_ptr<Block>((Block*) v);
			} else {
				auto elze = newBlock(block, v, iff->end_section);
				iff->elze = std::unique_ptr<Block>(elze);
			}
		} else {
			auto body = eatBlock(block, false, true, nullptr, iff->end_section);
			iff->elze = std::unique_ptr<Block>(body);
		}
	} else {
		iff->elze = std::unique_ptr<Block>(newEmptyBlock(block, iff->end_section));
	}

	if (then) {
		eat(TokenType::END);
	}

	return iff;
}

Match* SyntaxicAnalyzer::eatMatch(Block* block, bool force_value) {

	Match* match = new Match(env);

	eat(TokenType::MATCH);

	match->value.reset(eatExpression(block));

	eat(TokenType::OPEN_BRACE);

	while (t->type != TokenType::CLOSING_BRACE && t->type != TokenType::FINISHED) {
		std::vector<Match::Pattern> patterns;
		patterns.push_back(eatMatchPattern(block));
		while (t->type == TokenType::PIPE) {
			eat();
			patterns.push_back(eatMatchPattern(block));
		}
		match->pattern_list.emplace_back(std::move(patterns));
		eat(TokenType::COLON);
		if (t->type == TokenType::OPEN_BRACE) {
			match->returns.emplace_back(eatBlockOrObject(block));
		} else if (force_value) {
			Block* block = new Block(env);
			block->add_instruction(new ExpressionInstruction(env, std::unique_ptr<Value>(eatExpression(block))));
			match->returns.emplace_back(block);
		} else {
			Block* body = new Block(env);
			body->add_instruction(eatInstruction(body));
			match->returns.emplace_back(body);
		}

		while (t->type == TokenType::SEMICOLON) eat();
	}

	eat(TokenType::CLOSING_BRACE);
	return match;
}

Match::Pattern SyntaxicAnalyzer::eatMatchPattern(Block* block) {

	if (t->type == TokenType::TWO_DOTS) {
		eat();

		if (t->type == TokenType::COLON || t->type == TokenType::PIPE) {
			return Match::Pattern(std::unique_ptr<Value>(nullptr), std::unique_ptr<Value>(nullptr));
		} else {
			return Match::Pattern(std::unique_ptr<Value>(nullptr), std::unique_ptr<Value>(eatSimpleExpression(block)));
		}
	}

	Value* value = eatSimpleExpression(block);

	if (t->type == TokenType::TWO_DOTS) {
		eat();
		if (t->type == TokenType::COLON || t->type == TokenType::PIPE) {
			return Match::Pattern(std::unique_ptr<Value>(value), std::unique_ptr<Value>(nullptr));
		} else {
			return Match::Pattern(std::unique_ptr<Value>(value), std::unique_ptr<Value>(eatSimpleExpression(block)));
		}
	} else {
		return Match::Pattern(std::unique_ptr<Value>(value));
	}
}

Instruction* SyntaxicAnalyzer::eatFor(Block* block) {

	auto for_token = eat_get(TokenType::FOR);

	bool parenthesis = false;
	if (t->type == TokenType::OPEN_PARENTHESIS) {
		parenthesis = true;
		eat();
	}

	// Foreach:
	// for v in || for var v in || for k, v in
	// for var k, v in || for k, var v in || for var k, var v in
	auto t1 = t->type, t2 = nt->type, t3 = nextTokenAt(2)->type, t4 = nextTokenAt(3)->type, t5 = nextTokenAt(4)->type, t6 = nextTokenAt(5)->type;
	auto var = TokenType::VAR, let = TokenType::LET, ident = TokenType::IDENT, comma = TokenType::COMMA, colon = TokenType::COLON, in = TokenType::IN;
	bool is_foreach = (t1 == ident and t2 == in)
		or ((t1 == var or t1 == let) and t2 == ident and t3 == in)
		or (t1 == ident and (t2 == comma or t2 == colon) and t3 == ident and t4 == in)
		or ((t1 == var or t1 == let) and t2 == ident and (t3 == comma or t3 == colon) and t4 == ident and t5 == in)
		or (t1 == ident and (t2 == comma or t2 == colon) and (t3 == var or t3 == let) and t4 == ident and t5 == in)
		or ((t1 == var or t1 == let) and t2 == ident and (t3 == comma or t3 == colon) and (t4 == var or t4 == let) and t5 == ident and t6 == in);

	auto current_section = block->sections.back().get();

	if (!is_foreach) {

		auto init_block = new Block(env);

		auto init_section = init_block->sections[0].get();
		init_section->name = "init";

		init_section->predecessors.push_back(current_section);
		current_section->successors.push_back(init_section);
		while (true) {
			if (t->type == TokenType::FINISHED || t->type == TokenType::SEMICOLON || t->type == TokenType::IN || t->type == TokenType::OPEN_BRACE) {
				break;
			}
			auto ins = eatInstruction(init_block);
			if (ins) init_block->add_instruction(ins);
		}

		// for inits; condition; increments { body }
		auto f = new For(env);
		loops.push_back(f);
		f->token = for_token;

		f->end_section = new Section(env, "end_for");

		// init
		f->init = std::unique_ptr<Block>(init_block);
		eat(TokenType::SEMICOLON);

		// condition

		if (t->type == TokenType::SEMICOLON) {
			eat();
		} else {
			auto condition = std::unique_ptr<Value>(eatExpression(init_block));
			eat(TokenType::SEMICOLON);
			auto instruction = new ExpressionInstruction(env, std::move(condition));
			f->condition = std::make_unique<Block>(env);
			f->condition->sections.front()->name = "condition";
			f->condition->add_instruction(std::unique_ptr<Instruction>(instruction));

			f->condition->sections.front()->predecessors.push_back(init_section);
			init_section->successors.push_back(f->condition->sections.front().get());
		}

		// increment
		auto increment_block = new Block(env);
		increment_block->sections.front()->name = "increment";
		f->continue_section = increment_block->sections.front().get();
		while (true) {
			if (t->type == TokenType::FINISHED || t->type == TokenType::SEMICOLON || t->type == TokenType::DO || t->type == TokenType::OPEN_BRACE || t->type == TokenType::CLOSING_PARENTHESIS) {
				break;
			}
			auto ins = eatInstruction(increment_block);
			if (ins) increment_block->add_instruction(ins);
			if (t->type == TokenType::COMMA) {
				eat(TokenType::COMMA);
			}
		}
		f->increment = std::unique_ptr<Block>(increment_block);

		if (parenthesis)
			eat(TokenType::CLOSING_PARENTHESIS);

		// body
		auto condition_section = f->condition ? f->condition->sections.front().get() : init_section;
		if (t->type == TokenType::OPEN_BRACE) {
			f->body = std::unique_ptr<Block>(eatBlock(block, false, false, condition_section, f->increment->sections.front().get()));
		} else if (t->type == TokenType::DO) {
			eat(TokenType::DO);
			f->body = std::unique_ptr<Block>(eatBlock(block, false, false, condition_section, f->increment->sections.front().get()));
			eat(TokenType::END);
		} else {
			auto body = blockInit(block, condition_section, false);
			body->add_instruction(eatInstruction(body));
			blockEnd(body, f->increment->sections.front().get());
			f->body = std::unique_ptr<Block>(body);
		}
		f->body->sections.front()->name = "body";

		if (f->condition) {
			f->condition->sections.front()->successors.push_back(f->end_section);
			f->end_section->predecessors.push_back(f->condition->sections.front().get());
		} else {
			// f->body->sections.front()->predecessors.push_back(init_section);
			// init_section->successors.push_back(f->body->sections.front());
		}

		if (not f->increment->returning) {
			if (f->condition) {
				f->increment->sections.back()->successors.push_back(f->condition->sections.front().get());
				f->condition->sections.front()->predecessors.push_back(f->increment->sections.back().get());
			} else {
				f->increment->sections.back()->successors.push_back(f->body->sections.front().get());
				f->body->sections.front()->predecessors.push_back(f->increment->sections.back().get());
			}
		}

		loops.pop_back();
		return f;

	} else {

		// for key , value in container { body }
		auto f = new Foreach(env);
		f->token = for_token;

		auto wrapper_section = f->wrapper_block->sections[0].get();
		wrapper_section->name = "wrapper";

		wrapper_section->predecessors.push_back(current_section);
		current_section->successors.push_back(wrapper_section);

		loops.push_back(f);
		f->condition_section = std::make_unique<Section>(env, "condition");
		f->end_section = new Section(env, "end");

		if (t->type == TokenType::LET or t->type == TokenType::VAR) eat();

		if (nt->type == TokenType::COMMA || nt->type == TokenType::COLON) {
			f->key = eatIdent();
			eat();
		}
		if (t->type == TokenType::LET or t->type == TokenType::VAR) eat();

		f->value = eatIdent();

		eat(TokenType::IN);

		f->container = std::unique_ptr<Value>(eatExpression(f->wrapper_block.get()));

		if (f->container->jumping) {
			f->container->set_end_section(f->condition_section.get());
		} else {
			f->condition_section->predecessors.push_back(wrapper_section);
			wrapper_section->successors.push_back(f->condition_section.get());
		}

		if (parenthesis)
			eat(TokenType::CLOSING_PARENTHESIS);

		f->increment_section = std::make_unique<Section>(env, "increment");
		f->continue_section = f->increment_section.get();

		// body
		if (t->type == TokenType::OPEN_BRACE) {
			f->body = std::unique_ptr<Block>(eatBlock(block, false, false, f->condition_section.get(), f->increment_section.get()));
		} else if (t->type == TokenType::DO) {
			eat(TokenType::DO);
			f->body = std::unique_ptr<Block>(eatBlock(block, false, false, f->condition_section.get(), f->increment_section.get()));
			eat(TokenType::END);
		} else {
			auto body = blockInit(block, f->condition_section.get(), false);
			body->add_instruction(eatInstruction(body));
			blockEnd(body, f->increment_section.get());
			f->body = std::unique_ptr<Block>(body);
		}

		f->condition_section->successors.push_back(f->end_section);
		f->end_section->predecessors.push_back(f->condition_section.get());

		f->increment_section->successors.push_back(f->condition_section.get());
		f->condition_section->predecessors.push_back(f->increment_section.get());

		loops.pop_back();
		return f;
	}
}

Instruction* SyntaxicAnalyzer::eatWhile(Block* block) {

	auto while_token = eat_get(TokenType::WHILE);

	auto current_section = block->sections.back().get();

	auto w = new While(env);
	loops.push_back(w);

	w->end_section = new Section(env, "end");

	w->token = while_token;

	bool parenthesis = false;
	bool braces = false;

	if (t->type == TokenType::OPEN_PARENTHESIS) {
		parenthesis = true;
		eat();
	}

	auto condition_expression = eatExpression(block);
	auto condition_instruction = new ExpressionInstruction(env, std::unique_ptr<Value>(condition_expression));
	w->condition = std::make_unique<Block>(env);
	w->condition->add_instruction(std::unique_ptr<Instruction>(condition_instruction));
	w->continue_section = w->condition->sections.front().get();
	w->condition->sections.front()->name = "condition";

	current_section->add_successor(w->condition->sections.front().get());
	w->condition->sections.front()->add_predecessor(current_section);

	if (parenthesis) {
		eat(TokenType::CLOSING_PARENTHESIS);
	}
	if (t->type == TokenType::OPEN_BRACE) {
		braces = true;
	} else {
		eat(TokenType::DO);
	}

	w->body = std::unique_ptr<Block>(eatBlock(block, false, false, w->condition->sections.front().get(), w->condition->sections.front().get()));

	// w->condition_section->add_predecessor(w->body->sections.back());

	w->condition->sections.front()->add_successor(w->end_section);
	w->end_section->add_predecessor(w->condition->sections.front().get());

	if (!braces) {
		eat(TokenType::END);
	}

	// std::cout << "while end_section " << w->end_section << std::endl;

	loops.pop_back();

	return w;
}

Break* SyntaxicAnalyzer::eatBreak(Block* block) {
	Break* b = new Break(env);
	b->token = eat_get(TokenType::BREAK);

	if (t->type == TokenType::NUMBER /*&& t->line == lt->line*/) {
		int deepness = std::stoi(t->content);
		if (deepness <= 0) {
			file->errors.push_back(Error(Error::Type::BREAK_LEVEL_ZERO, ErrorLevel::ERROR, t, {}));
		} else {
			b->deepness = deepness;
			eat();
		}
	}

	if (b->deepness > loops.size()) {
		file->errors.push_back(Error(Error::Type::BREAK_MUST_BE_IN_LOOP, ErrorLevel::ERROR, t, {}));
	} else {
		auto loop = loops.at(loops.size() - b->deepness);

		b->end_section = loop->end_section;
		block->sections.back()->add_successor(loop->end_section);
		loop->end_section->add_predecessor(block->sections.back().get());
	}
	return b;
}

Continue* SyntaxicAnalyzer::eatContinue(Block* block) {
	Continue* c = new Continue(env);
	c->token = eat_get(TokenType::CONTINUE);

	if (t->type == TokenType::NUMBER /*&& t->line == lt->line*/) {
		int deepness = std::stoi(t->content);
		if (deepness <= 0) {
			file->errors.push_back(Error(Error::Type::CONTINUE_LEVEL_ZERO, ErrorLevel::ERROR, t, {}));
		} else {
			c->deepness = deepness;
			eat();
		}
	}
	if (c->deepness > loops.size()) {
		file->errors.push_back(Error(Error::Type::CONTINUE_MUST_BE_IN_LOOP, ErrorLevel::ERROR, t, {}));
	} else {
		auto loop = loops.at(loops.size() - c->deepness);
		auto condition = loop->continue_section;
		c->end_section = condition;
		block->sections.back()->add_successor(condition);
		condition->add_predecessor(block->sections.back().get());
	}
	return c;
}

ClassDeclaration* SyntaxicAnalyzer::eatClassDeclaration(Block* block) {

	eat(TokenType::CLASS);

	auto token = eatIdent();

	auto cd = new ClassDeclaration(env, token);
	eat(TokenType::OPEN_BRACE);

	while (t->type == TokenType::LET) {
		cd->fields.emplace_back(eatVariableDeclaration(block));
	}

	eat(TokenType::CLOSING_BRACE);

	return cd;
}

Token* SyntaxicAnalyzer::eatIdent() {
	return eat_get(TokenType::IDENT);
}

void SyntaxicAnalyzer::eat() {
	eat(TokenType::DONT_CARE);
}

void SyntaxicAnalyzer::eat(TokenType type) {
	eat_get(type);
}

Token* SyntaxicAnalyzer::eat_get() {
	return eat_get(TokenType::DONT_CARE);
}

Token* SyntaxicAnalyzer::eat_get(TokenType type) {

	auto eaten = t;

	last_character = t->location.start.column;
	last_line = t->location.start.line;
	last_size = t->size;
	if (i < file->tokens.size() - 1) {
		t = &file->tokens[++i];
	} else {
		t = &file->finished_token;
	}
	nt = i < file->tokens.size() - 1 ? &file->tokens[i + 1] : nullptr;

	if (type != TokenType::DONT_CARE && eaten->type != type) {
		file->errors.push_back({ Error::Type::UNEXPECTED_TOKEN, ErrorLevel::ERROR, eaten, {eaten->content} });
		// std::cout << "unexpected token : " << to_string((int) type) << " != " << to_string((int) eaten->type) << " (" << eaten->content << ") char " << eaten->location.start.column << std::endl;
		return &file->finished_token;
	}
	return eaten;
}

Token* SyntaxicAnalyzer::nextTokenAt(int pos) {
	if (i + pos < file->tokens.size())
		return &file->tokens[i + pos];
	else
		return &file->finished_token;
}

}
