代理支持多种工作模式，可以起多个进程通过一个端口同时对外提供服务 <br/>
1、twemproxy <br/>
代理可以取代twemproxy，比twemproxy性能更高（接近2倍）  <br/>
一个代理只能管理一个集群 <br/>
2、codis <br/>
代理可以取代codis(TODO) <br/>
3、cluster<br/>
代理可以管理redis原生的集群，支持在线扩缩容 <br/> <br/>

[设计方案与测试报告](https://github.com/chaohona/redis-proxy/blob/master/documents/%E6%B5%8B%E8%AF%95%E6%8A%A5%E5%91%8A.md)

公司内部已线上使用 <br/>

怎么编译 <br/>
make <br/>	

怎么运行 <br/>
./bin/gredis-proxy -c ./conf/gredis.yml <br/>

yaml解析库为0.6.0版本 <br/>
