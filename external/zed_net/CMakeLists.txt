cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

add_library(zed_net
	./zed_net.h
	./zed_net.c
)

target_include_directories(zed_net PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

