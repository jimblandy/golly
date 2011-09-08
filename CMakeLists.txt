cmake_minimum_required(VERSION 2.6)

project( golly )

set(SOURCES
   bigint.cpp generationsalgo.cpp ghashbase.cpp
   ghashdraw.cpp hlifealgo.cpp hlifedraw.cpp jvnalgo.cpp lifealgo.cpp
   lifepoll.cpp liferender.cpp liferules.cpp qlifealgo.cpp qlifedraw.cpp
   rd_algo.cpp readpattern.cpp ruletable_algo.cpp ruletreealgo.cpp
   util.cpp viewport.cpp writepattern.cpp
   
   bigint.h generationsalgo.h ghashbase.h hlifealgo.h jvnalgo.h lifealgo.h
   lifepoll.h liferender.h liferules.h platform.h qlifealgo.h rd_algo.h
   readpattern.h ruletable_algo.h ruletreealgo.h util.h viewport.h
   writepattern.h
)

set(WX_SOURCES
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

# on Mac OS X it's better to use locally installed wxWidgets headers and libs
# (the pre-installed wx stuff tends to be out of date; eg. 10.6 has wxMac 2.8.8 and it's a 32-bit debug build)
if(APPLE)
   set( wxWidgets_CONFIG_EXECUTABLE /usr/local/bin/wx-config )
   # Golly doesn't use wxrc but no harm in leaving this in
   set( wxWidgets_wxrc_EXECUTABLE /usr/local/bin/wxrc )
endif(APPLE)

find_package( PerlLibs REQUIRED )
find_package( PythonLibs REQUIRED )
find_package( wxWidgets REQUIRED html net adv core base )
find_package( ZLIB REQUIRED )

include_directories( 
   ${PERL_INCLUDE_PATH}
   ${PYTHON_INCLUDE_PATH}
   ${ZLIB_INCLUDE_DIR}
)
include( ${wxWidgets_USE_FILE} )

link_libraries( 
   ${PERL_LIBRARY}
   ${PYTHON_LIBRARIES}
   ${wxWidgets_LIBRARIES} 
   ${ZLIB_LIBRARIES} 
)

# pass extra settings to the compiler
add_definitions(-DZLIB -DVERSION=2.3b2)

# put the executables in the src folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable( golly 
   WIN32
   MACOSX_BUNDLE   # need this to build a properly bundled Mac app
   ${SOURCES}
   ${WX_SOURCES}
   ${RESOURCES} 
)

add_executable( bgolly
   bgolly.cpp
   ${SOURCES}
)

### AKT: need to fix up VERSION info in Info.plist.in file, and add the various *.icns files!!!
### Or maybe it is better/easier to create Info.plist on the fly???
if(APPLE)
   set_target_properties( golly PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in )
endif(APPLE)

add_executable( RuleTableToTree 
   RuleTableToTree.cpp 
   ${SOURCES}
) 

# hack to get around the "Debug" and "Release" folders Visual Studio adds on Windows
# http://stackoverflow.com/questions/543203/cmake-runtime-output-directory-on-windows
if(MSVC_IDE)
   set_target_properties( golly bgolly RuleTableToTree PROPERTIES PREFIX "../" )
endif(MSVC_IDE)
