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
	List<int> values;
	void init()
	{
		keys.alloc();
		values.alloc();
	}
	void bind(const char * key, int value)
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
	int lookup(const char * key)
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
};

struct Command {
	Command_Type type;
	union {
		struct {
			int constant;
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
	};
	static Command with_type(Command_Type type)
	{
		Command cmd;
		cmd.type = type;
		return cmd;
	}
};

void * scan_and_execute_from_queue(void *);

struct Execution_Context {
	Job_Queue job_queue;
	size_t cpu_count;
	Variable_Space var_space;
	
	void init()
	{
		cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
	}
	void run_threads_for_jobs(List<Job*> jobs) {
		job_queue.lock();
		for (int i = 0; i < jobs.size; i++) {
			job_queue.add(jobs[i]);
		}
		job_queue.unlock();
		
		pthread_t * threads = (pthread_t*) malloc(sizeof(pthread_t) * cpu_count);
		for (int i = 0; i < cpu_count; i++) {
			pthread_create(threads + i, NULL, scan_and_execute_from_queue, NULL);
		}
		for (int i = 0; i < cpu_count; i++) {
			pthread_join(threads[i], NULL);
		}
	}
};

Execution_Context exec_context;

struct VM {
	List<int> stack;
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
				int pop = stack.pop();
				pop *= -1;
				stack.push(pop);
			} break;
			default:
				fatal_internal("Invalid unary operator reached VM::execute()");
			}
		} break;
		case CMD_BINARY_OP: {
			switch (cmd.binary_op.op) {
			case BINARY_PLUS: {
				int right = stack.pop();
				int left = stack.pop();
				int result = left + right;
				stack.push(result);
			} break;
			case BINARY_MINUS: {
				int right = stack.pop();
				int left = stack.pop();
				int result = left - right;
				stack.push(result);
			} break;
			case BINARY_MULTIPLY: {
				int right = stack.pop();
				int left = stack.pop();
				int result = left * right;
				stack.push(result);
			} break;
			case BINARY_DIVIDE: {
				int right = stack.pop();
				int left = stack.pop();
				int result = left / right;
				stack.push(result);
			} break;
			default:
				fatal_internal("Invalid binary operator reached VM::execute()");
			}
		} break;
		case CMD_OUTPUT:
			printf("%d\n", stack.pop());
			break;
		default:
			fatal_internal("Invalid instruction reached VM::execute()");
		}
	}
}

void compile_job_spec(List<Command> * commands, Job_Spec * spec)
{
	Command cmd = Command::with_type(CMD_LOAD_CONST);
	cmd.load_const.constant = 15;
	commands->push(cmd);
}

void run_job(Job * job)
{
	List<Command> commands;
	commands.alloc();
	compile_job_spec(&commands, job->spec);
	
	VM vm;
	vm.init(commands);
	vm.execute();
	assert(vm.stack.size == 1);
	int result = vm.stack.pop();
	printf("%d\n", result);

	vm.dealloc();
	commands.dealloc();
}

void * scan_and_execute_from_queue(void *)
{
	while (true) {
		exec_context.job_queue.lock();
		if (exec_context.job_queue.empty()) {
			exec_context.job_queue.unlock();
			break;
		}
		Job * job = exec_context.job_queue.take();
		exec_context.job_queue.unlock();
		run_job(job);
	}
	return NULL;
}

int main(int argc, char ** argv)
{
	if (argc != 2) {
		printf("Provide one source file\n");
		return 1;
	}
	
	exec_context.init();
	exec_context.var_space.bind("test", 12);
	
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

