TEMPLATE -= app
CONFIG += console
CONFIG -= app_bundle declarative_debug
CONFIG -= qt
VPATH += ./src
DESTDIR = ./bin
LIBS += -pthread -lglib-2.0 -lzmq -lczmq

INCLUDEPATH += /usr/include/glib-2.0/ /usr/lib64/glib-2.0/include/


CONFIG(release, debug|release) {
//QMAKE_POST_LINK = strip -s $(TARGET)
OBJECTS_DIR = ./obj/release
}

CONFIG(debug, debug|release) {
TARGET = $$join(TARGET,,,d)
OBJECTS_DIR = ./obj/debug
}

QMAKE_LINK = gcc
QMAKE_CFLAGS += -std=gnu11 -Wextra -Werror

QMAKE_CFLAGS_RELEASE += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector -march=native -mtune=native -pthread

QMAKE_LFLAGS += -Wl,--as-needed

SOURCES += main.c

