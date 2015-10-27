cmake_minimum_required(VERSION 2.6)

project( golly )

set( APP_VERSION 2.8b1 )

if(APPLE OR WIN32)
   # app names are usually capitalized on Mac OS X and Windows
   set( APP_NAME Golly )
else()
   # Linux binaries are usually all lowercase
   set( APP_NAME golly )
endif()

# low-level code used in all executables
set(BASE_SOURCES
   ../gollybase/bigint.h            ../gollybase/bigint.cpp
   ../gollybase/generationsalgo.h   ../gollybase/generationsalgo.cpp
   ../gollybase/ghashbase.h         ../gollybase/ghashbase.cpp
                                    ../gollybase/ghashdraw.cpp
   ../gollybase/hlifealgo.h         ../gollybase/hlifealgo.cpp
                                    ../gollybase/hlifedraw.cpp
   ../gollybase/jvnalgo.h           ../gollybase/jvnalgo.cpp
   ../gollybase/lifealgo.h          ../gollybase/lifealgo.cpp
   ../gollybase/lifepoll.h          ../gollybase/lifepoll.cpp
   ../gollybase/liferender.h        ../gollybase/liferender.cpp
   ../gollybase/liferules.h         ../gollybase/liferules.cpp
   ../gollybase/platform.h
   ../gollybase/qlifealgo.h         ../gollybase/qlifealgo.cpp
                                    ../gollybase/qlifedraw.cpp
   ../gollybase/readpattern.h       ../gollybase/readpattern.cpp
   ../gollybase/ruleloaderalgo.h    ../gollybase/ruleloaderalgo.cpp
   ../gollybase/ruletable_algo.h    ../gollybase/ruletable_algo.cpp
   ../gollybase/ruletreealgo.h      ../gollybase/ruletreealgo.cpp
   ../gollybase/util.h              ../gollybase/util.cpp
   ../gollybase/viewport.h          ../gollybase/viewport.cpp
   ../gollybase/writepattern.h      ../gollybase/writepattern.cpp
)
include_directories( ../gollybase )

# high-level GUI code used in desktop Golly
set(GUI_SOURCES
   wxalgos.h        wxalgos.cpp
                    wxcontrol.cpp
   wxedit.h         wxedit.cpp
                    wxfile.cpp
   wxgolly.h        wxgolly.cpp
   wxhelp.h         wxhelp.cpp
   wxinfo.h         wxinfo.cpp
   wxlayer.h        wxlayer.cpp
   wxmain.h         wxmain.cpp
   wxperl.h         wxperl.cpp
   wxprefs.h        wxprefs.cpp
   wxpython.h       wxpython.cpp
   wxrender.h       wxrender.cpp
   wxrule.h         wxrule.cpp
   wxscript.h       wxscript.cpp
   wxselect.h       wxselect.cpp
   wxstatus.h       wxstatus.cpp
   wxtimeline.h     wxtimeline.cpp
   wxundo.h         wxundo.cpp
   wxutils.h        wxutils.cpp
   wxview.h         wxview.cpp
)

set(RESOURCES
   golly.rc
   Info.plist.in
   icons/appicon.ico icons/appicon16.ico icons/appicon32.ico icons/appicon48.ico icons/appicon.xpm
   icons/app.icns icons/file-mc.icns icons/file-rle.icns
)

if(APPLE)
   # on Mac OS X it's better to use locally installed wxWidgets headers and libs
   # (the pre-installed stuff tends to be out of date; eg. 10.6 has wxMac 2.8.8 and it's a 32-bit debug build)
   set( wxWidgets_CONFIG_EXECUTABLE /usr/local/bin/wx-config )
   set( wxWidgets_wxrc_EXECUTABLE /usr/local/bin/wxrc )         # not used, but no harm leaving it in
   
   # to avoid statically linking the Perl library into the app we do these 2 steps:
   # 1. override PERL_INCLUDE_PATH to simplified output from "/usr/bin/perl -MExtUtils::Embed -e ccopts"
   # 2. override PERL_LIBRARY to simplified output from "/usr/bin/perl -MExtUtils::Embed -e ldopts"
   set( PERL_INCLUDE_PATH /System/Library/Perl/5.10.0/darwin-thread-multi-2level/CORE )
   set( PERL_LIBRARY "-L/usr/local/lib -L/System/Library/Perl/5.10.0/darwin-thread-multi-2level/CORE -lperl -ldl -lm -lutil -lc" )
elseif(UNIX)
   # remove -rdynamic from link options on Linux to reduce golly size by about 1.2MB
   set( CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "" )
endif()

find_package( wxWidgets REQUIRED html net adv core base )

if(WIN32)
   # use zlib library included in wxWidgets
   set( ZLIB_INCLUDE_DIR ${wxWidgets_ROOT_DIR}/src/zlib )
   set( ZLIB_LIBRARY ${wxWidgets_LIB_DIR}/wxzlib.lib )
   
   # PERL_LIBARY is not used in our Windows build but cmake needs a setting
   set( PERL_LIBRARY lib_not_used )
endif()

find_package( ZLIB REQUIRED )
find_package( PerlLibs REQUIRED )
find_package( PythonLibs REQUIRED )

include_directories( 
   ${ZLIB_INCLUDE_DIR}
   ${PERL_INCLUDE_PATH}
   ${PYTHON_INCLUDE_PATH}
)
include( ${wxWidgets_USE_FILE} )

# pass extra settings to the compiler
add_definitions(-DZLIB -DVERSION=${APP_VERSION})
if(APPLE)
   # support Mac OS 10.6 or later
   add_definitions(-mmacosx-version-min=10.6)
endif()
if(APPLE OR UNIX)
   # use same settings as in makefiles
   add_definitions(-D_LARGE_FILES -O5)
endif()
# workaround for wx2.9.3 wxAssert link errors, see http://trac.wxwidgets.org/ticket/12626
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DwxDEBUG_LEVEL=0")
# avoid security warnings
if(MSVC)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS)
endif()

# set this to true if using Visual Leak Detector to find memory leaks
set(USING_VISUAL_LEAK_DETECTOR FALSE)
if(USING_VISUAL_LEAK_DETECTOR)
  set(VLD_INCLUDE_DIR "C:\Program Files\Visual Leak Detector\include")
  set(VLD_LIBRARIES "C:\Program Files\Visual Leak Detector\lib\Win32\vld.lib")
  add_definitions(/DUSING_VISUAL_LEAK_DETECTOR)
endif()

# put the executables in parent directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/..)

# create base library used by all 3 executables
add_library( gollybase STATIC ${BASE_SOURCES} )

add_executable( ${APP_NAME}
   WIN32
   MACOSX_BUNDLE      # build a properly bundled Mac app
   ${GUI_SOURCES}
   ${RESOURCES} 
)

add_executable( bgolly
   ../cmdline/bgolly.cpp
)

target_link_libraries( ${APP_NAME} gollybase ${wxWidgets_LIBRARIES} ${ZLIB_LIBRARIES} )
target_link_libraries( bgolly gollybase ${ZLIB_LIBRARIES} )

if(APPLE)
   # create Info.plist (using Info.plist.in) and PkgInfo files inside .app bundle
   add_custom_target( app_bundle
      COMMAND sed -e "s/VERSION/${APP_VERSION}/" ${CMAKE_SOURCE_DIR}/Info.plist.in >${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Golly.app/Contents/Info.plist
      COMMAND echo -n "APPLGoLy" >${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Golly.app/Contents/PkgInfo
   )
   add_dependencies( ${APP_NAME} app_bundle )
   
   # copy *.icns files into Resources directory inside .app bundle
   set_source_files_properties( ${CMAKE_SOURCE_DIR}/icons/app.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
   set_source_files_properties( ${CMAKE_SOURCE_DIR}/icons/file-mc.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
   set_source_files_properties( ${CMAKE_SOURCE_DIR}/icons/file-rle.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
   
   # strip out debug info (reduces app size by 1.6MB)
   target_link_libraries( ${APP_NAME} -Wl,-dead_strip -Wl,-S )
   
   # only the Mac app needs to be linked against the Perl and Python libs
   # (on Windows and Linux those libs are dynamically loaded the first time a script is run)
   target_link_libraries( ${APP_NAME} ${PERL_LIBRARY} ${PYTHON_LIBRARIES} )
endif()

# hack to get around the "Debug" and "Release" folders Visual Studio adds on Windows
# http://stackoverflow.com/questions/543203/cmake-runtime-output-directory-on-windows
if(MSVC_IDE)
   set_target_properties( ${APP_NAME} bgolly PROPERTIES PREFIX "../" )
endif()

if(USING_VISUAL_LEAK_DETECTOR)
   include_directories( ${VLD_INCLUDE_DIR} )
   target_link_libraries( ${APP_NAME} ${VLD_LIBRARIES} )
   target_link_libraries( bgolly ${VLD_LIBRARIES} )
endif()

# ensure we link the C runtime statically (N.B. still appears as /MD in the CMake gui but ignore this)
# see: http://www.cmake.org/Wiki/CMake_FAQ#Dynamic_Replace
foreach( var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO )
  string( REGEX REPLACE "/MD" "/MT" ${var} "${${var}}" )
endforeach()
