all: bin/stardet.o bin/readfits.o bin/main.o bin/conv.o
	gcc -o bin/main bin/main.o bin/readfits.o bin/stardet.o -lm
	gcc -o bin/conv bin/conv.o bin/readfits.o -lm
bin/conv.o: readfits/readfits.h fitstopgm.c
	gcc -o bin/conv.o -c fitstopgm.c -lm
bin/main.o: stardet/stardet.h readfits/readfits.h main.c
	gcc -o bin/main.o -c main.c -lm
bin/readfits.o: readfits/readfits.c readfits/readfits.h
	gcc -o bin/readfits.o -c readfits/readfits.c -lm
bin/stardet.o: stardet/stardet.c readfits/readfits.h
	gcc -o bin/stardet.o -c stardet/stardet.c -lm