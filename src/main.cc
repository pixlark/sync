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
	void add(Job * job)
	{
		pthread_mutex_lock(&mutex);
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
		pthread_mutex_unlock(&mutex);
	}
	Job * take()
	{
		pthread_mutex_lock(&mutex);
		assert(!empty());
		Job * ret = head->job;
		head = head->prev;
		if (!head) {
			tail = NULL;
		}
		pthread_mutex_unlock(&mutex);
		return ret;
	}
	bool empty()
	{
		pthread_mutex_lock(&mutex);
		if (!head || !tail) {
			assert(!head && !tail);
			pthread_mutex_unlock(&mutex);
			return true;
		}
		pthread_mutex_unlock(&mutex);
		return false;
	}
};

void * scan_and_execute_from_queue(void *);

struct Execution_Context {
	Job_Queue job_queue;
	
	pthread_mutex_t exec_mutex = PTHREAD_MUTEX_INITIALIZER;
	bool execution_completed = false;
	pthread_t * execution_threads;
	
	size_t cpu_count;
	static const bool threaded = false;

	void init()
	{
		if (threaded) {
			// TODO(pixlark): Linux-only
			cpu_count = sysconf(_SC_NPROCESSORS_ONLN);

			execution_threads = (pthread_t*) malloc(sizeof(pthread_t) * cpu_count);
			for (int i = 0; i < cpu_count; i++) {
				pthread_create(execution_threads + i, NULL, scan_and_execute_from_queue, NULL);
			}
		} else {
			scan_and_execute_from_queue(NULL);
		}
	}
};

Execution_Context exec_context;

void * scan_and_execute_from_queue(void *)
{
	printf("%d\n", Execution_Context::threaded);
}

int main(int argc, char ** argv)
{
	if (argc != 2) {
		printf("Provide one source file\n");
		return 1;
	}
	
	exec_context.init();
	
	const char * source = load_string_from_file(argv[1]);
	Lexer lexer(source);
	Parser parser(&lexer);
	
	/*
	while (!parser.at_end()) {
		List<Job_Spec*> frame_spec = parser.parse_frame_spec();
		printf("Frame:\n");
		for (int i = 0; i < frame_spec.size; i++) {
			char * s = frame_spec[i]->to_string();
			printf("  %s\n", s);
			free(s);
		}
	}*/
	
	return 0;
}

