SHELL=/bin/sh
CPLUS = g++

all: parser

parser: *.cpp *.h
	@${CPLUS} *.cpp -o parser

clean:
	@rm -rf parser
