build:
	gcc -Wall -std=c2x -I/usr/local/include/SDL2 -L/usr/local/lib -lSDL2 ./src/*.c -o renderer

run:
	./renderer

clean:
	rm renderer

