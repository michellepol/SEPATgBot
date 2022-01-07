cmake_minimum_required(VERSION 3.15)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

if (NOT build_suffix)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/bin.dbg)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin.rel)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_SOURCE_DIR}/bin.relwithdbg)
else()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin.${build_suffix})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/bin.${build_suffix}.dbg)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin.${build_suffix}.rel)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_SOURCE_DIR}/bin.${build_suffix}.relwithdbg)
endif()

project(ai_methods)

set(Boost_USE_MULTITHREADED ON)

find_package(Boost COMPONENTS system filesystem regex REQUIRED)

SET(GCC_COVERAGE_LINK_FLAGS  "-lboost_regex -D_GLIBCXX_USE_CXX11_ABI=1")
SET(Boost_INCLUDE_DIRS "/home/michellepol/Downloads/boost_1_61_0/boost")

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")

message(STATUS "boost found: ${Boost_FOUND}")
message(STATUS "boost include dir: ${Boost_INCLUDE_DIRS}")
message(STATUS "boost libraries: ${Boost_LIBRARIES}")

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PythonLibs REQUIRED) 

message(STATUS "python include dir: ${PYTHON_INCLUDE_DIRS} ")
message(STATUS "python libraries: ${PYTHON_LIBRARIES} ")

find_package(CURL)

include_directories(/usr/local/include ${OPENSSL_INCLUDE_DIR} ${Boost_INCLUDE_DIRS}  /home/michelle/anaconda3/include/python3.7m)

add_subdirectory(src)
target_link_libraries(ai_methods /usr/local/lib/libTgBot.a ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES} /home/michelle/anaconda3/lib/libpython3.7m.so)


