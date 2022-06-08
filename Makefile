docker: main.cc cgroup.cc
	${CXX} -g cgroup.cc main.cc -o docker -std=c++17

clean:
	rm -rf docker

