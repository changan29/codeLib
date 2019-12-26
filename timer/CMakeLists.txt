cmake_minimum_required(VERSION 3.10)
project( timer)

set(CMAKE_CXX_STANDARD 11)

add_executable( timer LinkList.h   Mempool.hpp  Singleton.h  Timer.cpp  Timer.h  TimerMgr.cpp  TimerMgr.h main.cpp  randHelper.h randHelper.cpp)
