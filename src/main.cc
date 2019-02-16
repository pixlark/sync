#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h" // Necessary evil

#include "error.cc"
#include "string_builder.cc"
#include "lexer.cc"
#include "parser.cc"

// Threadsafe Job Queue

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

//

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

