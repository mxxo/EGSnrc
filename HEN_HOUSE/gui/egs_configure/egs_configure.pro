
###############################################################################
#
#  EGSnrc Qt project file for the egs_configure graphical user interface
#  Copyright (C) 2015 National Research Council Canada
#
#  This file is part of EGSnrc.
#
#  EGSnrc is free software: you can redistribute it and/or modify it under
#  the terms of the GNU Affero General Public License as published by the
#  Free Software Foundation, either version 3 of the License, or (at your
#  option) any later version.
#
#  EGSnrc is distributed in the hope that it will be useful, but WITHOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
#  FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
#  more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with EGSnrc. If not, see <http://www.gnu.org/licenses/>.
#
###############################################################################
#
#  Author:          Ernesto Mainegra-Hing, 2003
#
#  Contributors:    Iwan Kawrakow
#
###############################################################################


SOURCES	+= ../egs_install/src/egs_tests.cpp  \
           ../egs_install/src/egs_tools.cpp  \
           ../egs_install/src/egs_sys_install.cpp \
           ../egs_gui/egs_config_reader.cpp
HEADERS	+= ../egs_install/include/egs_tests.h \
           ../egs_install/include/egs_tools.h \
           ../egs_gui/egs_config_reader.h
######################################################################
# Automatically generated by qmake (1.03a) Mon Oct 28 12:17:25 2002
######################################################################


DEPENDPATH += . ../egs_install/include ../egs_gui

MOC_DIR =       .moc/$$my_machine
OBJECTS_DIR =   .obj/$$my_machine

DESTDIR = ../../bin/$$my_machine

TARGET    = egsconfig

FORMS	= egsnrcmp_setup.ui
TEMPLATE	=lib
CONFIG	+= qt thread warn_off release plugin  $$my_build
win32:DEFINES	+= WIN32

INCLUDEPATH	+= . ../egs_install/include ../egs_gui ../../admin

win32 {
      QMAKE_CFLAGS = -nologo -Zm400
      QMAKE_CXXFLAGS = -nologo -Zm400
}
unix {
    contains( CONFIG, shared ):message( "Dynamic build..." )
    contains( CONFIG, static ){
        message( "Static build ..." )
        CONFIG  += staticlib
    }
}


LANGUAGE	= C++