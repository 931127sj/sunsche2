sunsche : main.o queue.o
	gcc -lm -o sunsche main.o queue.o
main.o : main.c
	gcc -c main.c init.h
queue.o : queue.c
	gcc -c queue.c init.h
