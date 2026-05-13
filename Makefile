all: diskinfo disklist diskget diskput

diskinfo: diskinfo.c
	gcc -Wall -o diskinfo diskinfo.c

disklist: disklist.c
	gcc -Wall -o disklist disklist.c

diskget: diskget.c
	gcc -Wall -o diskget diskget.c

diskput: diskput.c
	gcc -Wall -o diskput diskput.c

clean:
	rm -f diskinfo disklist diskget diskput
