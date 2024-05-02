CC=gcc
CFLAGS=--std=gnu99
SOURCE=smallsh.c
EXENAME=smallsh
TESTSCRIPT=p3testscript
TESTOUTPUT=testResults
ZIPNAME=grossmry_program3.zip

all:
	$(CC) $(CFLAGS) $(SOURCE)  -o $(EXENAME)

run:
	./$(TESTSCRIPT) > $(TESTOUTPUT) 2>&1

clean:
	rm -f $(EXENAME) $(TESTOUTPUT)

zip:
	zip $(ZIPNAME) $(SOURCE) README.txt