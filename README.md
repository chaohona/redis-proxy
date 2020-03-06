代理支持多种工作模式，可以起多个进程通过一个端口同时对外提供服务 <br/>
1、twemproxy <br/>
代理可以取代twemproxy，比twemproxy性能更高（接近2倍）  <br/>
一个代理只能管理一个集群 <br/>
2、cluster<br/>
代理可以管理redis原生的集群，支持在线扩缩容 <br/> <br/>

支持Redis虚拟化，一个Redis进程支持上千个使用实例。

[测试报告](https://github.com/chaohona/redis-proxy/blob/master/documents/%E6%B5%8B%E8%AF%95%E6%8A%A5%E5%91%8A.md)


怎么使用 <br/>
make <br/>	
make release <br/>
cd release <br/>
sh tiny_run.sh <br/>
启动之后可以通过界面"http://ip:8888/redis"申请，释放，清空Redis <br/>

yaml解析库为0.6.0版本 <br/>


## Redis协议特点
+ 请求与相应的映射关系靠的是顺序的一一对应，没有回调数据，先发送的请求先响应
+ 协议头没有协议的整体字节长度信息，只有一个字符一个字符的解析协议，才能知道协议的边界在哪

## 代码主流程
+ main->GR_Run->GR_MasterProcess(监控进程)
+ main->GR_Run->GR_StartNewChild(启动工作进程)->GR_Proxy::WorkLoop

## GR_Proxy：进程管理类

## GR_Epoll: epoll网络事件管理类
## GR_Event: 网络事件基类，redis和client的连接都继承自它

## GR_AccessMgr: 客户端管理类
+ GR_AccessMgr::CreateListenEvent 启动监听端口
+ GR_AccessMgr::CreateClientEvent 创建客户端连接GR_AccessEvent

## GR_AccessEvent: 客户端连接
+ m_ReadCache 消息接收buffer
+ m_ReadMsg 消息解析器，用于解析收到的redis协议的消息
+ m_pRoute Redis连接管理，客户端消息路由处理类
+ m_pWaitRing 等待响应的请求等待队列
+ GR_AccessEvent::Read->ProcessMsg处理从客户端过来的消息
	+ m_pRoute->Route 将消息路由给对应的Redis
+ GR_AccessEvent::DoReply 处理从redis响应的消息
	+  GR_AccessEvent::Sendv 向客户端发送收到的响应

## GR_RedisEvent：Redis连接
+ m_ReadCache 消息接收buffer
+ m_ReadMsg 消息解析器，用于解析收到的redis协议的消息
+ m_pWaitRing 等待响应的请求等待队列
+ GR_RedisEvent::Sendv 向Redis发送收到的客户端的请求
+ GR_RedisEvent::Read->ProcessMsg 处理从Redis过来的消息
	+ pIdenty->GetReply 将响应返回给客户端
	
## GR_Route：Redis连接管理基类，处理请求到Redis的路由
+ GR_ClusterRoute Redis原生集群的消息路由处理类
+ GR_TwemRoute Twem模式的消息路由处理类
