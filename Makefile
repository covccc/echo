target_udp_svr_name = echo_udp_svr
target_tcp_svr_name = echo_tcp_svr
target_tcp_cli_name = echo_tcp_client

udp_svr:echo_udp.cpp
	@g++ -o $(target_udp_svr_name) $^

tcp_svr:EchoTcpServer.cpp EpollManager.cpp main.cpp define.cpp
	@g++ -o $(target_tcp_svr_name) $^

tcp_client:echo_tcp_client.cpp define.cpp
	@g++ -o $(target_tcp_cli_name) $^ -lpthread

clear:
	-@rm $(target_tcp_svr_name) $(target_udp_svr_name) $(target_tcp_cli_name)
