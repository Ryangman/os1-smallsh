CC=gcc
CFLAGS=--std=gnu99
SOURCE=smallsh.c
EXENAME=smallsh

ZIPNAME=grossmry_program3.zip

all:
	$(CC) $(CFLAGS) $(SOURCE)  -o $(EXENAME)
clean:
	rm -f $(EXENAME)

zip:
	zip $(ZIPNAME) $(SOURCE) README.txt