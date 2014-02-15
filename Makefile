hitman: hitman.c main.c
	gcc hitman.c http-parser/http_parser.c sds/sds.c main.c -o hitman
