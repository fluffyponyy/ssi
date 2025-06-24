CFLAGS=-Wall -Werror 

ssi: ssi.c
	gcc $(CFLAGS) -o ssi ssi.c