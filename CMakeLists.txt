cmake_minimum_required(VERSION 3.11)

project(mypthon)
set(CMAKE_CXX_STANDART 17)

if(CMAKE_SYSTEM_NAME MATCHES "^MINGW")
    set(SYSTEM_LIBS -lstdc++)
else()
    set(SYSTEM_LIBS)
endif()

set(SOURCE_DIR src)

set(MYTHON_FILES ${SOURCE_DIR}/main.cpp ${SOURCE_DIR}/lexer.h ${SOURCE_DIR}/lexer.cpp ${SOURCE_DIR}/parse.h ${SOURCE_DIR}/parse.cpp ${SOURCE_DIR}/runtime.h ${SOURCE_DIR}/runtime.cpp ${SOURCE_DIR}/statement.h ${SOURCE_DIR}/statement.cpp)

add_executable(mython ${MYTHON_FILES})

target_link_libraries(mython ${SYSTEM_LIBS})