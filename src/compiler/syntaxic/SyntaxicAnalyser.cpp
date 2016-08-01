#include "SyntaxicAnalyser.hpp"

#include <string>
#include <math.h>

#include "../value/Function.hpp"
#include "../instruction/Instruction.hpp"
#include "../value/Block.hpp"
#include "../instruction/Break.hpp"
#include "../instruction/Foreach.hpp"
#include "../instruction/For.hpp"
#include "../value/If.hpp"
#include "../instruction/Continue.hpp"
#include "../instruction/ExpressionInstruction.hpp"
#include "../instruction/ClassDeclaration.hpp"
#include "../instruction/VariableDeclaration.hpp"
#include "../instruction/Return.hpp"
#include "../instruction/While.hpp"
#include "../value/AbsoluteValue.hpp"
#include "../value/Array.hpp"
#include "../value/ArrayAccess.hpp"
#include "../value/Boolean.hpp"
#include "../value/FunctionCall.hpp"
#include "../value/Nulll.hpp"
#include "../value/Number.hpp"
#include "../value/Object.hpp"
#include "../value/ObjectAccess.hpp"
#include "../value/PostfixExpression.hpp"
#include "../value/PrefixExpression.hpp"
#include "../value/Reference.hpp"
#include "../value/String.hpp"
#include "../value/VariableValue.hpp"
#include "../../vm/Program.hpp"
#include "SyntaxicalError.hpp"
#include "../lexical/Token.hpp"

using namespace std;

namespace ls {

SyntaxicAnalyser::SyntaxicAnalyser() {
	time = 0;
	lt = nullptr;
	nt = nullptr;
	t = nullptr;
	i = 0;
}

SyntaxicAnalyser::~SyntaxicAnalyser() {
	for (SyntaxicalError* error : errors) {
		delete error;
	}
}

Program* SyntaxicAnalyser::analyse(vector<Token>& tokens) {

	this->tokens = tokens;
	this->lt = nullptr;
	this->t = &tokens.at(0);
	this->nt = tokens.size() > 1 ? &tokens.at(1) : nullptr;
	this->i = 0;

	Program* program = new Program();
	program->body = eatMain();

	return program;
}

Block* SyntaxicAnalyser::eatMain() {
	Block* block = new Block();

	while (true) {
		if (t->type == TokenType::FINISHED) {
			eat();
			break;
		} else if (t->type == TokenType::SEMICOLON) {
			eat();
		} else {
			Instruction* ins = eatInstruction();
			if (ins) block->instructions.push_back(ins);
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
bool SyntaxicAnalyser::isObject() {

	if (nt != nullptr and nt->type == TokenType::CLOSING_BRACE) {
		return true;
	}

	auto nnt = nextTokenAt(2);
	if (nt != nullptr and nt->type == TokenType::IDENT
			and nnt != nullptr and nnt->type == TokenType::COLON) {
		return true;
	}
	return false;
}

Value* SyntaxicAnalyser::eatBlockOrObject() {
	if (isObject()) {
		return eatObject();
	} else {
		return eatBlock();
	}
}

Block* SyntaxicAnalyser::eatBlock() {

	Block* block = new Block();

	bool brace = false;
	if (t->type == TokenType::OPEN_BRACE) {
		brace = true;
		eat();
	}

	while (true) {
		if (t->type == TokenType::CLOSING_BRACE) {
			eat();
			if (!brace) {
				errors.push_back(new SyntaxicalError(t, "Unexpected closing brace, forgot to open it ?"));
			}
			break;
		} else if (t->type == TokenType::FINISHED || t->type == TokenType::ELSE || t->type == TokenType::END) {
			if (brace) {
				errors.push_back(new SyntaxicalError(t, "Expecting closing brace at end of the block"));
			}
			break;
		} else if (t->type == TokenType::SEMICOLON) {
			eat();
		} else {
			Instruction* ins = eatInstruction();
			if (ins) block->instructions.push_back(ins);
		}
	}

	return block;
}

Object* SyntaxicAnalyser::eatObject() {

	eat(TokenType::OPEN_BRACE);

	Object* o = new Object();

	while (t->type == TokenType::IDENT) {

		o->keys.push_back(new Ident(eatIdent()));
		eat(TokenType::COLON);
		o->values.push_back(eatExpression());

		if (t->type == TokenType::COMMA) {
			eat();
		}
	}
	eat(TokenType::CLOSING_BRACE);

	return o;
}

Instruction* SyntaxicAnalyser::eatInstruction()
{
	switch (t->type) {

		case TokenType::LET:
		case TokenType::GLOBAL:
			return eatVariableDeclaration();

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
		case TokenType::POWER:
		case TokenType::MODULO:
		case TokenType::PIPE:
		case TokenType::TILDE:
			return new ExpressionInstruction(eatExpression());

		case TokenType::FUNCTION:
			return eatFunctionDeclaration();

		case TokenType::RETURN: {
			eat();
			if (t->type == TokenType::FINISHED or t->type == TokenType::CLOSING_BRACE
				or t->type == TokenType::ELSE or t->type == TokenType::END) {
				return new Return(new Nulll());
			} else {
				return new Return(eatExpression());
			}
		}
		case TokenType::BREAK:
			eat();
			return new Break();

		case TokenType::CONTINUE:
			eat();
			return new Continue();

		case TokenType::CLASS:
			return eatClassDeclaration();

		case TokenType::FOR:
			return eatFor();

		case TokenType::WHILE:
			return eatWhile();

		default:
			errors.push_back(new SyntaxicalError(t, "Unexpected token <" + to_string((int)t->type) + "> (" + t->content + ")"));
			eat();
			return nullptr;
	}
}

VariableDeclaration* SyntaxicAnalyser::eatVariableDeclaration() {

	VariableDeclaration* vd = new VariableDeclaration();

	if (t->type == TokenType::GLOBAL) {
		vd->global = true;
		eat(TokenType::GLOBAL);
	} else {
		eat(TokenType::LET);
	}

	if (t->type == TokenType::NEW) {
		vd->variables.push_back(t);
		eat();
	} else {
		vd->variables.push_back(eatIdent());
	}

	while (t->type == TokenType::COMMA) {

		eat(TokenType::COMMA);

		if (t->type == TokenType::NEW) {
			vd->variables.push_back(t);
			eat();
		} else {
			vd->variables.push_back(eatIdent());
		}
	}

	if (t->type == TokenType::EQUAL) {

		eat(TokenType::EQUAL);

		vd->expressions.push_back(eatExpression());

		while (t->type == TokenType::COMMA) {
			eat();
			vd->expressions.push_back(eatExpression());
		}
	}

	return vd;
}

Function *SyntaxicAnalyser::eatFunction()
{
	if (t->type == TokenType::FUNCTION)
		eat();

	Function* f = new Function();

	eat(TokenType::OPEN_PARENTHESIS);

	while (t->type != TokenType::FINISHED && t->type != TokenType::CLOSING_PARENTHESIS) {

		bool reference = false;
		if (t->type == TokenType::AROBASE) {
			eat();
			reference = true;
		}

		Token* ident = eatIdent();

		Value* defaultValue = nullptr;
		if (t->type == TokenType::EQUAL) {
			eat();
			defaultValue = eatExpression();
		}

		f->addArgument(ident, reference, defaultValue);

		if (t->type == TokenType::COMMA)
			eat();
	}
	eat(TokenType::CLOSING_PARENTHESIS);

	bool braces = false;
	if (t->type == TokenType::OPEN_BRACE) {
		braces = true;
	}

	f->body = eatBlock();

	if (!braces)
		eat(TokenType::END);

	return f;
}

VariableDeclaration* SyntaxicAnalyser::eatFunctionDeclaration() {

	eat(TokenType::FUNCTION);

	VariableDeclaration* vd = new VariableDeclaration();
	vd->global = true;

	vd->variables.push_back(eatIdent());
	vd->expressions.push_back(eatFunction());

	return vd;
}

Value* SyntaxicAnalyser::eatSimpleExpression(bool pipe_opened) {

	Value* e = nullptr;

	if (t->type == TokenType::OPEN_PARENTHESIS) {

		eat();

		if (t->type == TokenType::CLOSING_PARENTHESIS) {
			eat();
			e = new Nulll();
		} else {
			e = eatExpression();
			e->parenthesis = true;
			eat(TokenType::CLOSING_PARENTHESIS);
		}

	} else if (t->type == TokenType::PIPE) {

		eat();
		AbsoluteValue* av = new AbsoluteValue();
		av->expression = eatExpression(true);
		eat(TokenType::PIPE);
		e = new Expression(av);

	} else {

		// Opérateurs unaires préfixe
		if (t->type == TokenType::NEW || t->type == TokenType::MINUS ||
				t->type == TokenType::NOT || t->type == TokenType::MINUS_MINUS ||
				t->type == TokenType::PLUS_PLUS || t->type == TokenType::TILDE) {

			if (t->type == TokenType::MINUS && nt != nullptr && t->line == nt->line) {

				Operator* op = new Operator(eat());
				Value* ex = eatExpression(pipe_opened);
				Expression* expr = dynamic_cast<Expression*>(ex);

				if (expr and expr->op->priority >= op->priority) {
					PrefixExpression* pexp = new PrefixExpression();
					pexp->operatorr = op;
					pexp->expression = expr->v1;
					expr->v1 = pexp;
					e = expr;
				} else {
					PrefixExpression* pe = new PrefixExpression();
					pe->operatorr = op;
					pe->expression = ex;
					e = pe;
				}
			} else {
				PrefixExpression* ex = new PrefixExpression();
				ex->operatorr = new Operator(eat());
				ex->expression = eatSimpleExpression();
				e = new Expression(ex);
			}

		} else {
			e = eatValue();
		}
	}

	while (t->type == TokenType::OPEN_BRACKET || t->type == TokenType::OPEN_PARENTHESIS
		   || t->type == TokenType::DOT) {

		if (t->character != lt->character + lt->size)
			break;

		switch (t->type) {

			case TokenType::OPEN_BRACKET: {

				ArrayAccess* aa = new ArrayAccess();
				eat(TokenType::OPEN_BRACKET);

				aa->array = e;
				aa->key = eatExpression();

				if (t->type == TokenType::COLON) {
					eat();
					aa->key2 = eatExpression();
				}

				eat(TokenType::CLOSING_BRACKET);
				e = aa;
				break;
			}

			case TokenType::OPEN_PARENTHESIS: {

				FunctionCall* fc = new FunctionCall();
				eat(TokenType::OPEN_PARENTHESIS);

				fc->function = e;

				while (t->type != TokenType::FINISHED and t->type != TokenType::CLOSING_PARENTHESIS) {
					fc->arguments.push_back(eatExpression());
					if (t->type == TokenType::COMMA) {
						eat(TokenType::COMMA);
					}
				}
				eat(TokenType::CLOSING_PARENTHESIS);

				e = fc;
				break;
			}

			case TokenType::DOT: {

				ObjectAccess* oa = new ObjectAccess();
				eat(TokenType::DOT);

				oa->object = e;

				if (t->type == TokenType::NEW || t->type == TokenType::CLASS) {
					oa->field = t;
					eat();
				} else if (t->type == TokenType::NUMBER) {
					oa->field = t;
					//					oa->isNumber = true;
					eat();
				} else {
					oa->field = eatIdent();
				}
				e = oa;
				break;
			}

			default:
				break;
		}
	}

	// Opérateurs unaires postfixes
	if (t->type == TokenType::MINUS_MINUS || t->type == TokenType::PLUS_PLUS) {

		if (lt->line == t->line) {

			Token* op = eat();
			PostfixExpression* ex = new PostfixExpression();

			ex->operatorr = new Operator(op);
			ex->expression = (LeftValue*) e;

			e = ex;
		}
	}

	return e;
}

Value* SyntaxicAnalyser::eatExpression(bool pipe_opened) {

	Expression* ex = nullptr;
	Value* e = eatSimpleExpression(pipe_opened);

	// Opérateurs binaires
	while (t->type == TokenType::PLUS || t->type == TokenType::MINUS ||
		   t->type == TokenType::TIMES || t->type == TokenType::DIVIDE ||
		   t->type == TokenType::MODULO || t->type == TokenType::AND ||
		   t->type == TokenType::OR || t->type == TokenType::XOR ||
		   t->type == TokenType::EQUAL || t->type == TokenType::POWER ||
		   t->type == TokenType::DOUBLE_EQUAL || t->type == TokenType::DIFFERENT ||
		   t->type == TokenType::TRIPLE_EQUAL || t->type == TokenType::TRIPLE_DIFFERENT ||
		   t->type == TokenType::GREATER || t->type == TokenType::LOWER ||
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
		   t->type == TokenType::DOUBLE_QUESTION_MARK
		   ) {

		if (t->type == TokenType::MINUS && t->line != lt->line && nt != nullptr && t->line == nt->line)
			break;

		Operator* op = new Operator(t);
		eat();

		if (ex == nullptr) {
			if (Expression* exx = dynamic_cast<Expression*>(e)) {
				ex = exx;
			} else {
				ex = new Expression();
				ex->v1 = e;
			}
		}
		ex->append(op, eatSimpleExpression());
	}

	if (ex != nullptr) {
		return ex;
	}
	return e;
}

Value* SyntaxicAnalyser::eatValue() {

	switch (t->type) {

		case TokenType::PLUS:
		case TokenType::MINUS:
		case TokenType::MODULO:
		case TokenType::TIMES:
		case TokenType::DIVIDE:
		case TokenType::POWER:
		case TokenType::TERNARY:
		{
			VariableValue* v = new VariableValue(t);
			eat();
			return v;
		}

		case TokenType::NUMBER:
		{
			Number* n = new Number(stod(t->content), t);
			eat();
			return n;
		}

		case TokenType::PI:
			eat();
			return new Number(M_PI, t);

		case TokenType::STRING:
		{
			String* v = new String(t->content, t);
			eat();
			return v;
		}

		case TokenType::TRUE:
		case TokenType::FALSE:
		{
			Boolean* bv = new Boolean(t->type == TokenType::TRUE);
			eat();
			return bv;
		}

		case TokenType::NULLL:
			eat();
			return new Nulll();

		case TokenType::IDENT:
		{
			Token* ident = eatIdent();

			switch (t->type) {

				case TokenType::ARROW: {

					Function* l = new Function();
					l->lambda = true;
					l->arguments.push_back(ident);
					eat(TokenType::ARROW);
					l->body = new Block();
					l->body->instructions.push_back(new Return(eatExpression()));

					return l;
				}

				case TokenType::COMMA: {

					// Là y'a une virgule, il faut voir si y'a une flèche
					// plus loin
					bool canBeLamda = true;
					int pos = i;
					while (true) {
						if (tokens.at(pos).type == TokenType::ARROW) {
							break;
						}
						if (tokens.at(pos).type != TokenType::COMMA || tokens.at(pos + 1).type != TokenType::IDENT) {
							canBeLamda = false;
							break;
						}
						pos += 2;
					}

					if (canBeLamda) {

						Function* l = new Function();
						l->lambda = true;
						l->arguments.push_back(ident);
						while (t->type == TokenType::COMMA) {
							eat();
							l->arguments.push_back(eatIdent());
						}

						eat(TokenType::ARROW);
						l->body = new Block();
						l->body->instructions.push_back(new Return(eatExpression()));

						return l;

					} else {
						return new VariableValue(ident);
					}
				}
				default: {
					return new VariableValue(ident);
				}
			}
			break;
		}

		case TokenType::AROBASE:
			eat();
			return new Reference(eatIdent());

		case TokenType::OPEN_BRACKET:
			return eatArray();

		case TokenType::OPEN_BRACE:
			return eatBlockOrObject();

		case TokenType::IF:
			return eatIf();

		case TokenType::FUNCTION:
			return eatFunction();


		case TokenType::ARROW:
		{
			Function* l = new Function();
			l->lambda = true;
			eat(TokenType::ARROW);
			l->body = new Block();
			l->body->instructions.push_back(new Return(eatExpression()));
			return l;
		}

		default:
			break;
	}

	errors.push_back(new SyntaxicalError(t, "Expected value, got <" + to_string((int)t->type) + "> (" + t->content + ")"));
	eat();
	return new Nulll();
}

Array* SyntaxicAnalyser::eatArray() {

	eat(TokenType::OPEN_BRACKET);

	Array* a = new Array();

	// Empty array
	if (t->type == TokenType::CLOSING_BRACKET) {
		eat();
		return a;
	}

	Value* key = nullptr;
	Value* value = eatExpression();

	if (t->type == TokenType::TWO_DOTS) {
		eat();

		a->interval = true;
		a->addValue(value, nullptr);

		Value* value2 = eatExpression();

		a->addValue(value2, nullptr);

		eat(TokenType::CLOSING_BRACKET);

		return a;
	}

	if (t->type == TokenType::COLON) {
		eat(TokenType::COLON);
		key = value;
		value = eatExpression();
	}
	a->addValue(value, key);

	if (t->type == TokenType::COMMA) {
		eat();
	}

	while (t->type != TokenType::CLOSING_BRACKET && t->type != TokenType::FINISHED) {

		value = eatExpression();

		if (t->type == TokenType::COLON) {
			eat(TokenType::COLON);
			key = value;
			value = eatExpression();
		}

		a->addValue(value, key);

		if (t->type == TokenType::COMMA) {
			eat();
		}
	}
	eat(TokenType::CLOSING_BRACKET);

	return a;
}

If* SyntaxicAnalyser::eatIf() {

	If* iff = new If();

	eat(TokenType::IF);

	iff->condition = eatExpression();

	bool braces = false;
	bool then = false;
	if (t->type == TokenType::OPEN_BRACE) {
		braces = true;
	} else if (t->type == TokenType::THEN) {
		eat(TokenType::THEN);
		then = true;
	}

	if (then or braces) {
		iff->then = eatBlockOrObject();
	} else {
		Block* block = new Block();
		block->instructions.push_back(eatInstruction());
		iff->then = block;
	}

	if (then) {
		if (t->type != TokenType::ELSE) {
			eat(TokenType::END);
		}
	}

	if (t->type == TokenType::ELSE) {
		eat();

		bool bracesElse = false;
		if (t->type == TokenType::OPEN_BRACE) {
			bracesElse = true;
		}

		if (then or bracesElse) {
			iff->elze = eatBlockOrObject();
		} else {
			Block* body = new Block();
			body->instructions.push_back(eatInstruction());
			iff->elze = body;
		}

		if (then) {
			eat(TokenType::END);
		}
	}

	return iff;
}

Instruction* SyntaxicAnalyser::eatFor() {

	eat(TokenType::FOR);

	bool parenthesis = false;
	if (t->type == TokenType::OPEN_PARENTHESIS) {
		parenthesis = true;
		eat();
	}

	bool forEach = true;
	bool declare = false;
	if (t->type == TokenType::SEMICOLON) {
		forEach = false;
	} else {
		if (t->type == TokenType::LET) {
			eat();
			declare = true;
		}
		Token* t3 = nextTokenAt(3);
		Token* t4 = nextTokenAt(4);
		forEach = (nt != nullptr && (nt->type == TokenType::COLON || nt->type == TokenType::IN))
				|| (t3 != nullptr && t3->type == TokenType::IN) || (t4 != nullptr && t4->type == TokenType::IN);
	}

	if (forEach) {

		Foreach* f = new Foreach();

		if (nt->type == TokenType::COMMA || nt->type == TokenType::COLON) {
			f->key = eatIdent();
			eat();
		}
		if (t->type == TokenType::LET)
			eat();

		f->value = eatIdent();

		eat(TokenType::IN);

		f->array = eatExpression();

		if (parenthesis)
			eat(TokenType::CLOSING_PARENTHESIS);

		bool braces = false;
		if (t->type == TokenType::OPEN_BRACE) {
			braces = true;
		} else {
			eat(TokenType::DO);
		}

		f->body = eatBlock();

		if (!braces) {
			eat(TokenType::END);
		}

		return f;

	} else {

		For* f = new For();

		while (t->type != TokenType::SEMICOLON && t->type != TokenType::FINISHED) {
			if (t->type == TokenType::LET) {
				eat();
				declare = true;
			}
			f->variables.push_back(eatIdent());
			f->declare_variables.push_back(declare);
			if (t->type == TokenType::EQUAL) {
				eat();
				f->variablesValues.push_back(eatExpression());
			} else {
				f->variablesValues.push_back(nullptr);
			}
			if (t->type == TokenType::COMMA) {
				eat();
			}
			declare = false;
		}

		eat(TokenType::SEMICOLON);

		if (t->type != TokenType::SEMICOLON) {
			f->condition = eatExpression();
		}

		eat(TokenType::SEMICOLON);

		while (parenthesis ? (t->type != TokenType::CLOSING_PARENTHESIS) :
			   (t->type != TokenType::DO && t->type != TokenType::OPEN_BRACE) && t->type != TokenType::FINISHED) {

			if (t->type != TokenType::FINISHED) {
				//				System.out.println(t);
			}
			f->iterations.push_back(eatExpression());
			if (t->type == TokenType::COMMA)
				eat();
		}

		if (parenthesis)
			eat(TokenType::CLOSING_PARENTHESIS);

		bool braces = false;
		if (t->type == TokenType::OPEN_BRACE) {
			braces = true;
		} else {
			eat(TokenType::DO);
		}

		f->body = eatBlock();

		if (!braces) {
			eat(TokenType::END);
		}

		return f;
	}
}

Instruction* SyntaxicAnalyser::eatWhile() {

	eat(TokenType::WHILE);

	While* w = new While();

	bool parenthesis = false;
	bool braces = false;

	if (t->type == TokenType::OPEN_PARENTHESIS) {
		parenthesis = true;
		eat();
	}

	w->condition = eatExpression();

	if (parenthesis) {
		eat(TokenType::CLOSING_PARENTHESIS);
	}
	if (t->type == TokenType::OPEN_BRACE) {
		//eat();
		braces = true;
	} else {
		eat(TokenType::DO);
	}

	w->body = eatBlock();

	if (!braces) {
		eat(TokenType::END);
	}

	return w;
}

ClassDeclaration* SyntaxicAnalyser::eatClassDeclaration() {

	ClassDeclaration* cd = new ClassDeclaration();

	eat(TokenType::CLASS);
	cd->name = eatIdent()->content;
	eat(TokenType::OPEN_BRACE);

	while (t->type == TokenType::LET) {
		cd->fields.push_back(eatVariableDeclaration());
	}

	eat(TokenType::CLOSING_BRACE);

	return cd;
}

Token* SyntaxicAnalyser::eatIdent() {
	return eat(TokenType::IDENT);
}

Token* SyntaxicAnalyser::eat() {
	return eat(TokenType::DONT_CARE);
}

Token* SyntaxicAnalyser::eat(TokenType type) {

	Token* eaten = t;

	lt = t;
	if (i < tokens.size() - 1) {
		t = &tokens.at(++i);
		// System.out.println(">> " + t.content);
	} else {
		t = new Token(TokenType::FINISHED, 0, 0, "");
		// System.out.println(">>>> done.");
	}
	nt = i < tokens.size() - 1 ? &tokens.at(i + 1) : nullptr;

	if (type != TokenType::DONT_CARE && eaten->type != type) {
		errors.push_back(new SyntaxicalError(eaten, "Expected token of type <" + to_string((int)type) + ">, got <" + to_string((int)eaten->type) + "> (" + eaten->content + ")"));
		return new Token(type, 0, 0, "**Error**");
	}
	return eaten;
}

Token* SyntaxicAnalyser::nextTokenAt(int pos) {
	if (i + pos < tokens.size())
		return &tokens.at(i + pos);
	else
		return nullptr;
}

vector<SyntaxicalError*> SyntaxicAnalyser::getErrors() {
	return errors;
}

long SyntaxicAnalyser::getTime() {
	return time;
}

}
