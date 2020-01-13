代理支持多种工作模式
1、twemproxy
代理可以取代twemproxy，比twemproxy性能更高（接近2倍）
一个代理只能管理一个集群
2、codis
代理可以渠道codis
3、cluster
代理可以管理cluster原生的集群


代码目录结构
gredis--|src
			|codis
			|common
			|example
			|proxy
			|redis
			|store
			|thirdparty
		|conf
		|lib
		|bin
		|shell
		Makefile
		README.md

怎么编译
make	

怎么运行
./bin/gredis-proxy -c ./conf/gredis.toml 

yaml解析库为0.6.0版本
