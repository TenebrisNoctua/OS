luwow_add_library(Luwow.OsLibrary
    NAME os
    MODES SERIAL
    SOURCES
        Library.h
        Library.cpp
)
target_include_directories(Luwow.OsLibrary PRIVATE ${OS_ROOT}/src)
if(WIN32)
    target_compile_definitions(Luwow.OsLibrary PRIVATE NOMINMAX)
    target_link_libraries(Luwow.OsLibrary PRIVATE advapi32 psapi)
endif()
