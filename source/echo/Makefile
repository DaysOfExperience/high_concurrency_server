all:echo cli

echo:main.cc
	g++ -o $@ $^ -std=c++11 -lpthread

cli:cli.cc
	g++ -o $@ $^ -std=c++11 -lpthread

.PHONY:clean
clean:
	rm -f echo cli