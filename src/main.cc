#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "list.h" // Necessary evil

// Unity build
#include "utility.cc"
#include "error.cc"
#include "string_builder.cc"
#include "lexer.cc"
#include "value.cc"
#include "parser.cc"

struct Job {
	Job_Spec * spec;
};

struct Job_Queue_Node {
	Job * job;
	Job_Queue_Node * prev;
};

struct Job_Queue {
	pthread_mutex_t mutex;
	Job_Queue_Node * head = NULL;
	Job_Queue_Node * tail = NULL;
	Job_Queue()
	{
		pthread_mutex_init(&mutex, NULL);
	}
	~Job_Queue()
	{
		pthread_mutex_destroy(&mutex);
	}
	void lock()
	{
		pthread_mutex_lock(&mutex);
	}
	void unlock()
	{
		pthread_mutex_unlock(&mutex);
	}
	void add(Job * job)
	{
		if (!head || !tail) {
			assert(!head && !tail);
			Job_Queue_Node * node = (Job_Queue_Node*) malloc(sizeof(Job_Queue_Node));
			node->job = job;
			node->prev = NULL;
			head = node;
			tail = node;
		} else {
			Job_Queue_Node * node = (Job_Queue_Node*) malloc(sizeof(Job_Queue_Node));
			node->job = job;
			node->prev = NULL;
			tail->prev = node;
			tail = node;
		}
	}
	Job * take()
	{
		assert(!empty());
		Job * ret = head->job;
		head = head->prev;
		if (!head) {
			tail = NULL;
		}
		return ret;
	}
	bool empty()
	{
		if (!head || !tail) {
			assert(!head && !tail);
			return true;
		}
		return false;
	}
};

struct Variable_Space {
	List<const char *> keys;
	List<Value> values;
	void init()
	{
		keys.alloc();
		values.alloc();
	}
	void bind(const char * key, Value value)
	{
		for (int i = 0; i < keys.size; i++) {
			if (strcmp(keys[i], key) == 0) {
				values[i] = value;
				return;
			}
		}
		keys.push(strdup(key));
		values.push(value);
	}
	bool is_bound(const char * key)
	{
		for (int i = 0; i < keys.size; i++) {
			if (strcmp(keys[i], key) == 0) {
				return true;
			}
		}
		return false;
	}
	Value lookup(const char * key)
	{
		for (int i = 0; i < keys.size; i++) {
			if (strcmp(keys[i], key) == 0) {
				return values[i];
			}
		}
		fatal("Tried to lookup unbound variable %s", key);
	}
};

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

struct Assignment {
	const char * symbol;
	Value value;
};

void * scan_and_execute_from_queue(void *);

struct Execution_Context {
	Job_Queue job_queue;
	size_t cpu_count;
	Variable_Space var_space;
	
	void init()
	{
		var_space.init();
		cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
	}
	void run_threads_for_jobs(List<Job*> jobs) {
		job_queue.lock();
		for (int i = 0; i < jobs.size; i++) {
			job_queue.add(jobs[i]);
		}
		job_queue.unlock();
		
		pthread_t * threads = (pthread_t*) malloc(sizeof(pthread_t) * cpu_count);
		List<Assignment> all_assignments;
		all_assignments.alloc();
		
		for (int i = 0; i < cpu_count; i++) {
			pthread_create(threads + i, NULL, scan_and_execute_from_queue, NULL);
		}
		
		for (int i = 0; i < cpu_count; i++) {
			List<Assignment> * assignments;
			pthread_join(threads[i], (void**) &assignments);
			for (int i = 0; i < assignments->size; i++) {
				all_assignments.push(assignments->at(i));
			}
			assignments->dealloc();
		}

		List<const char *> symbols_seen;
		symbols_seen.alloc();
		for (int i = 0; i < all_assignments.size; i++) {
			for (int j = 0; j < symbols_seen.size; j++) {
				if (strcmp(symbols_seen[j], all_assignments[i].symbol) == 0) {
					fatal("Tried to assign to variable '%s' multiple times in one frame",
						  symbols_seen[j]);
				}
			}
			symbols_seen.push(all_assignments[i].symbol);
		}
		symbols_seen.dealloc();

		for (int i = 0; i < all_assignments.size; i++) {
			var_space.bind(all_assignments[i].symbol,
						   all_assignments[i].value);
		}
		
		all_assignments.dealloc();
	}
};

Execution_Context exec_context;

struct VM {
	List<Value> stack;
	List<Command> commands;
	size_t counter = 0;
	void init(List<Command> commands)
	{
		stack.alloc();
		this->commands = commands;
	}
	void dealloc()
	{
		stack.dealloc();
	}
	void execute();
};

void VM::execute()
{
	while (counter < commands.size) {
		Command cmd = commands[counter++];
		switch (cmd.type) {
		case CMD_LOAD_CONST:
			stack.push(cmd.load_const.constant);
			break;
		case CMD_LOOKUP:
			stack.push(exec_context.var_space.lookup(cmd.lookup.symbol));
			break;
		case CMD_UNARY_OP: {
			switch (cmd.unary_op.op) {
			case UNARY_MINUS: {
				Value pop = stack.pop();
				assert(pop.type == VALUE_INTEGER);
				pop.integer *= -1;
				stack.push(pop);
			} break;
			default:
				fatal_internal("Invalid unary operator reached VM::execute()");
			}
		} break;
		case CMD_BINARY_OP: {
			switch (cmd.binary_op.op) {
			case BINARY_PLUS: {
				Value right = stack.pop();
				Value left = stack.pop();
				assert(right.type == VALUE_INTEGER &&
					   left.type == VALUE_INTEGER);
				Value result = Value::make_integer(left.integer + right.integer);
				stack.push(result);
			} break;
			case BINARY_MINUS: {
				Value right = stack.pop();
				Value left = stack.pop();
				assert(right.type == VALUE_INTEGER &&
					   left.type == VALUE_INTEGER);
				Value result = Value::make_integer(left.integer - right.integer);
				stack.push(result);
			} break;
			case BINARY_MULTIPLY: {
				Value right = stack.pop();
				Value left = stack.pop();
				assert(right.type == VALUE_INTEGER &&
					   left.type == VALUE_INTEGER);
				Value result = Value::make_integer(left.integer * right.integer);
				stack.push(result);
			} break;
			case BINARY_DIVIDE: {
				Value right = stack.pop();
				Value left = stack.pop();
				assert(right.type == VALUE_INTEGER &&
					   left.type == VALUE_INTEGER);
				Value result = Value::make_integer(left.integer / right.integer);
				stack.push(result);
			} break;
			default:
				fatal_internal("Invalid binary operator reached VM::execute()");
			}
		} break;
		case CMD_OUTPUT: {
			char * s = stack.pop().to_string();
			printf("%s\n", s);
			free(s);
		} break;
		case CMD_MAKE_TUPLE: {
			size_t length = cmd.make_tuple.length;
			Value * elements = (Value*) malloc(sizeof(Value) * length);
			for (int i = length - 1; i >= 0; i--) {
				elements[i] = stack.pop();
			}
			Value value = Value::with_type(VALUE_TUPLE);
			value.tuple.length = length;
			value.tuple.elements = elements;
			stack.push(value);
		} break;
		default:
			fatal_internal("Invalid instruction reached VM::execute()");
		}
	}
}

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

bool run_job(Job * job, Assignment * assignment)
{
	Compiler compiler;
	compiler.init();
	const char * assign_symbol = job->spec->left;
	compiler.compile_expression(job->spec->right);
	
	VM vm;
	vm.init(compiler.commands);
	vm.execute();
	assert(vm.stack.size == 1);
	Value result = vm.stack.pop();

	vm.dealloc();
	compiler.dealloc();

	if (assign_symbol) {
		*assignment = (Assignment) { assign_symbol, result };
	}
	return (bool) assign_symbol;
}

void * scan_and_execute_from_queue(void *)
{
	List<Assignment> * assignments = (List<Assignment> *) malloc(sizeof(List<Assignment>));
	assignments->alloc();
	while (true) {
		exec_context.job_queue.lock();
		if (exec_context.job_queue.empty()) {
			exec_context.job_queue.unlock();
			break;
		}
		Job * job = exec_context.job_queue.take();
		exec_context.job_queue.unlock();
		Assignment assign;
		if (run_job(job, &assign)) {
			assignments->push(assign);
		}
	}
	return assignments;
}

int main(int argc, char ** argv)
{
	if (argc != 2) {
		printf("Provide one source file\n");
		return 1;
	}
	
	exec_context.init();
	exec_context.var_space.bind("test", Value::make_integer(12));
	
	const char * source = load_string_from_file(argv[1]);
	Lexer lexer(source);
	Parser parser(&lexer);
	
	while (!parser.at_end()) {
		List<Job_Spec*> frame_spec = parser.parse_frame_spec();
		List<Job*> jobs;
		jobs.alloc();
		for (int i = 0; i < frame_spec.size; i++) {
			Job * job = (Job*) malloc(sizeof(Job));
			job->spec = frame_spec[i];
			jobs.push(job);
		}
		exec_context.run_threads_for_jobs(jobs);
	}
	
	return 0;
}

