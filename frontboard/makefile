APPS = gredis_webserver
BASE = $(PWD)/src/lib

all:install

install:
	@rm -rf make.log;\
	export GOPATH=$(BASE):$(PWD)\
	&& for ser in $(APPS);\
	do \
		echo "\033[32mmake $$ser start... \033[0m";\
		go install -x $$ser...;\
		if [ "$$?" != "0" ]; then\
			echo "\033[31mmake $$ser failed... \033[0m";\
            exit 1;\
        else\
        	echo "\033[32mmake $$ser success... \033[0m";\
        fi;\
	done
	
robot:
	export GOPATH=$(BASE):$(PWD);\
	go install -x robot > makefile.log 2>&1 
	
clean:
	export GOPATH=$(PWD)\
	&& go clean -i ./...\
	&& go clean -i ./src/lib\
	
gz:
	@rm -rf release
	@mkdir -pv release/
	@mkdir -pv release/bin
	@cp -avf run.sh.example release/
	@cp -avf bin/* release/bin
	@svn info>release/ver.txt
	@tar zcvf `date +%Y%m%d%H%M`.tar.gz release/
