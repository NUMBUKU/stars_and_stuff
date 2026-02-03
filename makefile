all: bin/stardet.o bin/readfits.o bin/analyse.o
	gcc -o bin/analyse bin/analyse.o bin/readfits.o bin/stardet.o -lm
bin/analyse.o: src/stardet/stardet.h src/readfits/readfits.h src/analyse.c
	gcc -o bin/analyse.o -c src/analyse.c -lm
bin/readfits.o: src/readfits/readfits.c src/readfits/readfits.h
	gcc -o bin/readfits.o -c src/readfits/readfits.c -lm
bin/stardet.o: src/stardet/stardet.c src/readfits/readfits.h
	gcc -o bin/stardet.o -c src/stardet/stardet.c -lm