main: main.c
	gcc main.c $(shell pkg-config --cflags --libs gtk+-3.0) -o main -export-dynamic

run: main
	./main

clean:
	rm -f main
