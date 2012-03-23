cmake_minimum_required(VERSION 2.6)

project( golly )

set( APP_VERSION 2.4b1 )

if(APPLE OR WIN32)
   # app names are usually capitalized on Mac OS X and Windows
   set( APP_NAME Golly )
else()
   # Linux binaries are usually all lowercase
   set( APP_NAME golly )
endif()

# low-level code used in all executables
set(BASE_SOURCES
   bigint.cpp generationsalgo.cpp ghashbase.cpp
   ghashdraw.cpp hlifealgo.cpp hlifedraw.cpp jvnalgo.cpp lifealgo.cpp
   lifepoll.cpp liferender.cpp liferules.cpp qlifealgo.cpp qlifedraw.cpp
   readpattern.cpp ruletable_algo.cpp ruletreealgo.cpp
   util.cpp viewport.cpp writepattern.cpp
   
   bigint.h generationsalgo.h ghashbase.h hlifealgo.h jvnalgo.h lifealgo.h
   lifepoll.h liferender.h liferules.h platform.h qlifealgo.h
   readpattern.h ruletable_algo.h ruletreealgo.h
   util.h viewport.h writepattern.h
)

# high-level GUI code used only in Golly
set(GUI_SOURCES
   wxalgos.cpp wxcontrol.cpp
   wxedit.cpp wxfile.cpp wxgolly.cpp wxhelp.cpp wxinfo.cpp wxlayer.cpp
   wxmain.cpp wxperl.cpp wxprefs.cpp wxpython.cpp wxrender.cpp wxrule.cpp
   wxscript.cpp wxselect.cpp wxstatus.cpp wxtimeline.cpp wxundo.cpp
   wxutils.cpp wxview.cpp
   
   wxalgos.h wxedit.h wxgolly.h wxhelp.h wxinfo.h wxlayer.h
   wxmain.h wxperl.h wxprefs.h wxpython.h wxrender.h wxrule.h wxscript.h
   wxselect.h wxstatus.h wxtimeline.h wxundo.h wxutils.h wxview.h
)

set(RESOURCES
   golly.rc
   appicon.ico appicon16.ico appicon32.ico appicon48.ico appicon.xpm
   Info.plist.in
   app.icns file-mc.icns file-rle.icns
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

# put the executables in the src folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# create base library used by all 3 executables
add_library( gollybase STATIC ${BASE_SOURCES} )

add_executable( ${APP_NAME}
   WIN32
   MACOSX_BUNDLE      # build a properly bundled Mac app
   ${GUI_SOURCES}
   ${RESOURCES} 
)

add_executable( bgolly
   bgolly.cpp
)

add_executable( RuleTableToTree
   RuleTableToTree.cpp 
)

if(APPLE OR WIN32)
   target_link_libraries( ${APP_NAME} gollybase ${wxWidgets_LIBRARIES} ${ZLIB_LIBRARIES} )
   target_link_libraries( bgolly gollybase ${ZLIB_LIBRARIES} )
   target_link_libraries( RuleTableToTree gollybase ${ZLIB_LIBRARIES} )
else()
   # on Linux we statically link zlib because some systems (eg. Ubuntu) have a non-standard zlib that contains gzopen64
   add_library( static_zlib STATIC IMPORTED )
   set_property( TARGET static_zlib PROPERTY IMPORTED_LOCATION /usr/lib/libz.a )
   target_link_libraries( ${APP_NAME} gollybase ${wxWidgets_LIBRARIES} static_zlib )
   target_link_libraries( bgolly gollybase static_zlib )
   target_link_libraries( RuleTableToTree gollybase static_zlib )
endif()

if(APPLE)
   # create Info.plist (using Info.plist.in) and PkgInfo files inside .app bundle
   add_custom_target( app_bundle
      COMMAND sed -e "s/VERSION/${APP_VERSION}/" ${CMAKE_SOURCE_DIR}/Info.plist.in >${CMAKE_SOURCE_DIR}/Golly.app/Contents/Info.plist
      COMMAND echo -n "APPLGoLy" >${CMAKE_SOURCE_DIR}/Golly.app/Contents/PkgInfo
   )
   add_dependencies( ${APP_NAME} app_bundle )
   
   # copy *.icns files into Resources directory inside .app bundle
   set_source_files_properties( ${CMAKE_SOURCE_DIR}/app.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
   set_source_files_properties( ${CMAKE_SOURCE_DIR}/file-mc.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
   set_source_files_properties( ${CMAKE_SOURCE_DIR}/file-rle.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
   
   # strip out debug info (reduces app size by 1.6MB)
   target_link_libraries( ${APP_NAME} -Wl,-dead_strip -Wl,-S )
   
   # only the Mac app needs to be linked against the Perl and Python libs
   # (on Windows and Linux those libs are dynamically loaded the first time a script is run)
   target_link_libraries( ${APP_NAME} ${PERL_LIBRARY} ${PYTHON_LIBRARIES} )
endif()

# hack to get around the "Debug" and "Release" folders Visual Studio adds on Windows
# http://stackoverflow.com/questions/543203/cmake-runtime-output-directory-on-windows
if(MSVC_IDE)
   set_target_properties( ${APP_NAME} bgolly RuleTableToTree PROPERTIES PREFIX "../" )
endif()
