struct Compiler {
	List<Command> commands;
	void init()
	{
		commands.alloc();
	}
	void dealloc()
	{
		commands.dealloc();
	}
	void compile_expression(Expr * expr);
};

void Compiler::compile_expression(Expr * expr)
{
	switch (expr->type) {
	case EXPR_NIL: {
		Command cmd = Command::with_type(CMD_LOAD_CONST);
		cmd.load_const.constant = Value::with_type(VALUE_NIL);
		commands.push(cmd);
	} break;
	case EXPR_INTEGER: {
		Value value = Value::make_integer(expr->integer);
		Command cmd = Command::with_type(CMD_LOAD_CONST);
		cmd.load_const.constant = value;
		commands.push(cmd);
	} break;
	case EXPR_TUPLE: {
		Value value = Value::with_type(VALUE_TUPLE);
		for (int i = 0; i < expr->tuple.size; i++) {
			compile_expression(expr->tuple[i]);
		}
		Command cmd = Command::with_type(CMD_MAKE_TUPLE);
		cmd.make_tuple.length = expr->tuple.size;
		commands.push(cmd);
	} break;
	case EXPR_VARIABLE: {
		Command cmd = Command::with_type(CMD_LOOKUP);
		cmd.lookup.symbol = expr->variable;
		commands.push(cmd);
	} break;
	case EXPR_UNARY: {
		compile_expression(expr->unary.expr);
		Command cmd = Command::with_type(CMD_UNARY_OP);
		cmd.unary_op.op = expr->unary.op;
		commands.push(cmd);
	} break;
	case EXPR_BINARY: {
		compile_expression(expr->binary.left);
		compile_expression(expr->binary.right);
		Command cmd = Command::with_type(CMD_BINARY_OP);
		cmd.binary_op.op = expr->binary.op;
		commands.push(cmd);
	} break;
	case EXPR_FUNCALL: {
		// Only built-ins for the moment
		if (strcmp(expr->funcall.symbol, "output") == 0) {
			for (int i = 0; i < expr->funcall.arguments.size; i++) {
				compile_expression(expr->funcall.arguments[i]);
				commands.push(Command::with_type(CMD_OUTPUT));
			}
			Command result = Command::with_type(CMD_LOAD_CONST);
			result.load_const.constant = Value::make_integer(expr->funcall.arguments.size);
			commands.push(result);
		} else {
			fatal("Function %s unbound", expr->funcall.symbol);
		}
	} break;
	default:
		fatal_internal("Compiler::compile_expression() type switch incomplete");
	}
}
