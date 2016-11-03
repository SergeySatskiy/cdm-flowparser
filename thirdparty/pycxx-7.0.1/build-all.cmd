if not "%1%2" == "" goto :build_%1_%2

:build_27_32
    setlocal
    call "%LOCALAPPDATA%\Programs\Common\Microsoft\Visual C++ for Python\9.0\vcvarsall.bat" x86
    if exist c:\python27.win32\python.exe (
        c:\python27.win32\python setup_makefile.py win32 win32.mak
        nmake -f win32.mak clean all 2>&1 | c:\unxutils\tee tmp-win32-python27-build.log
        nmake -f win32.mak test 2>&1 | c:\unxutils\tee tmp-win32-python27-test.log
    )
    endlocal
if not "%1%2" == "" goto :eof

:build_33_32
    setlocal
    call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
    if exist c:\python33.win32\python.exe (
        c:\python33.win32\python setup_makefile.py win32 win32.mak
        nmake -f win32.mak clean all 2>&1 | c:\unxutils\tee tmp-win32-python33-build.log
        nmake -f win32.mak test 2>&1 | c:\unxutils\tee tmp-win32-python33-test.log
    )
    endlocal
if not "%1%2" == "" goto :eof

:build_34_32
    setlocal
    call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
    if exist c:\python34.win32\python.exe (
        c:\python34.win32\python setup_makefile.py win32 win32.mak
        nmake -f win32.mak clean all 2>&1 | c:\unxutils\tee tmp-win32-python34-build.log
        nmake -f win32.mak test 2>&1 | c:\unxutils\tee tmp-win32-python34-test.log
    )
    endlocal
if not "%1%2" == "" goto :eof

:build_35_32
    setlocal
    call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
    if exist c:\python35.win32\python.exe (
        c:\python35.win32\python setup_makefile.py win32 win32.mak
        nmake -f win32.mak clean all 2>&1 | c:\unxutils\tee tmp-win32-python35-build.log
        nmake -f win32.mak test 2>&1 | c:\unxutils\tee tmp-win32-python35-test.log
    )
    endlocal
if not "%1%2" == "" goto :eof

:build_27_64
    setlocal
    call "%LOCALAPPDATA%\Programs\Common\Microsoft\Visual C++ for Python\9.0\vcvarsall.bat" x64
    if exist c:\python27.win64\python.exe (
        c:\python27.win64\python setup_makefile.py win64 win64.mak
        nmake -f win64.mak clean all 2>&1 | c:\unxutils\tee tmp-win64-python27-build.log
        nmake -f win64.mak test 2>&1 | c:\unxutils\tee tmp-win64-python27-test.log
    )
    endlocal
if not "%1%2" == "" goto :eof

:build_35_64
    setlocal
    call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
    if exist c:\python35.win64\python.exe (
        c:\python35.win64\python setup_makefile.py win64 win64.mak
        nmake -f win64.mak clean all 2>&1 | c:\unxutils\tee tmp-win64-python35-build.log
        nmake -f win64.mak test 2>&1 | c:\unxutils\tee tmp-win64-python35-test.log
    )
    endlocal
if not "%1%2" == "" goto :eof
