TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    midifile/midifile.c


win32:LIBS += $${_PRO_FILE_PWD_}/libusb/libusb-1.0.a

HEADERS += \
    libusb/libusb.h \
    midifile/midifile.h
