#pragma once

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

};
