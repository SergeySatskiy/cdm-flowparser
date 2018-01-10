/*
 * codimension - graphics python two-way code editor and analyzer
 * Copyright (C) 2014-2016  Sergey Satskiy <sergey.satskiy@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Python extension module
 */

#include "cflowparser.hpp"

#include "cflowversion.hpp"
#include "cflowdocs.hpp"
#include "cflowfragmenttypes.hpp"
#include "cflowfragments.hpp"

#include "cflowmodule.hpp"



CDMControlFlowModule::CDMControlFlowModule() :
    Py::ExtensionModule< CDMControlFlowModule >( "cdmcfparser" )
{
    Fragment::initType();
    BangLine::initType();
    EncodingLine::initType();
    Comment::initType();
    CMLComment::initType();
    Docstring::initType();
    Decorator::initType();
    CodeBlock::initType();

    Annotation::initType();
    Argument::initType();
    Function::initType();
    Class::initType();
    Break::initType();
    Continue::initType();
    Return::initType();
    Raise::initType();
    Assert::initType();
    SysExit::initType();
    While::initType();
    For::initType();
    Import::initType();
    ElifPart::initType();
    If::initType();
    With::initType();
    ExceptPart::initType();
    Try::initType();
    ControlFlow::initType();

    // Free functions visible from the module
    add_varargs_method( "getControlFlowFromMemory",
                        &CDMControlFlowModule::getControlFlowFromMemory,
                        GET_CF_MEMORY_DOC );
    add_varargs_method( "getControlFlowFromFile",
                        &CDMControlFlowModule::getControlFlowFromFile,
                        GET_CF_FILE_DOC );


    initialize( MODULE_DOC );

    // Constants visible from the module
    Py::Dict        d( moduleDictionary() );
    d[ "VERSION" ]                  = Py::String( CDM_CF_PARSER_VERSION );
    d[ "CML_VERSION" ]              = Py::String( CML_VERSION_AS_STRING );

    d[ "UNDEFINED_FRAGMENT" ]       = Py::Int( UNDEFINED_FRAGMENT );
    d[ "FRAGMENT" ]                 = Py::Int( FRAGMENT );
    d[ "BANG_LINE_FRAGMENT" ]       = Py::Int( BANG_LINE_FRAGMENT );
    d[ "ENCODING_LINE_FRAGMENT" ]   = Py::Int( ENCODING_LINE_FRAGMENT );
    d[ "COMMENT_FRAGMENT" ]         = Py::Int( COMMENT_FRAGMENT );
    d[ "DOCSTRING_FRAGMENT" ]       = Py::Int( DOCSTRING_FRAGMENT );
    d[ "DECORATOR_FRAGMENT" ]       = Py::Int( DECORATOR_FRAGMENT );
    d[ "CODEBLOCK_FRAGMENT" ]       = Py::Int( CODEBLOCK_FRAGMENT );
    d[ "ANNOTATION_FRAGMENT" ]      = Py::Int( ANNOTATION_FRAGMENT );
    d[ "ARGUMENT_FRAGMENT" ]        = Py::Int( ARGUMENT_FRAGMENT );
    d[ "FUNCTION_FRAGMENT" ]        = Py::Int( FUNCTION_FRAGMENT );
    d[ "CLASS_FRAGMENT" ]           = Py::Int( CLASS_FRAGMENT );
    d[ "BREAK_FRAGMENT" ]           = Py::Int( BREAK_FRAGMENT );
    d[ "CONTINUE_FRAGMENT" ]        = Py::Int( CONTINUE_FRAGMENT );
    d[ "RETURN_FRAGMENT" ]          = Py::Int( RETURN_FRAGMENT );
    d[ "RAISE_FRAGMENT" ]           = Py::Int( RAISE_FRAGMENT );
    d[ "ASSERT_FRAGMENT" ]          = Py::Int( ASSERT_FRAGMENT );
    d[ "SYSEXIT_FRAGMENT" ]         = Py::Int( SYSEXIT_FRAGMENT );
    d[ "WHILE_FRAGMENT" ]           = Py::Int( WHILE_FRAGMENT );
    d[ "FOR_FRAGMENT" ]             = Py::Int( FOR_FRAGMENT );
    d[ "IMPORT_FRAGMENT" ]          = Py::Int( IMPORT_FRAGMENT );
    d[ "ELIF_PART_FRAGMENT" ]       = Py::Int( ELIF_PART_FRAGMENT );
    d[ "IF_FRAGMENT" ]              = Py::Int( IF_FRAGMENT );
    d[ "WITH_FRAGMENT" ]            = Py::Int( WITH_FRAGMENT );
    d[ "EXCEPT_PART_FRAGMENT" ]     = Py::Int( EXCEPT_PART_FRAGMENT );
    d[ "TRY_FRAGMENT" ]             = Py::Int( TRY_FRAGMENT );
    d[ "CML_COMMENT_FRAGMENT" ]     = Py::Int( CML_COMMENT_FRAGMENT );
    d[ "CONTROL_FLOW_FRAGMENT" ]    = Py::Int( CONTROL_FLOW_FRAGMENT );

}


CDMControlFlowModule::~CDMControlFlowModule()
{}


Py::Object
CDMControlFlowModule::getControlFlowFromMemory( const Py::Tuple &  args )
{
    // One or two arguments are expected:
    // - string with the python code - mandatory
    // - bool to serialize or not - optional (default: true)
    if ( args.length() < 1 || args.length() > 2 )
    {
        char    buf[ 32 ];
        sprintf( buf, "%ld", args.length() );
        throw Py::TypeError( "Unexpected number of arguments. "
                             "Expected 1 or 2, received " + std::string( buf ) );
    }

    Py::Object      pythonCode( args[ 0 ] );
    if ( ! pythonCode.isString() )
        throw Py::TypeError( "Unexpected first argument type. "
                             "Expected a string: python code buffer" );

    bool            makeCopy( true );
    if ( args.length() > 1 )
    {
        Py::Object      serialize( args[ 1 ] );
        if ( ! serialize.isBoolean() )
            throw Py::TypeError( "Unexpected second argument type. "
                                 "Expected a boolean: serialize control flow or not" );
        makeCopy = serialize.isTrue();
    }


    Py::String      code( pythonCode );
    size_t          codeSize( code.size() );

    if ( codeSize == 0 )
    {
        ControlFlow *   controlFlow = new ControlFlow();
        return Py::asObject( controlFlow );
    }

    std::string     content;
    content.reserve( codeSize + 2 );

    // By some reasons the python parser is very sensitive to the end of the
    // file. It needs a complete empty line at the end of the content with
    // trailing LF. It is specifically important for trailing comments for a
    // scope. Weird, but there is a simple solution: add two LF at the end of
    // the content unconditionally. It will not harm anyway.
    content = code.as_std_string( "utf-8" ) + "\n\n";

    if ( makeCopy )
    {
        char *      contentCopy = new char[ content.size() + 1 ];
        strncpy( contentCopy, content.c_str(), content.size() + 1 );
        return parseInput( contentCopy, "dummy.py", true );
    }
    return parseInput( content.c_str(), "dummy.py", false );
}


Py::Object
CDMControlFlowModule::getControlFlowFromFile( const Py::Tuple &  args )
{
    // One parameter is expected: python file name
    if ( args.length() != 1 )
    {
        char    buf[ 32 ];
        sprintf( buf, "%ld", args.length() );
        throw Py::TypeError( "getControlFlowFromFile() takes exactly 1 "
                             "argument (" + std::string( buf ) + "given)" );
    }

    Py::Object      fName( args[ 0 ] );
    if ( ! fName.isString() )
        throw Py::TypeError( "getControlFlowFromFile() takes exactly 1 "
                             "argument of string type: python file name" );


    std::string     fileName( Py::String( fName ).as_std_string( "utf-8" ) );
    if ( fileName.empty() )
        throw Py::RuntimeError( "Invalid argument: file name is empty" );

    // Read the whole file
    char *  buffer = NULL;
    FILE *  f;
    f = fopen( fileName.c_str(), "r" );
    if ( f == NULL )
        throw Py::RuntimeError( "Cannot open file " + fileName );

    struct stat     st;
    stat( fileName.c_str(), &st );

    // By some reasons the python parser is very sensitive to the end of the
    // file. It needs a complete empty line at the end of the content with
    // trailing LF. It is specifically important for trailing comments for a
    // scope. Weird, but there is a simple solution: add two LF at the end of
    // the content unconditionally. It will not harm anyway.
    if ( st.st_size > 0 )
    {
        buffer = new char[ st.st_size + 3 ];

        int             elem = fread( buffer, st.st_size, 1, f );

        fclose( f );
        if ( elem != 1 ) {
            delete [] buffer;
            throw Py::RuntimeError( "Cannot read file " + fileName );
        }

        buffer[ st.st_size ] = '\n';
        buffer[ st.st_size + 1 ] = '\n';
        buffer[ st.st_size + 2 ] = '\0';
        return parseInput( buffer, fileName.c_str(), true );
    }

    // File size is zero
    fclose( f );

    ControlFlow *   controlFlow = new ControlFlow();
    return Py::asObject( controlFlow );
}




static CDMControlFlowModule *  CDMControlFlow;

#if PY_MAJOR_VERSION == 2
extern "C" void initcdmcf()
{
    CDMControlFlow = new CDMControlFlowModule;
}

// symbol required for the debug version
extern "C" void initcdmcf_d()
{
    initcdmcf();
}
#else
extern "C" PyObject *  PyInit_cdmcfparser()
{
    CDMControlFlow = new CDMControlFlowModule;
    return CDMControlFlow->module().ptr();
}

// symbol required for the debug version
extern "C" PyObject *  PyInit_cdmcfparser_d()
{
    return PyInit_cdmcfparser();
}

#endif


