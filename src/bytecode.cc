enum Command_Type {
	CMD_LOAD_CONST,
	CMD_LOOKUP,
	CMD_UNARY_OP,
	CMD_BINARY_OP,
	CMD_OUTPUT,
	CMD_MAKE_TUPLE,
};

struct Command {
	Command_Type type;
	union {
		struct {
			Value constant;
		} load_const;
		struct {
			const char * symbol;
		} lookup;
		struct {
			Unary_Op op;
		} unary_op;
		struct {
			Binary_Op op;
		} binary_op;
		struct {
			size_t length;
		} make_tuple;
	};
	static Command with_type(Command_Type type)
	{
		Command cmd;
		cmd.type = type;
		return cmd;
	}
};
