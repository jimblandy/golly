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

add_executable( golly 
  WIN32 
  ${SOURCES}
  ${WX_SOURCES}
  ${RESOURCES} 
)

add_executable( bgolly
  bgolly.cpp
  ${SOURCES}
)

add_executable( RuleTableToTree 
  RuleTableToTree.cpp 
  ${SOURCES}
) 

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DZLIB -DVERSION=2.3")
