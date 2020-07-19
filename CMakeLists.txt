cmake_minimum_required(VERSION 3.10)
project(_simple_player)

set(CMAKE_CXX_STANDARD 14)

add_executable(_simple_player main.cpp AudioResampler.cpp VideoScalar.cpp)


target_link_libraries(_simple_player avformat avcodec swscale swresample avutil pthread SDL2 yuv)