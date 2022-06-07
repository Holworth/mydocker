docker:
	${CXX} -g main.cc -o docker -std=c++17

clean:
	rm -rf docker

