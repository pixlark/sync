enum Value_Type {
	VALUE_NIL,
	VALUE_INTEGER,
	VALUE_TUPLE,
};

struct Tuple;

struct Value_Tuple {
	size_t length;
	struct Value * elements;
};

struct Value {
	Value_Type type;
	union {
		int integer;
		Value_Tuple tuple;
	};
	static Value with_type(Value_Type type)
	{
		Value value;
		value.type = type;
		return value;
	}
	static Value make_integer(int integer)
	{
		Value value = Value::with_type(VALUE_INTEGER);
		value.integer = integer;
		return value;
	}
	char * to_string()
	{
		switch (type) {
		case VALUE_NIL:
			return strdup("nil");
		case VALUE_INTEGER: {
			char buf[512];
			snprintf(buf, 512, "%d", integer);
			return strdup(buf);
		}
		case VALUE_TUPLE: {
			String_Builder builder;
			builder.append("[");
			for (int i = 0; i < tuple.length; i++) {
				char * s = tuple.elements[i].to_string();
				builder.append(s);
				free(s);
				if (i < tuple.length - 1) builder.append(" . ");
			}
			builder.append("]");
			return builder.final_string();
		}
		default:
			fatal("Value::to_string() type switch incomplete");
		}
	}
};
