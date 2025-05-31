cmake_minimum_required(VERSION 3.28)
project(ChroneX)

include_directories(include)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_CXX_STANDARD 23)

if(UNIX)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined -g3")
    # address sanitizer and thread sanitizer are incompatible and can't be enabled together
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
    # set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

add_executable(ChroneX
    main.cpp
    src/orderbook/Order.cpp
)

target_compile_options(ChroneX PRIVATE -Wall -Werror -Wextra -Wpedantic -Wconversion -Wshadow)

# Testing start
add_subdirectory(testing EXCLUDE_FROM_ALL)
# Testing end

# Packaging start

install(TARGETS ChroneX
    RUNTIME DESTINATION bin
    COMPONENT ChroneX
)

set(CPACK_COMPONENTS_ALL "ChroneX")

set(CPACK_PACKAGE_NAME "ChroneX")
set(CPACK_PACKAGE_VERSION "0.1.0")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB")  # .deb
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CPACK_GENERATOR "NSIS")  # .msi
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CPACK_GENERATOR "DragNDrop")  # .dmg
endif()

include(CPack)

# Packaging end
