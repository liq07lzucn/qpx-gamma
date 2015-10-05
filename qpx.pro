#-------------------------------------------------------------------------------
#
#  This software was developed at the National Institute of Standards and
#  Technology (NIST) by employees of the Federal Government in the course
#  of their official duties. Pursuant to title 17 Section 105 of the
#  United States Code, this software is not subject to copyright protection
#  and is in the public domain. NIST assumes no responsibility whatsoever for
#  its use by other parties, and makes no guarantees, expressed or implied,
#  about its quality, reliability, or any other characteristic.
# 
#  This software can be redistributed and/or modified freely provided that
#  any derivative works bear some notice that they are derived from it, and
#  any modified versions bear some notice that they have been modified.
#
#  Author(s):
#       Martin Shetty (NIST)
#
#  Description:
#       Project file for qpx-gamma
# 
#-------------------------------------------------------------------------------

TARGET   = qpx
TEMPLATE = app

CONFIG += qt debug_and_release c++11 static

INSTALLS += target icon desktop

BADDIRS = "debug release"

QMAKE_CLEAN += -r $$BADDIRS \
               $$files(qpx*.log) \
               $$files(qpx.pro.user*) \
               PIXIEmsg*.txt \
               install

CONFIG(debug, debug|release) {
   TARGET = $$join(TARGET,,,d)
   DEFINES += "QPX_DBG_"
}


CONFIG -= warn_off warn_on

#not interested in warnings on PLX and XIA code
QMAKE_CFLAGS_DEBUG += -O0 -w
QMAKE_CXXFLAGS_DEBUG += -O0 -Wextra

QMAKE_CFLAGS_RELEASE += -w

QMAKE_CXXFLAGS  += -DBOOST_LOG_DYN_LINK

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

unix {
   SUSE = $$system(cat /proc/version | grep -o SUSE)
   UBUNTU = $$system(cat /proc/version | grep -o Ubuntu)
   ARCH = $$system(uname -m)
   contains( SUSE, SUSE): {
       LIBS += -llua
   }
   contains( UBUNTU, Ubuntu): {
       LIBS += -llua5.2
   }
   DEFINES += "XIA_LINUX"
   DEFINES += "PLX_LINUX"
   !android {
      QMAKE_CC = g++
   }
      
   contains ( ARCH, x86_64): {
      LIBS += PLX/Library/64bit/PlxApi.a
   }

   contains ( ARCH, i686):  {
      LIBS += PLX/Library/32bit/PlxApi.a
   }
   
   LIBS += -lm -ldl -DBOOST_LOG_DYN_LINK \
           -lboost_system -lboost_date_time -lboost_thread -lboost_log \
           -lboost_program_options -lboost_filesystem \
           -lboost_log_setup -lboost_timer -lz

   target.path = /usr/local/bin/
   icon.path = /usr/share/icons/
   desktop.path = /usr/share/applications/
		
   LIBPATH += /usr/local/lib
		
   QMAKE_CFLAGS   += -fpermissive
   QMAKE_CXXFLAGS_RELEASE -= -O2 -std=c++0x
   QMAKE_CXXFLAGS_RELEASE += -O3 -std=c++11
}

android {
   INCLUDEPATH += ../crystax-ndk-10.2.1/sources/boost/1.58.0/include
   equals(ANDROID_TARGET_ARCH, armeabi-v7a) { 
      LIBPATH     += ../crystax-ndk-10.2.1/sources/boost/1.58.0/libs/armeabi-v7a
   }
}
	 
win32 {
    LIBS += dependencies/PLX/PlxApi.lib

		LIBPATH += D:\dev\boost_1_57_0\stage\lib
		
		INCLUDEPATH += D:/dev/boost_1_57_0
		
	   }	


INCLUDEPATH += engine \
               math \
               engine \
               engine/spectrum \
               engine/plugins \
               analysis \
               gui \
               PLX/Include \
               dependencies \
               dependencies/XIA \
               dependencies/xylib \
               dependencies/tinyxml2 \
               dependencies/pugixml \
               dependencies/qcustomplot \
               dependencies/qtcolorpicker \
               dependencies/fityk \
               /usr/include/lua5.2

SOURCES += $$files(dependencies/*.cpp) \
           $$files(dependencies/XIA/*.c) \
           $$files(dependencies/qcustomplot/*.cpp) \
           $$files(dependencies/qtcolorpicker/*.cpp) \
           $$files(dependencies/tinyxml2/*.cpp) \
           $$files(dependencies/pugixml/*.cpp) \
           $$files(dependencies/xylib/*.cpp) \
           $$files(dependencies/fityk/*.cpp) \
           $$files(dependencies/fityk/swig/*.cpp) \
           $$files(dependencies/fityk/cmpfit/*.c) \
           $$files(gui/*.cpp) \
           $$files(engine/*.cpp) \
           $$files(engine/spectrum/*.cpp) \
           $$files(engine/plugins/*.cpp) \
           $$files(analysis/*.cpp) \
           $$files(math/*.cpp)

HEADERS  += $$files(dependencies/*.h) \
            $$files(dependencies/XIA/*.h) \
            $$files(dependencies/qcustomplot/*.h) \
            $$files(dependencies/qtcolorpicker/*.h) \
            $$files(dependencies/tinyxml2/*.h) \
            $$files(dependencies/pugixml/*.hpp) \
            $$files(dependencies/xylib/*.h) \
            $$files(dependencies/fityk/*.h) \
            $$files(PLX/Include/*.h) \
            $$files(gui/*.h) \
            $$files(engine/*.h) \
            $$files(engine/spectrum/*.h) \
            $$files(engine/plugins/*.h) \
            $$files(analysis/*.h) \
            $$files(math/*.h)

FORMS   += $$files(forms/*.ui)

RESOURCES += $$files(forms/*.qrc)
