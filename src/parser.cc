enum Expr_Type {
	EXPR_NIL,
	EXPR_INTEGER,
	EXPR_TUPLE,
	EXPR_VARIABLE,
	EXPR_UNARY,
	EXPR_BINARY,
	EXPR_FUNCALL,
};

enum Unary_Op {
	UNARY_MINUS,
};

enum Binary_Op {
	BINARY_PLUS,
	BINARY_MINUS,
	BINARY_MULTIPLY,
	BINARY_DIVIDE,
};

struct Expr {
	Expr_Type type;
	union {
		int integer;
		List<Expr*> tuple;
		const char * variable;
		struct {
			Unary_Op op;
			Expr * expr;
		} unary;
		struct {
			Binary_Op op;
			Expr * left;
			Expr * right;
		} binary;
		struct {
			const char * symbol;
			List<Expr*> arguments;
		} funcall;
	};
	static Expr * with_type(Expr_Type type)
	{
		Expr * expr = (Expr*) malloc(sizeof(Expr));
		expr->type = type;
		return expr;
	}
	char * to_string()
	{
		switch (type) {
		case EXPR_INTEGER:
			return itoa(integer);
		case EXPR_VARIABLE:
			return strdup(variable);
		case EXPR_UNARY: {
			String_Builder builder;
			builder.append("(");
			switch (unary.op) {
			case UNARY_MINUS:
				builder.append("-");
				break;
			default:
				fatal_internal("Expr::to_string for unary expressions is incomplete");
			}
			char * expr_s = unary.expr->to_string();
			builder.append(expr_s);
			free(expr_s);
			builder.append(")");
			return builder.final_string();
		}
		case EXPR_BINARY: {
			String_Builder builder;
			builder.append("(");
			{
				char * left_s = binary.left->to_string();
				builder.append(left_s);
				free(left_s);
			}
			switch (binary.op) {
			case BINARY_PLUS:
				builder.append("+");
				break;
			case BINARY_MINUS:
				builder.append("-");
				break;
			case BINARY_MULTIPLY:
				builder.append("*");
				break;
			case BINARY_DIVIDE:
				builder.append("/");
				break;								
			}
			{
				char * right_s = binary.right->to_string();
				builder.append(right_s);
				free(right_s);
			}
			builder.append(")");
			return builder.final_string();
		}
		case EXPR_FUNCALL: {
			String_Builder builder;
			builder.append("(");
			builder.append(funcall.symbol);
			builder.append(": (");
			for (int i = 0; i < funcall.arguments.size; i++) {
				char * s = funcall.arguments[i]->to_string();
				builder.append(s);
				free(s);
				if (i < funcall.arguments.size - 1) {
					builder.append(" . ");
				}
			}
			builder.append("))");
			return builder.final_string();
		}
		default: {
			fatal_internal("Expr::to_string ran on expression without procedure");
		}
		}
	}
};

struct Job_Spec {
	const char * left;
	Expr * right;
	char * to_string()
	{
		String_Builder builder;
		builder.append("[");
		if (left) {
			builder.append(left);
		} else {
			builder.append("_");
		}
		builder.append(" <- ");
		{
			char * right_s = right->to_string();
			builder.append(right_s);
			free(right_s);
		}
		builder.append("]");
		return builder.final_string();
	}
};

struct Parser {
	Lexer * lexer;
	Token peek;
	Parser(Lexer * lexer);
	bool is(Token_Type type);
	bool at_end();
	Token next();
	Token expect(Token_Type type);
	Token weak_expect(Token_Type type);
	void advance();
	Expr * parse_atom();
	Expr * parse_tuple();
	Expr * parse_expression();
	Expr * parse_expr_0();
	Expr * parse_expr_1();
	Expr * parse_expr_2();
	Expr * parse_expr_3();
	Job_Spec * parse_job_spec();
	List<Job_Spec*> parse_frame_spec();
};

Parser::Parser(Lexer * lexer)
{
	this->lexer = lexer;
	this->peek = lexer->next_token();
}

bool Parser::is(Token_Type type)
{
	return peek.type == type;
}

bool Parser::at_end()
{
	return peek.type == TOKEN_EOF;
}

Token Parser::next()
{
	Token t = peek;
	advance();
	return t;
}

Token Parser::expect(Token_Type type)
{
	if (!is(type)) {
		fatal("Expected %s, got %s",
			  Token::type_to_string(type),
			  peek.to_string());
	}
	return next();
}

Token Parser::weak_expect(Token_Type type)
{
	if (!is(type)) {
		fatal("Expected %s, got %s",
			  Token::type_to_string(type),
			  peek.to_string());
	}
	return peek;
}

void Parser::advance()
{
	this->peek = lexer->next_token();
}

Expr * Parser::parse_expression()
{
	return parse_expr_3();
}

Expr * Parser::parse_atom()
{
	switch (peek.type) {
	case TOKEN_NIL: {
		advance();
		return Expr::with_type(EXPR_NIL);
	}
	case TOKEN_INTEGER_LITERAL: {
		Expr * expr = Expr::with_type(EXPR_INTEGER);
		expr->integer = expect(TOKEN_INTEGER_LITERAL).values.integer;
		return expr;
	}
	case '(': {
		advance();
		Expr * expr = parse_expression();
		expect((Token_Type) ')');
		return expr;
	}
	case '[': {
		return parse_tuple();
	}
	default: {
		fatal("Expected some kind of atom, got %s", peek.to_string());
	}
	}	
}

Expr * Parser::parse_tuple()
{
	List<Expr*> tuple;
	tuple.alloc();
	expect((Token_Type) '[');
	while (true) {
		if (is((Token_Type) ']')) {
			advance();
			break;
		}
		tuple.push(parse_expression());
	}
	Expr * expr = Expr::with_type(EXPR_TUPLE);
	expr->tuple = tuple;
	return expr;
}

Expr * Parser::parse_expr_0()
{
	if (is(TOKEN_SYMBOL)) {
		Token symbol_tok = next();
		if (is((Token_Type) ':')) {
			// Function call
			advance();
			Expr * expr = Expr::with_type(EXPR_FUNCALL);
			expr->funcall.symbol = symbol_tok.values.symbol;
			expr->funcall.arguments = parse_tuple()->tuple; // @temporary
			return expr;
		} else {
			// Variable
			Expr * expr = Expr::with_type(EXPR_VARIABLE);
			expr->variable = symbol_tok.values.symbol;
			return expr;
		}
	} else {
		return parse_atom();
	}
}

Expr * Parser::parse_expr_1()
{
	if (is((Token_Type) '-')) {
		Expr * expr = Expr::with_type(EXPR_UNARY);
		expr->unary.op = UNARY_MINUS;
		advance();
		expr->unary.expr = parse_expr_1();
		return expr;
	} else {
		return parse_expr_0();
	}
}

Expr * Parser::parse_expr_2()
{
	Expr * left = parse_expr_1();
	while (is((Token_Type) '*') || is((Token_Type) '/')) {
		Expr * expr = Expr::with_type(EXPR_BINARY);
		switch (peek.type) {
		case '*':
			expr->binary.op = BINARY_MULTIPLY;
			break;
		case '/':
			expr->binary.op = BINARY_DIVIDE;
			break;
		}
		advance();
		expr->binary.left = left;
		expr->binary.right = parse_expr_2();
		left = expr;
	}
	return left;
}

Expr * Parser::parse_expr_3()
{
	Expr * left = parse_expr_2();
	while (is((Token_Type) '+') || is((Token_Type) '-')) {
		Expr * expr = Expr::with_type(EXPR_BINARY);
		switch (peek.type) {
		case '+':
			expr->binary.op = BINARY_PLUS;
			break;
		case '-':
			expr->binary.op = BINARY_MINUS;
			break;
		}
		advance();
		expr->binary.left = left;
		expr->binary.right = parse_expr_3();
		left = expr;
	}
	return left;
}

Job_Spec * Parser::parse_job_spec()
{
	const char * symbol;
	if (is(TOKEN_PLACEHOLDER)) {
		symbol = NULL;
		advance();
	} else {
		weak_expect(TOKEN_SYMBOL);
		symbol = peek.values.symbol;
		advance();
	}
	expect(TOKEN_LEFT_ARROW);
	Expr * right = parse_expression();
	
	Job_Spec * spec = (Job_Spec*) malloc(sizeof(Job_Spec));
	spec->left = symbol;
	spec->right = right;
	return spec;
}

List<Job_Spec*> Parser::parse_frame_spec()
{
	List<Job_Spec*> frame_spec;
	frame_spec.alloc();
	while (true) {
		frame_spec.push(parse_job_spec());
		if (is((Token_Type) ';')) {
			advance();
			break;
		} else if (!is((Token_Type) ',')) {
			fatal("Expected , or ;, got %s", peek.to_string());
		}
		advance();
	}
	return frame_spec;
}
