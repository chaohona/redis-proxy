代理支持多种工作模式，可以起多个进程通过一个端口同时对外提供服务 <br/>
1、twemproxy <br/>
代理可以取代twemproxy，比twemproxy性能更高（接近2倍）  <br/>
一个代理只能管理一个集群 <br/>
2、codis <br/>
代理可以取代codis(TODO) <br/>
3、cluster<br/>
代理可以管理redis原生的集群，支持在线扩缩容 <br/> <br/>


代码目录结构 <br/>
gredis--|src <br/>
			|codis <br/>
			|common <br/>
			|example <br/>
			|proxy <br/>
			|redis <br/>
			|store <br/>
			|thirdparty <br/>
		|conf <br/>
		|lib <br/>
		|bin <br/>
		|shell <br/>
		Makefile <br/>
		README.md <br/>

怎么编译 <br/>
make <br/>	

怎么运行 <br/>
./bin/gredis-proxy -c ./conf/gredis.yml <br/>

yaml解析库为0.6.0版本 <br/>
