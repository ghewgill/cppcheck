=========
Cppcheck
=========


About

    The original name of this program is "C++check" but it was later changed to "cppcheck".

Manual

    A manual is available online:
    http://cppcheck.sf.net/manual.pdf

Compiling

    Any C++ compiler should work.

    To build the GUI, you need Qt.

    To build the command line tool, no dependencies are required. However for
    the handling of rules, PCRE is needed.

    There are multiple compilation choices:
      * qmake - cross platform build tool
      * Visual Studio - Windows
      * gnu make
      * g++

    qmake
    =====
        You can use the gui/gui.pro file to build the GUI.
            cd gui
            qmake
            make

    Visual Studio
    =============
        Use the cppcheck.sln file.

    gnu make
    ========
        make

    g++ (for experts)
    =================
        If you just want to build Cppcheck without dependencies then you can use this command:
            g++ -o cppcheck -Ilib cli/*.cpp lib/*.cpp

        If you want to use --rule and --rule-file then dependencies are needed:
            g++ -o cppcheck -lpcre -DHAVE_RULES -Ilib -Iexternals cli/*.cpp lib/*.cpp externals/tinyxml/*.cpp
    mingw
    =====
        make LDFLAGS=-lshlwapi

Cross compiling Win32 (CLI) version of Cppcheck in Linux

    sudo apt-get install mingw32
    make CXX=i586-mingw32msvc-g++ LDFLAGS="-lshlwapi"
    mv cppcheck cppcheck.exe

Webpage

    http://cppcheck.sourceforge.net/

