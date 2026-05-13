CC = gcc
CFLAGS = -Wall -g -Iinclude
SRC_DIR = src
OBJ = $(SRC_DIR)/fat12.o
TARGETS = diskinfo disklist diskget diskput

all: $(TARGETS)

# Link each utility with the shared fat12 logic
diskinfo: $(SRC_DIR)/diskinfo.c $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

disklist: $(SRC_DIR)/disklist.c $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

diskget: $(SRC_DIR)/diskget.c $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

diskput: $(SRC_DIR)/diskput.c $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

# Compile the shared logic into an object file
$(SRC_DIR)/fat12.o: $(SRC_DIR)/fat12.c $(SRC_DIR)/fat12.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/fat12.c -o $@

clean:
	rm -f $(TARGETS) $(SRC_DIR)/*.o