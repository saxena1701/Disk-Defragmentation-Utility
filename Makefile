all: defrag

defrag: defrag.c
    gcc -std=c11 -O0 -g -o defrag defrag.c

run: defrag.c
	./defrag ${ARGS}

clean:
	rm -f defrag *.o