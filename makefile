all: bin/readfits.o bin/readtiff.o bin/stardet.o bin/analyse.o
	gcc -o bin/analyse bin/analyse.o bin/readfits.o bin/readtiff.o bin/stardet.o -lm
bin/analyse.o: src/analyse.c src/stardet/stardet.h src/readfits/readfits.h src/readtiff/readtiff.h
	gcc -o bin/analyse.o -c src/analyse.c -lm
bin/stardet.o: src/stardet/stardet.c src/readfits/readfits.h src/readtiff/readtiff.h
	gcc -o bin/stardet.o -c src/stardet/stardet.c -lm
bin/readfits.o: src/readfits/readfits.c src/readfits/readfits.h src/readtiff/readtiff.h
	gcc -o bin/readfits.o -c src/readfits/readfits.c -lm
bin/readtiff.o: src/readtiff/readtiff.c src/readtiff/readtiff.h
	gcc -o bin/readtiff.o -c src/readtiff/readtiff.c -lm