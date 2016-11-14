cmake_minimum_required(VERSION 3.4.1)

file(GLOB golly_SRC
    "../../../gui-common/*.cpp"
    "../../../gui-common/MiniZip/*.c"
    "../../../gollybase/*.cpp"
)

add_library(golly SHARED
    src/main/jni/jnicalls.cpp
    ${golly_SRC})

include_directories(src/main/jni/)
include_directories(../../../gui-common)
include_directories(../../../gui-common/MiniZip)
include_directories(../../../gollybase)

find_library(GLESv1_CM-lib GLESv1_CM )

find_library(log-lib log )

find_library(z-lib z )

find_library(jnigraphics-lib jnigraphics )

target_link_libraries(golly ${GLESv1_CM-lib} )

target_link_libraries(golly ${log-lib} )

target_link_libraries(golly ${z-lib} )

target_link_libraries(golly ${jnigraphics-lib} )
