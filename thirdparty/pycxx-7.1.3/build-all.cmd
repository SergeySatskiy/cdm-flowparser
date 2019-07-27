if not "%1%2" == "" goto :build_%1_%2

:build_27_32
    call build-unlimited-api.cmd 27 win32 9.0
if not "%1%2" == "" goto :eof

:build_33_32
    call build-unlimited-api.cmd 33 win32 10.0
if not "%1%2" == "" goto :eof

:build_34_32
    call build-unlimited-api.cmd 34 win32 10.0
    call build-limited-api.cmd 34 win32 10.0 3.4
if not "%1%2" == "" goto :eof

:build_35_32
    call build-unlimited-api.cmd 35 win32 14.0
    call build-limited-api.cmd 35 win32 14.0 3.4
    call build-limited-api.cmd 35 win32 14.0 3.5
if not "%1%2" == "" goto :eof

:build_27_64
    call build-unlimited-api.cmd 27 win64 9.0
if not "%1%2" == "" goto :eof

:build_35_64
    call build-unlimited-api.cmd 35 win64 14.0
    call build-limited-api.cmd 35 win64 14.0 3.4
    call build-limited-api.cmd 35 win64 14.0 3.5
if not "%1%2" == "" goto :eof

:build_36_64
    call build-unlimited-api.cmd 36 win64 14.0
    call build-limited-api.cmd 36 win64 14.0 3.4
    call build-limited-api.cmd 36 win64 14.0 3.6
if not "%1%2" == "" goto :eof

:build_37_64
    call build-unlimited-api.cmd 37 win64 14.0
    call build-limited-api.cmd 37 win64 14.0 3.4
    call build-limited-api.cmd 37 win64 14.0 3.7
if not "%1%2" == "" goto :eof

:build_38_64
    call build-unlimited-api.cmd 38 win64 14.0
    call build-limited-api.cmd 38 win64 14.0 3.4
    call build-limited-api.cmd 38 win64 14.0 3.7
if not "%1%2" == "" goto :eof
