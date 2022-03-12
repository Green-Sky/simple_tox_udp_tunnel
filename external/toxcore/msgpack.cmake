set(MSGP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/msgpack-c/")

# orig import START
INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(BIGENDIAN)
IF (BIGENDIAN)
	SET(MSGPACK_ENDIAN_BIG_BYTE 1)
	SET(MSGPACK_ENDIAN_LITTLE_BYTE 0)
ELSE ()
	SET(MSGPACK_ENDIAN_BIG_BYTE 0)
	SET(MSGPACK_ENDIAN_LITTLE_BYTE 1)
ENDIF ()

CONFIGURE_FILE (
	${MSGP_DIR}cmake/sysdep.h.in
	${MSGP_DIR}include/msgpack/sysdep.h
	@ONLY
)

CONFIGURE_FILE (
	${MSGP_DIR}cmake/pack_template.h.in
	${MSGP_DIR}include/msgpack/pack_template.h
	@ONLY
)

# orig import END

add_library(msgpackc STATIC
	${MSGP_DIR}include/msgpack.h

	# conf
	${MSGP_DIR}include/msgpack/sysdep.h
	${MSGP_DIR}include/msgpack/pack_template.h

	${MSGP_DIR}include/msgpack/fbuffer.h
	${MSGP_DIR}include/msgpack/gcc_atomic.h
	${MSGP_DIR}include/msgpack/object.h
	${MSGP_DIR}include/msgpack/pack_define.h
	${MSGP_DIR}include/msgpack/pack.h
	${MSGP_DIR}include/msgpack/sbuffer.h
	${MSGP_DIR}include/msgpack/timestamp.h
	${MSGP_DIR}include/msgpack/unpack_define.h
	${MSGP_DIR}include/msgpack/unpack.h
	${MSGP_DIR}include/msgpack/unpack_template.h
	${MSGP_DIR}include/msgpack/util.h
	${MSGP_DIR}include/msgpack/version.h
	${MSGP_DIR}include/msgpack/version_master.h
	${MSGP_DIR}include/msgpack/vrefbuffer.h
	${MSGP_DIR}include/msgpack/zbuffer.h

	${MSGP_DIR}src/objectc.c
	${MSGP_DIR}src/unpack.c
	${MSGP_DIR}src/version.c
	${MSGP_DIR}src/vrefbuffer.c
	${MSGP_DIR}src/zone.c
)

target_include_directories(msgpackc PUBLIC ${MSGP_DIR}/include)

