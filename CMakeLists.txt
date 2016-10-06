cmake_minimum_required(VERSION 3.0.2)
include_directories (".")

# DEPENDS ON:
# ZLIB
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})

#SOURCE SETS
set (TDUTILS_SOURCE
  td/utils/port/Fd.cpp
  td/utils/port/FileFd.cpp
  td/utils/port/IPAddress.cpp
  td/utils/port/path.cpp
  td/utils/port/ServerSocketFd.cpp
  td/utils/port/SocketFd.cpp
  td/utils/port/Stat.cpp
  td/utils/port/detail/Epoll.cpp
  td/utils/port/detail/EventFdBsd.cpp
  td/utils/port/detail/EventFdPosix.cpp
  td/utils/port/detail/EventFdWindows.cpp
  td/utils/port/detail/KQueue.cpp
  td/utils/port/detail/Select.cpp

  td/utils/base64.cpp
  td/utils/buffer.cpp
  td/utils/common.cpp
  td/utils/Gzip.cpp
  td/utils/GzipByteFlow.cpp
  td/utils/Hints.cpp
  td/utils/JsonBuilder.cpp
  td/utils/logging.cpp
  td/utils/MimeType.cpp
  td/utils/Time.cpp
  td/utils/Timer.cpp
  td/utils/tl_parser.cpp
  td/utils/utf8.cpp

  td/utils/port/Clocks.h
  td/utils/port/config.h
  td/utils/port/CxCli.h
  td/utils/port/EventFd.h
  td/utils/port/EventFdBase.h
  td/utils/port/Fd.h
  td/utils/port/FileFd.h
  td/utils/port/IPAddress.h
  td/utils/port/path.h
  td/utils/port/Poll.h
  td/utils/port/PollBase.h
  td/utils/port/RwMutex.h
  td/utils/port/ServerSocketFd.h
  td/utils/port/SocketFd.h
  td/utils/port/Stat.h
  td/utils/port/detail/Epoll.h
  td/utils/port/detail/EventFdBsd.h
  td/utils/port/detail/EventFdPosix.h
  td/utils/port/detail/EventFdWindows.h
  td/utils/port/detail/KQueue.h
  td/utils/port/detail/Select.h
  td/utils/port/detail/WineventPoll.h

  td/utils/base64.h
  td/utils/buffer.h
  td/utils/BufferedFd.h
  td/utils/BufferedReader.h
  td/utils/ByteFlow.h
  td/utils/Closure.h
  td/utils/common.h
  td/utils/FileLog.h
  td/utils/format.h
  td/utils/Gzip.h
  td/utils/GzipByteFlow.h
  td/utils/Heap.h
  td/utils/Hints.h
  td/utils/int_types.h
  td/utils/JsonBuilder.h
  td/utils/logging.h
  td/utils/MemoryLog.h
  td/utils/MimeType.h
  td/utils/misc.h
  td/utils/ObjectPool.h
  td/utils/OptionsParser.h
  td/utils/Parser.h
  td/utils/queue.h
  td/utils/ScopeGuard.h
  td/utils/Slice-decl.h
  td/utils/Slice.h
  td/utils/StackAllocator.h
  td/utils/Status.h
  td/utils/Storer.h
  td/utils/StringBuilder.h
  td/utils/Time.h
  td/utils/Timer.h
  td/utils/tl_parser.h
  td/utils/tl_storer.h
  td/utils/utf8.h
)

#RULES
#LIBRARIES
add_library (tdutils STATIC ${TDUTILS_SOURCE})
if (MSVC)
  target_link_libraries(tdutils PUBLIC ws2_32 Mswsock)
endif()
target_link_libraries(tdutils PUBLIC ${ZLIB_LIBRARIES})
