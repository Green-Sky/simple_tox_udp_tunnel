cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

# HACK

if(${TOXCORE_HACK})
	#add_library(toxcore SHARED IMPORTED GLOBAL)
	#set_property(TARGET toxcore PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../../tmp_tox_install/lib/libtoxcore.so.2")

	add_library(toxcore STATIC IMPORTED GLOBAL)
	set_property(TARGET toxcore PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../../tmp_tox_install/lib/libtoxcore.a")

	target_include_directories(toxcore INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../../tmp_tox_install/include/")
	# TODO: find Threads and sodium
	target_link_libraries(toxcore INTERFACE pthread sodium)

else()
	add_subdirectory(./toxcore)
endif()

add_subdirectory(./zed_net)

