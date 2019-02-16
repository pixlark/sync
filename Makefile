make:
	g++ -g -Iinclude/ src/main.cc -o sync -lpthread

clang:
	clang++ -std=c++11 -g -Iinclude/ src/main.cc -o sync -lpthread
