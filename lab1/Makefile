# .PHONY: dist-test dist

to_remove = \
	echoer.o\
	echoer\

all: echoer

echoer: echoer.c
	gcc -o echoer echoer.c

clean:
	rm -f $(to_remove) a.out
