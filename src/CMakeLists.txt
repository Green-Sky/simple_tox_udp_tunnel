cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(simple_tox_udp_tunnel C CXX)

########################################

add_executable(simple_tox_udp_tunnel
	./simple_tox_udp_tunnel.cpp
)

target_compile_features(simple_tox_udp_tunnel PUBLIC cxx_std_17)

target_link_libraries(simple_tox_udp_tunnel
	toxcore
	zed_net
)

