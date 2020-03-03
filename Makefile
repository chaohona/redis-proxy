# gredis Makefile
PWD=$(shell pwd)

INC=-I$(PWD)/src/common/
TWEM_INC=-I$(PWD)/src/proxy/twem/
CODIS_INC=-I$(PWD)/src/proxy/codis/
NATIVE_INC=-I$(PWD)/src/proxy/native/
REPLICA_INC=-I$(PWD)/src/proxy/replica/
TINY_INC=-I$(PWD)/src/proxy/tiny/
YAML_INC=-I$(PWD)/src/thirdparty/yaml/include/
THIRD_INC=-I$(PWD)/src/thirdparty/
GLOG_INC=-I$(PWD)/src/thirdparty/glog/src/
PROXY_INC=-I$(PWD)/src/proxy/ $(INC) $(TINY_INC) $(REPLICA_INC) $(TWEM_INC) $(CODIS_INC) $(NATIVE_INC) $(THIRD_INC) $(GLOG_INC) $(YAML_INC)
LIB=-L$(PWD)/lib/ -lyaml-cpp -lpthread
CXX=g++
DEBUG=-g -ggdb -rdynamic
CXXFLAGS=-std=c++11 -Wall $(DEBUG) $(PROXY_INC)
DEPS=$(LIB)

PROXY=$(PWD)/bin/gredis-proxy
STORE=$(PWD)/bin/gredis-store
COMMON_SRC=$(wildcard $(PWD)/src/common/*.cpp)
COMMON_OBJ=$(COMMON_SRC:%.cpp=%.o)
PROXY_SRC=$(wildcard $(PWD)/src/proxy/*.cpp)
PROXY_OBJ=$(PROXY_SRC:%.cpp=%.o)
#proxy目录中除了main函数所在文件proxy.cpp之外的其它文件
PROXY_COMMON=$(subst $(PWD)/src/proxy/proxy.o,,$(PROXY_OBJ))
STORE_SRC=$(wildcard $(PWD)/src/store/*.cpp)
STORE_OBJ=$(STORE_SRC:%.cpp=%.o)
TWEM_SRC=$(wildcard $(PWD)/src/proxy/twem/*.cpp)
TWEM_OBJ=$(TWEM_SRC:%.cpp=%.o)
NATIVE_SRC=$(wildcard $(PWD)/src/proxy/native/*.cpp)
NATIVE_OBJ=$(NATIVE_SRC:%.cpp=%.o)
REPLICA_SRC=$(wildcard $(PWD)/src/proxy/replica/*.cpp)
REPLICA_OBJ=$(REPLICA_SRC:%.cpp=%.o)
TINY_SRC=$(wildcard $(PWD)/src/proxy/tiny/*.cpp)
TINY_OBJ=$(TINY_SRC:%.cpp=%.o)

ALL_DEPS=$(PWD)/lib/libyaml-cpp.a
ALL_BIN=$(PROXY)
all: prepare $(ALL_BIN) $(ALL_DEPS)

proxy: prepare $(PROXY)
store: prepare $(STORE)

#$(COMMON_OBJ): $(COMMON_SRC)
#	$(CXX) $(CXXFLAGS) $(INC) -o $@ -c $<
#$(PROXY_OBJ): $(PROXY_SRC)
#	$(CXX) $(CXXFLAGS) $(INC) -o $@ -c $<
	
$(PROXY): $(PROXY_OBJ) $(TINY_OBJ) $(REPLICA_OBJ) $(COMMON_OBJ) $(TWEM_OBJ) $(NATIVE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^  $(DEPS)

$(STORE): $(STORE_OBJ) $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^  $(DEPS)
	
#below mode is for test#################################
EPOLL=$(PWD)/bin/gredis-epoll
BUFFER=$(PWD)/bin/gredis-msgbuffer
RING=$(PWD)/bin/gredis-ring
PARSE=$(PWD)/bin/gredis-parse
YAML=$(PWD)/bin/gredis-yaml
TIMER=$(PWD)/bin/gredis-timer
LOCK=$(PWD)/bin/gredis-lock
EPOLL_SRC=$(wildcard $(PWD)/src/example/epoll/*.cpp)
EPOLL_OBJ=$(EPOLL_SRC:%.cpp=%.o)
BUFFER_SRC=$(wildcard $(PWD)/src/example/msgbuffer/*.cpp)
BUFFER_OBJ=$(BUFFER_SRC:%.cpp=%.o)
RING_SRC=$(wildcard $(PWD)/src/example/ringbuffer/*.cpp)
RING_OBJ=$(RING_SRC:%.cpp=%.o)
PARSE_SRC=$(wildcard $(PWD)/src/example/redismsg/*.cpp)
PARSE_OBJ=$(PARSE_SRC:%.cpp=%.o)
YAML_SRC=$(wildcard $(PWD)/src/example/yaml/*.cpp)
YAML_OBJ=$(YAML_SRC:%.cpp=%.o)
TIMER_SRC=$(wildcard $(PWD)/src/example/timer/*.cpp)
TIMER_OBJ=$(TIMER_SRC:%.cpp=%.o)
LOCK_SRC=$(wildcard $(PWD)/src/example/lock/*.cpp)
LOCK_OBJ=$(LOCK_SRC:%.cpp=%.o)

test: $(EPOLL) $(BUFFER) $(RING) $(PARSE)

epoll_test: $(EPOLL)
buffer_test: $(BUFFER)
ring_test: $(RING)
parse_test: $(PARSE)
yaml_test: $(YAML)
timer_test: $(TIMER)
lock_test: $(LOCK)

$(EPOLL) : $(EPOLL_OBJ) $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUFFER) : $(BUFFER_OBJ) $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) $(PROXY_INC) -o $@ $^

$(RING) : $(RING_OBJ) $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $^
	
$(PARSE) : $(PROXY_COMMON) $(PARSE_OBJ) $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) $(PROXY_INC) -o $@ $^
	
$(YAML) : $(YAML_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(DEPS)
	
$(TIMER) : $(TIMER_OBJ) $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(DEPS)
	
$(LOCK) : $(LOCK_OBJ) $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(DEPS)
#######################################################

clean:
	$(RM) $(COMMON_OBJ) $(PROXY_OBJ) $(STORE_OBJ) $(TWEM_OBJ) $(NATIVE_OBJ) $(TINY_OBJ)
	$(RM) $(ALL_BIN)
	
prepare:
	if [ ! -d $(PWD)/bin ]; then mkdir -p $(PWD)/bin; fi;
	
release:
	$(RM) -rf release
	@mkdir -pv release/
	@mkdir -pv release/proxy
	@mkdir -pv release/proxy/bin
	@mkdir -pv release/proxy/conf
	@cp -avf bin/* release/proxy/bin
	@cp -avf conf/* release/proxy/conf
	@cp -avf run.sh release
	@cp -avf tiny_run.sh release
	@cp -avf redis release
	@mkdir -pv release/frontboard
	@mkdir -pv release/frontboard/bin
	@mkdir -pv release/frontboard/config
	@cp -avf frontboard/bin/* release/frontboard/bin
	@cp -avf frontboard/config/* release/frontboard/config
	@cp -avf frontboard/dist release/frontboard/dist
	@cp -acv frontboard/run.sh release/frontboard
	@git rev-parse HEAD > release/proxy/version
	@tar zcvf `date +%Y%m%d%H%M`.tar.gz release/