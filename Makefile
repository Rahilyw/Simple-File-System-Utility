CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
TARGETS = diskinfo disklist diskget diskput

all: $(TARGETS)

diskinfo: diskinfo.c
	$(CC) $(CFLAGS) -o diskinfo diskinfo.c

disklist: disklist.c
	$(CC) $(CFLAGS) -o disklist disklist.c

diskget: diskget.c
	$(CC) $(CFLAGS) -o diskget diskget.c

diskput: diskput.c
	$(CC) $(CFLAGS) -o diskput diskput.c

clean:
	rm -f $(TARGETS)