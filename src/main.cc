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
#include "collection.cc"
#include "value.cc"
#include "parser.cc"
#include "bytecode.cc"
#include "compiler.cc"
#include "execution.cc"

namespace Collector {
	void mark_value(Value value)
	{
		
	}
	void collect()
	{
		// Reset all allocations to unmarked state
		for (int i = 0; i < allocations.size; i++) {
			allocations[i].mark = false;
		}
		// Go through execution context and mark what you find
		for (int i = 0; i < exec_context.var_space.values.size; i++) {
			mark_value(exec_context.var_space.values[i]);
		}
	}
};

int main(int argc, char ** argv)
{
	if (argc != 2) {
		printf("Provide one source file\n");
		return 1;
	}

	Collector::init();
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
