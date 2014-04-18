TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

VPATH += ./src

INCLUDEPATH += ../../../pureble/include
INCLUDEPATH += ../../../pureble/third_party/build/include/mariadb
QMAKE_LIBDIR += ../../../pureble/bin
QMAKE_RPATHDIR += ../../../pureble/bin

DESTDIR = ./bin
OUT_PWD = ./bin

CONFIG(release, debug|release) {
    OBJECTS_DIR = ./build/release
    MOC_DIR = ./build/release
    RCC_DIR = ./build/release
    UI_DIR = ./build/release
    INCLUDEPATH += ./build/release
    QMAKE_POST_LINK = strip -s $(TARGET)
    LIBS += -lpureble
}

CONFIG(debug, debug|release) {
    TARGET = $$join(TARGET,,,d)
    OBJECTS_DIR = ./build/debug
    MOC_DIR = ./build/debug
    RCC_DIR = ./build/debug
    UI_DIR = ./build/debug
    INCLUDEPATH += ./build/debug
    LIBS += -lpurebled
}


LIBS += -lzmq -lczmq -lsodium -pthread -lstdc++


QMAKE_LINK = gcc
QMAKE_CFLAGS += -std=gnu11 -Wextra -Werror -DMODULE_NAME=\\\"\$\$\(p=\"$@\"; echo \$\${p%.*}\)\\\"

QMAKE_CFLAGS_RELEASE += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector -march=native -mtune=native -flto

QMAKE_LFLAGS += -Wl,--as-needed

SOURCES += main.c \
    src/broker.c

HEADERS += \
    src/broker.h

