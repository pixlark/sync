#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "list.h" // Necessary evil

// Error handling

#define RESET         "\e[0m"
#define SET_BOLD      "\e[1m"
#define SET_DIM       "\e[2m"
#define SET_UNDERLINE "\e[4m"
#define SET_BLINK     "\e[5m"
#define SET_INVERTED  "\e[7m"
#define SET_HIDDEN    "\e[8m"

#define SET_RED "\e[31m"

#define BOLD(x) SET_BOLD x RESET
#define DIM(x) SET_DIM x RESET
#define INVERTED(x) SET_INVERTED x RESET
#define RED(x)  SET_RED x RESET

void fatal(const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	fprintf(stderr, RED(BOLD("encountered error")) ":\n");
	vfprintf(stderr, fmt, args);
	printf("\n");

	va_end(args);
	exit(1);
}

void _fatal_internal(const char * fmt, const char * file, size_t line, ...)
{
	va_list args;
	va_start(args, line);

	fprintf(stderr, INVERTED(RED(BOLD("internal error"))) ":\n");
	fprintf(stderr, DIM("%s:%d") "\n", file, line);
	vfprintf(stderr, fmt, args);
	printf("\n");

	va_end(args);
	exit(1);
}

// TODO(pixlark): GCC-dependent... not really much of a choice though
#define fatal_internal(fmt, ...) _fatal_internal(fmt, __FILE__, __LINE__, ##__VA_ARGS__)

//

// String Builder

struct String_Builder {
	List<char> builder;
	String_Builder();
	~String_Builder();
	void append(const char * s);
	char * final_string();
};

String_Builder::String_Builder()
{
	builder.alloc();
}

String_Builder::~String_Builder()
{
	builder.dealloc();
}

void String_Builder::append(const char * s)
{
	for (int i = 0; i < strlen(s); i++) {
		builder.push(s[i]);
	}
}

char * String_Builder::final_string()
{
	char * str = (char*) malloc(sizeof(char) * (builder.size + 1));
	strncpy(str, builder.arr, builder.size);
	str[builder.size] = '\0';
	return str;
}

//

enum Token_Type {
	TOKEN_EOF = 256,
	TOKEN_PLACEHOLDER,
	TOKEN_SYMBOL,
	TOKEN_INTEGER_LITERAL,
	TOKEN_LEFT_ARROW,
};

struct Token {
	Token_Type type;
	union {
		int integer;
		char * symbol;
	} values;
	static Token eof()
	{
		Token token;
		token.type = TOKEN_EOF;
		return token;
	}
	static Token with_type(Token_Type type)
	{
		Token token;
		token.type = type;
		return token;
	}
	char * to_string();
};

char * itoa(int integer)
{
	char buf[512];
	sprintf(buf, "%d", integer);
	return strdup(buf);
}

char * Token::to_string()
{
	if (type >= 0 && type < 256) {
		char buf[2];
		buf[0] = type;
		buf[1] = '\0';
		return strdup(buf);
	}
	switch (type) {
	case TOKEN_EOF: {
		return strdup("EOF");
	}
	case TOKEN_SYMBOL: {
		return strdup(values.symbol);
	}
	case TOKEN_INTEGER_LITERAL: {
		return itoa(values.integer);
	}
	case TOKEN_LEFT_ARROW: {
		return strdup("<-");
	}
	default:
		fatal_internal("Token::to_string ran on token (%d) without procedure", type);
	}
}

struct Lexer {
	const char * source;
	size_t source_length;
	size_t cursor;
	Lexer(const char * source);
	char next();
	char peek();
	void advance();
	Token next_token();
};

Lexer::Lexer(const char * source)
{
	this->source = source;
	source_length = strlen(source);
	cursor = 0;
}

char Lexer::next()
{
	char c = peek();
	advance();
	return c;
}

char Lexer::peek()
{
	if (cursor >= source_length) {
		return '\0';
	}
	return source[cursor];
}

void Lexer::advance()
{
	cursor++;
}

Token Lexer::next_token()
{
 reset:
	if (peek() == '\0') {
		return Token::eof();
	}
	
	if (isspace(peek())) {
		advance();
		goto reset;
	}

	if (isalpha(peek()) || peek() == '_') {
		char buf[512];
		size_t i = 0;
		while (isalnum(peek()) || peek() == '_') {
			buf[i++] = next();
		}
		buf[i++] = '\0';
		if (strcmp(buf, "_") == 0) {
			return Token::with_type(TOKEN_PLACEHOLDER);
		}
		Token token;
		token.type = TOKEN_SYMBOL;
		token.values.symbol = strdup(buf);
		return token;
	}

	if (isdigit(peek())) {
		char buf[512];
		size_t i = 0;
		while (isdigit(peek())) {
			buf[i++] = next();
		}
		buf[i++] = '\0';
		Token token;
		token.type = TOKEN_INTEGER_LITERAL;
		token.values.integer = strtol(buf, NULL, 10);
		return token;
	}
	
	switch (peek()) {
	case '[':
	case ']':
	case '(':
	case ')':
	case ':':
	case ',':
	case ';':
	case '.':
	case '+':
	case '-':
	case '*':
	case '/':
		return Token::with_type((Token_Type) next());
	case '<': {
		advance();
		if (peek() == '-') {
			advance();
			return Token::with_type(TOKEN_LEFT_ARROW);
		} else {
			return Token::with_type((Token_Type) '<');
		}
	} break;
	}
	
	fatal("Misplaced character %c (%d)", peek(), peek());
}

enum Expr_Type {
	EXPR_INTEGER,
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
		struct {
			int value;
		} integer;
		struct {
			const char * symbol;
		} variable;
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
			return itoa(integer.value);
		case EXPR_VARIABLE:
			return strdup(variable.symbol);
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
	List<Expr*> parse_tuple();
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
		fatal("Parser::expect"); // TODO(pixlark): Fancier error
	}
	return next();
}

Token Parser::weak_expect(Token_Type type)
{
	if (!is(type)) {
		fatal("Parser::expect"); // TODO(pixlark): Fancier error
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
	case TOKEN_INTEGER_LITERAL: {
		Expr * expr = Expr::with_type(EXPR_INTEGER);
		expr->integer.value = expect(TOKEN_INTEGER_LITERAL).values.integer;
		return expr;
	}
	case '(': {
		advance();
		Expr * expr = parse_expression();
		expect((Token_Type) ')');
		return expr;
	}
	default: {
		fatal("Expected some kind of atom, got %s", peek.to_string());
	}
	}	
}

List<Expr*> Parser::parse_tuple()
{
	List<Expr*> tuple;
	tuple.alloc();
	expect((Token_Type) '(');
	while (true) {
		if (is((Token_Type) ')')) {
			advance();
			break;
		}
		tuple.push(parse_expression());
		if (!is((Token_Type) ')')) {
			expect((Token_Type) '.');
		} else {
			if (is((Token_Type) '.')) advance();
		}
	}
	return tuple;
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
			expr->funcall.arguments = parse_tuple();
			return expr;
		} else {
			// Variable
			Expr * expr = Expr::with_type(EXPR_VARIABLE);
			expr->variable.symbol = symbol_tok.values.symbol;
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

char * load_string_from_file(char * path)
{
	FILE * file = fopen(path, "r");
	if (file == NULL) return NULL;
	int file_len = 0;
	while (fgetc(file) != EOF) file_len++;
	char * str = (char*) malloc(file_len + 1);
	str[file_len] = '\0';
	fseek(file, 0, SEEK_SET);
	for (int i = 0; i < file_len; i++) str[i] = fgetc(file);
	fclose(file);
	return str;
}

int main(int argc, char ** argv)
{
	if (argc != 2) {
		printf("Provide one source file\n");
		return 1;
	}

	const char * source = load_string_from_file(argv[1]);
	Lexer lexer(source);
	Parser parser(&lexer);
	
	while (!parser.at_end()) {
		List<Job_Spec*> frame_spec = parser.parse_frame_spec();
		printf("Frame:\n");
		for (int i = 0; i < frame_spec.size; i++) {
			char * s = frame_spec[i]->to_string();
			printf("  %s\n", s);
			free(s);
		}
	}
	
	return 0;
}

