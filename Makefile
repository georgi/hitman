hitman.o: hitman.c
	gcc -Wall -c hitman.c -o hitman.o 

hitman: hitman.c main.c
	gcc -Wall hitman.c http-parser/http_parser.c sds/sds.c main.c -o hitman
