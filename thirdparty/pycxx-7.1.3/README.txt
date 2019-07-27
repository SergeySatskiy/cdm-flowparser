Version: 7.1.3 (1-Jul-2019)

Fix for https://sourceforge.net/p/cxx/bugs/43/
memory leak caused by wrong ref count on python3 Py::String objects.

Remove support for supportPrint() etc as the tp_print field is
being removed from python either in 3.8 or 3.9.
