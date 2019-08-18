all:
	gcc -pthread -g -o dinning_deadlock dinning_deadlock.c
	gcc -pthread -g -o abba abba.c
	gcc -shared -fPIC -o ddetector.so ddetector.c -ldl
