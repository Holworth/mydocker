docker: clean main.cc cgroup.cc
	${CXX} -g cgroup.cc main.cc -o docker -std=c++11

clean:
	rm -rf docker
	sudo rm -rf ./centos
