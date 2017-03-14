Version: 7.0.1 (29-Aug-2016)

Add extra methods to Py::String that as needed on Windows to support
full unicode range of code points.

On Windows Python defines Py_UNICODE as unsigned short, which is too
small to hold all Unicode values.

PyCXX has  added  to  the  Py::String  API to support creationg from
Py_UCS4  strings  and  converting  Py::String()  into Py::ucs4string
objects.

Fix validate for Bytes to use the correct check function.
