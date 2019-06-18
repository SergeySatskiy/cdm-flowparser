/*
 * codimension - graphics python two-way code editor and analyzer
 * Copyright (C) 2014 - 2016  Sergey Satskiy <sergey.satskiy@gmail.com>
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
 * Python extension module - control flow fragments
 */

#include <stdlib.h>

#include <string>
#include <limits>

#include "cflowfragments.hpp"
#include "cflowfragmenttypes.hpp"
#include "cflowversion.hpp"
#include "cflowdocs.hpp"
#include "cflowutils.hpp"


// small helper functions

void throwWrongBufArgument( const char *  funcName )
{
    throw Py::RuntimeError( "Unexpected number of arguments. " +
                            std::string( funcName ) +
                            "() supports no arguments or one argument "
                            "(text buffer)" );
}


// Convenience macros
#define GETINTATTR( member )                                   \
    do { if ( strcmp( attrName, CDM_CF_STR( member ) ) == 0 )  \
         { retval = PYTHON_INT_TYPE( member );                 \
           return true; } } while ( 0 )
#define GETBOOLATTR( member )                                  \
    do { if ( strcmp( attrName, CDM_CF_STR( member ) ) == 0 )  \
         { retval = Py::Boolean( member );                     \
           return true; } } while ( 0 )



#define TOFRAGMENT( member )                \
    (static_cast<Fragment *>(member.ptr()))


static std::string
representFragmentPart( const Py::Object &  value,
                       const char *        name )
{
    if ( value.isNone() )
        return std::string( name ) + ": None";
    return std::string( name ) + ": " + TOFRAGMENT( value )->as_string();
}

static std::string
representList( const Py::List &  lst,
               const char *      name )
{
    if ( lst.size() == 0 )
        return std::string( name ) + ": n/a";

    std::string     result( name );
    result += ": ";
    for ( ssize_t  k = 0; k < lst.size(); ++k )
    {
        if ( k != 0 )
            result += "\n";
        result += lst[ k ].as_string();
    }
    return result;
}

static std::string
representPart( const Py::Object &  value,
               const char *        name )
{
    if ( value.isNone() )
        return std::string( name ) + ": None";
    return std::string( name ) + ": " + value.as_string();
}



FragmentBase::FragmentBase() :
    parent( NULL ), content( NULL ),
    kind( UNDEFINED_FRAGMENT ),
    begin( -1 ), end( -1 ), beginLine( -1 ), beginPos( -1 ),
    endLine( -1 ), endPos( -1 )
{}


FragmentBase::~FragmentBase()
{
    if ( content != NULL )
    {
        delete [] content;
        content = NULL;
    }
}


void  FragmentBase::appendMembers( Py::List &  container ) const
{
    container.append( Py::String( "kind" ) );
    container.append( Py::String( "begin" ) );
    container.append( Py::String( "end" ) );
    container.append( Py::String( "beginLine" ) );
    container.append( Py::String( "beginPos" ) );
    container.append( Py::String( "endLine" ) );
    container.append( Py::String( "endPos" ) );
    return;
}


bool  FragmentBase::getAttribute( const char *  attrName, Py::Object &  retval )
{
    GETINTATTR( kind );
    GETINTATTR( begin );
    GETINTATTR( end );
    GETINTATTR( beginLine );
    GETINTATTR( beginPos );
    GETINTATTR( endLine );
    GETINTATTR( endPos );

    return false;
}


std::string  FragmentBase::alignBlock( const std::string &  content,
                                       FragmentBase *  firstFragment )
{
    size_t                      indent( firstFragment->beginPos - 1 );
    std::vector< std::string >  lines( splitLines( content ) );

    for ( std::vector< std::string >::iterator  k( lines.begin() );
            k != lines.end(); ++k )
    {
        if ( k == lines.begin() )
            *k = std::string( firstFragment->beginPos - 1, ' ' ) + *k;
        *k = expandTabs( *k );
        if ( k != lines.begin() )
        {
            size_t      strippedSize( strlen( trimStart( k->c_str(), k->length() ) ) );
            if ( strippedSize > 0 )
                indent = std::min( indent, k->length() - strippedSize );
        }
    }

    // Remove indentation and trailing spaces
    size_t      lineNum( firstFragment->beginLine );
    ssize_t     lastIndex( lines.size() - 1 );
    for ( ssize_t  k( 0 ); k <= lastIndex; ++k )
    {
        std::string &   tmp( lines[ k ] );

        if ( indent != 0 )
            tmp = std::string( tmp.c_str() + indent );
        if ( k != lastIndex )
            trimEndInplace( tmp );

        ++lineNum;
    }

    // Glue the lines back
    std::string     result;
    for ( std::vector< std::string >::iterator  k( lines.begin() );
            k != lines.end(); ++k )
    {
        if ( ! result.empty() )
            result += "\n";
        result += *k;
    }
    return result;
}


std::string  FragmentBase::getContent( const char *  buf )
{
    if ( buf != NULL )
        return std::string( buf + begin, end - begin + 1 );

    // Check if serialized
    FragmentBase *      current = this;
    while ( current->parent != NULL )
        current = current->parent;
    if ( current->content != NULL )
        return std::string( current->content + begin, end - begin + 1 );

    throw Py::RuntimeError( "Cannot get content of not serialized "
                            "fragment without its buffer" );
}


Py::Object  FragmentBase::getContent( const Py::Tuple &  args )
{
    size_t      argCount( args.length() );

    if ( argCount == 0 )
        return Py::String( getContent( NULL ) );

    if ( argCount == 1 )
    {
        std::string  content( Py::String( args[ 0 ] ).as_std_string() );
        return Py::String( getContent( content.c_str() ) );
    }

    throwWrongBufArgument( "getContent" );
    return Py::None();  // Suppress compiler warning
}


Py::Object  FragmentBase::getLineContent( const Py::Tuple &  args )
{
    size_t      argCount( args.length() );

    if ( argCount == 0 )
        return Py::String( std::string( beginPos - 1, ' ' ) +
                           getContent( NULL ) );

    if ( argCount == 1 )
    {
        std::string  content( Py::String( args[ 0 ] ).as_std_string() );
        return Py::String( std::string( beginPos - 1, ' ' ) +
                           getContent( content.c_str() ) );
    }

    throw Py::RuntimeError( "Unexpected number of arguments. getLineContent() "
                            "supports no arguments or one argument "
                            "(text buffer)" );
}


Py::Object  FragmentBase::getParentIfID( void )
{
    FragmentBase *  current = parent;
    while ( current != NULL )
    {
        if ( current->kind == ELIF_PART_FRAGMENT )
            return PYTHON_INT_TYPE(reinterpret_cast<INT_TYPE>(current));

        current = current->parent;
    }
    return Py::None();
}


void FragmentBase::updateBegin( const FragmentBase *  other )
{
    if ( begin == -1 || other->begin < begin )
    {
        begin = other->begin;
        beginLine = other->beginLine;
        beginPos = other->beginPos;

        // Spread the change to the upper levels
        if ( parent != NULL )
            parent->updateBegin( other );
    }
}

void FragmentBase::updateEnd( const FragmentBase *  other )
{
    if ( end == -1 || other->end > end )
    {
        end = other->end;
        endLine = other->endLine;
        endPos = other->endPos;

        // Spread the change to the upper levels
        if ( parent != NULL )
            parent->updateEnd( other );
    }
}

void FragmentBase::updateBeginEnd( const FragmentBase *  other )
{
    updateBegin( other );
    updateEnd( other );
}


Py::Object  FragmentBase::getLineRange( void )
{
    return Py::TupleN( PYTHON_INT_TYPE( beginLine ),
                       PYTHON_INT_TYPE( endLine ) );
}


Py::Object  FragmentBase::getAbsPosRange( void )
{
    return Py::TupleN( PYTHON_INT_TYPE( begin ),
                       PYTHON_INT_TYPE( end ) );
}


std::string  FragmentBase::as_string( void ) const
{
    char    buffer[ 64 ];
    sprintf( buffer, "[%ld:%ld] (%ld,%ld) (%ld,%ld)",
                     begin, end,
                     beginLine, beginPos,
                     endLine, endPos );
    return buffer;
}


// --- End of FragmentBase definition ---


Fragment::Fragment()
{
    kind = FRAGMENT;
}


Fragment::~Fragment()
{}


void Fragment::initType( void )
{
    behaviors().name( "Fragment" );
    behaviors().doc( FRAGMENT_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );

    behaviors().readyType();
}


Py::Object Fragment::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        return members;
    }

    Py::Object      retval;
    if ( getAttribute( attrName, retval ) )
        return retval;
    return getattr_methods( attrName );
}


Py::Object  Fragment::repr( void )
{
    return Py::String( "<Fragment " + as_string() + ">" );
}


// --- End of Fragment definition ---


FragmentWithComments::FragmentWithComments()
{
    leadingComment = Py::None();
    sideComment = Py::None();
    body = Py::None();
}

FragmentWithComments::~FragmentWithComments()
{}

void FragmentWithComments::appendMembers( Py::List &  container )
{
    container.append( Py::String( "leadingComment" ) );
    container.append( Py::String( "sideComment" ) );
    container.append( Py::String( "leadingCMLComments" ) );
    container.append( Py::String( "sideCMLComments" ) );
    container.append( Py::String( "body" ) );
    return;
}

bool FragmentWithComments::getAttribute( const char *  attrName,
                                         Py::Object &  retval )
{
    if ( strcmp( attrName, "leadingComment" ) == 0 )
    {
        retval = leadingComment;
        return true;
    }
    if ( strcmp( attrName, "sideComment" ) == 0 )
    {
        retval = sideComment;
        return true;
    }
    if ( strcmp( attrName, "leadingCMLComments" ) == 0 )
    {
        retval = leadingCMLComments;
        return true;
    }
    if ( strcmp( attrName, "sideCMLComments" ) == 0 )
    {
        retval = sideCMLComments;
        return true;
    }
    if ( strcmp( attrName, "body" ) == 0 )
    {
        retval = body;
        return true;
    }
    return false;
}

std::string  FragmentWithComments::as_string( void ) const
{
    return representFragmentPart( body, "Body" ) +
           "\n" + representPart( leadingComment, "LeadingComment" ) +
           "\n" + representPart( sideComment, "SideComment" ) +
           "\n" + representList( leadingCMLComments, "LeadingCMLComments" ) +
           "\n" + representList( sideCMLComments, "SideCMLComments" );
}

Fragment *
FragmentWithComments::getSideCommentFragmentForLine( INT_TYPE  lineNo )
{
    Fragment *  f( NULL );

    if ( ! sideComment.isNone() )
        f = static_cast<Comment *>(sideComment.ptr())->getFragmentForLine( lineNo );
    if ( f == NULL )
    {
        Py::List::size_type     partCount( sideCMLComments.length() );
        for ( Py::List::size_type k( 0 ); k < partCount; ++k )
        {
            CMLComment *  currentComment( static_cast<CMLComment *>(sideCMLComments[ k ].ptr()) );
            f = currentComment->getFragmentForLine( lineNo );
            if ( f != NULL )
                return f;
        }
    }
    return f;
}

std::string
FragmentWithComments::alignBlockAndStripSideComments(const std::string &  content,
                                                     FragmentBase *  firstFragment)
{
    // The content may have trailing side comments
    size_t                      indent( firstFragment->beginPos - 1 );
    std::vector< std::string >  lines( splitLines( content ) );

    for ( std::vector< std::string >::iterator  k( lines.begin() );
            k != lines.end(); ++k )
    {
        if ( k == lines.begin() )
            *k = std::string( firstFragment->beginPos - 1, ' ' ) + *k;
        *k = expandTabs( *k );
        if ( k != lines.begin() )
        {
            size_t      strippedSize( strlen( trimStart( k->c_str(), k->length() ) ) );
            if ( strippedSize > 0 )
                indent = std::min( indent, k->length() - strippedSize );
        }
    }

    // Remove indentation, trailing comments and trailing spaces
    size_t      lineNum( firstFragment->beginLine );
    ssize_t     lastIndex( lines.size() - 1 );
    for ( ssize_t  k( 0 ); k <= lastIndex; ++k )
    {
        std::string &   tmp( lines[ k ] );
        Fragment *      f = getSideCommentFragmentForLine( lineNum );

        if ( f != NULL && k != lastIndex )
        {
            INT_TYPE        commentSize( f->endPos - f->beginPos + 1 );
            tmp = std::string( tmp.c_str(), tmp.size() - commentSize );
        }
        if ( indent != 0 )
            tmp = std::string( tmp.c_str() + indent );
        if ( k != lastIndex )
            trimEndInplace( tmp );

        ++lineNum;
    }

    // Glue the lines back
    std::string     result;
    for ( std::vector< std::string >::iterator  k( lines.begin() );
            k != lines.end(); ++k )
    {
        if ( ! result.empty() )
            result += "\n";
        result += *k;
    }
    return result;
}


// --- End of FragmentWithComments definition ---



BangLine::BangLine()
{
    kind = BANG_LINE_FRAGMENT;
}


BangLine::~BangLine()
{}


void BangLine::initType( void )
{
    behaviors().name( "BangLine" );
    behaviors().doc( BANGLINE_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &BangLine::getDisplayValue,
                        BANGLINE_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}


Py::Object BangLine::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        return members;
    }

    Py::Object      retval;
    if ( getAttribute( attrName, retval ) )
        return retval;
    return getattr_methods( attrName );
}


Py::Object  BangLine::repr( void )
{
    return Py::String( "<BangLine " + as_string() + ">" );
}

Py::Object  BangLine::getDisplayValue( const Py::Tuple &  args )
{
    std::string     content;

    switch ( args.length() )
    {
        case 0:
            content = FragmentBase::getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = FragmentBase::getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    if ( content.length() < 2 )
        throw Py::RuntimeError( "Unexpected bang line fragment. The fragment "
                                "is shorter than 2 characters." );
    if ( content[ 0 ] != '#' || content[ 1 ] != '!' )
        throw Py::RuntimeError( "Unexpected bang line fragment. There is "
                                "no #! at the beginning." );

    return Py::String( trim( content.c_str() + 2, content.length() - 2 ) );
}

// --- End of BangLine definition ---

EncodingLine::EncodingLine()
{
    kind = ENCODING_LINE_FRAGMENT;
}


EncodingLine::~EncodingLine()
{}


void EncodingLine::initType( void )
{
    behaviors().name( "EncodingLine" );
    behaviors().doc( ENCODINGLINE_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &EncodingLine::getDisplayValue,
                        ENCODINGLINE_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}


Py::Object EncodingLine::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        members.append( Py::String( "normalizedName" ) );
        return members;
    }

    Py::Object      retval;
    if ( getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "normalizedName" ) == 0 )
        return normalizedName;
    return getattr_methods( attrName );
}


Py::Object  EncodingLine::repr( void )
{
    return Py::String( "<EncodingLine " + as_string() +
                       "\nNormalizedName: " + normalizedName.as_std_string() +
                       ">" );
}

Py::Object  EncodingLine::getDisplayValue( const Py::Tuple &  args )
{
    std::string     content;

    switch ( args.length() )
    {
        case 0:
            content = FragmentBase::getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = FragmentBase::getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    const char *    lineStart( content.c_str() );
    const char *    encBegin( strstr( lineStart, "coding" ) );

    if ( encBegin == NULL )
        throw Py::RuntimeError( "Inconsistency detected. Cannot find 'coding' "
                                "substring in the EncodingLine fragment" );

    encBegin += 6;     /* len( 'coding' ) */
    if ( *encBegin == ':' || *encBegin == '=' )
        ++encBegin;
    while ( isspace( *encBegin ) )
        ++encBegin;

    const char *    encEnd( encBegin );
    while ( *encEnd != '\0' && isspace( *encEnd ) == 0 )
        ++encEnd;

    return Py::String( encBegin, encEnd - encBegin );
}

// --- End of EncodingLine definition ---


Comment::Comment()
{
    kind = COMMENT_FRAGMENT;
}


Comment::~Comment()
{}


void Comment::initType( void )
{
    behaviors().name( "Comment" );
    behaviors().doc( COMMENT_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Comment::getDisplayValue,
                        COMMENT_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}


Py::Object Comment::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        members.append( Py::String( "parts" ) );
        return members;
    }

    Py::Object      retval;
    if ( getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "parts" ) == 0 )
        return parts;
    return getattr_methods( attrName );
}


Py::Object  Comment::repr( void )
{
    return Py::String( "<Comment " + FragmentBase::as_string() +
                        "\n" + representList( parts, "Parts" ) +
                        ">" );
}

Py::Object  Comment::getDisplayValue( const Py::Tuple &  args )
{
    size_t          argCount( args.length() );
    std::string     buf;
    const char *    bufPointer;

    if ( argCount == 0 )
    {
        bufPointer = NULL;
    }
    else if ( argCount == 1 )
    {
        buf = Py::String( args[ 0 ] ).as_std_string();
        bufPointer = buf.c_str();
    }
    else
    {
        throwWrongBufArgument( "getDisplayValue" );
        throw std::exception();     // Suppress compiler warning
    }


    Py::List::size_type     partCount( parts.length() );

    if (partCount == 0)
        return Py::String( "" );

    Fragment *  firstFragment( static_cast<Fragment *>(parts[ 0 ].ptr()) );
    INT_TYPE    minShift( firstFragment->beginPos );
    bool        sameShift( true );
    size_t      minPostHashSpaces( std::numeric_limits< size_t >::max() );
    std::string lineContent;

    for ( Py::List::size_type k( 0 ); k < partCount; ++k )
    {
        Fragment *  currentFragment( static_cast<Fragment *>(parts[ k ].ptr()) );
        INT_TYPE    shift( currentFragment->beginPos );
        if ( shift != minShift )
        {
            sameShift = false;
            if ( shift < minShift )
                minShift = shift;
        }

        size_t      postHashSpaces( 0 );

        lineContent = currentFragment->getContent( bufPointer );
        if ( lineContent.size() > 1 )
        {
            for ( size_t  index = 1; index < lineContent.size(); ++index )
            {
                if ( lineContent[ index ] != ' ' )
                    break;
                ++postHashSpaces;
            }
            if ( postHashSpaces < minPostHashSpaces )
                minPostHashSpaces = postHashSpaces;
        }
    }

    // Check if a comment contains only lines with the only '#' character
    if ( minPostHashSpaces == std::numeric_limits< size_t >::max() )
        minPostHashSpaces = 0;

    std::string      content;
    INT_TYPE         currentLine( firstFragment->beginLine );

    for ( Py::List::size_type k( 0 ); k < partCount; ++k )
    {
        Fragment *  currentFragment( static_cast<Fragment *>(parts[ k ].ptr()) );

        if ( k != 0 )
            content += "\n";
        if ( currentFragment->beginLine - currentLine > 1 )
            content += std::string( currentFragment->beginLine - currentLine - 1, '\n' );

        if ( !sameShift )
            if ( currentFragment->beginPos > minShift )
                content += std::string( currentFragment->beginPos - minShift, ' ' );
        lineContent = currentFragment->getContent( bufPointer );

        // Strip the '#' character -- 1
        // Strip the common number of post '#' spaces
        if ( lineContent.size() > 1 )
            content += lineContent.substr( 1 + minPostHashSpaces );

        currentLine = currentFragment->beginLine;
    }

    return Py::String( content );
}

Fragment *  Comment::getFragmentForLine( INT_TYPE  lineNo )
{
    if ( lineNo < beginLine || lineNo > endLine )
        return NULL;

    Py::List::size_type     partCount( parts.length() );
    for ( Py::List::size_type k( 0 ); k < partCount; ++k )
    {
        Fragment *  currentFragment( static_cast<Fragment *>(parts[ k ].ptr()) );
        if ( currentFragment->beginLine == lineNo )
            return currentFragment;
    }
    return NULL;
}


// --- End of Comment definition ---

CMLComment::CMLComment()
{
    kind = CML_COMMENT_FRAGMENT;
    version = Py::Int( 0 );
    recordType = Py::String( "" );
}

CMLComment::~CMLComment()
{}

void CMLComment::initType( void )
{
    behaviors().name( "Comment" );
    behaviors().doc( CML_COMMENT_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );

    behaviors().readyType();
}

Py::Object CMLComment::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        members.append( Py::String( "parts" ) );
        members.append( Py::String( "version" ) );
        members.append( Py::String( "recordType" ) );
        members.append( Py::String( "properties" ) );
        return members;
    }

    Py::Object      retval;
    if ( getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "parts" ) == 0 )
        return parts;
    if ( strcmp( attrName, "version" ) == 0 )
        return version;
    if ( strcmp( attrName, "recordType" ) == 0 )
        return recordType;
    if ( strcmp( attrName, "properties" ) == 0 )
        return properties;
    return getattr_methods( attrName );
}

Py::Object  CMLComment::repr( void )
{
    return Py::String( "<CMLComment " + FragmentBase::as_string() +
                        "\n" + representList( parts, "Parts" ) +
                        "\n" + representPart( version, "Version" ) +
                        "\n" + representPart( recordType, "RecordType" ) +
                        "\n" + representPart( properties, "Properties" ) +
                        ">" );
}


// Extracts version, record type and properties
void CMLComment::extractProperties( Context *  context )
{
    // Combine the whole string considering continuations
    std::string     completed;
    int             firstLine( -1 );

    for ( ssize_t  k = 0; k < parts.size(); ++k )
    {
        Fragment *      f( TOFRAGMENT( parts[ k ] ) );
        const char *    b;

        if ( k == 0 )
        {
            b = strstr( context->buffer + f->begin, "cml" ) + 3;
            firstLine = f->beginLine;
        }
        else
        {
            b = strstr( context->buffer + f->begin, "cml+" ) + 4;
            completed += " ";
        }

        size_t          shift = b - ( context->buffer + f->begin );
        completed += std::string( b, f->end - f->begin + 1 - shift );
    }

    // version, recordType, properties
    std::string     token;
    ssize_t         pos( 0 );

    // Version
    token = getCMLCommentToken( completed, pos );
    if ( token.empty() )
    {
        context->flow->addWarning( firstLine, -1,
                                   "Could not find CML version" );
        return;
    }
    int     ver( atol( token.c_str() ) );
    if ( ver <= 0 )
    {
        context->flow->addWarning( firstLine, -1, "Unknown format of the "
                                   "CML version. Expected positive integer." );
        return;
    }
    this->version = Py::Int( ver );

    // Record type
    token = getCMLCommentToken( completed, pos );
    if ( token.empty() )
    {
        context->flow->addWarning( firstLine, -1,
                                   "Could not find CML record type" );
        return;
    }
    this->recordType = Py::String( token );

    // Properties
    for ( ; ; )
    {
        token = getCMLCommentToken( completed, pos );
        if ( token.empty() )
            break;

        std::string     key( token );
        token = getCMLCommentToken( completed, pos );
        if ( token != std::string( "=" ) )
        {
            context->flow->addWarning( firstLine, -1, "Could not find '=' "
                                       "after a property name (property '" +
                                       key + "')" );
            return;
        }

        std::string     warning;
        token = getCMLCommentValue( completed, pos, warning );
        if ( ! warning.empty() )
        {
            context->flow->addWarning( firstLine, -1,
                                       warning + " (property '" + key + "')" );
            return;
        }
        this->properties.setItem( key, Py::String( token ) );
    }
}

Fragment *  CMLComment::getFragmentForLine( INT_TYPE  lineNo )
{
    if ( lineNo < beginLine || lineNo > endLine )
        return NULL;

    Py::List::size_type     partCount( parts.length() );
    for ( Py::List::size_type k( 0 ); k < partCount; ++k )
    {
        Fragment *  currentFragment( static_cast<Fragment *>(parts[ k ].ptr()) );
        if ( currentFragment->beginLine == lineNo )
            return currentFragment;
    }
    return NULL;
}


// --- End of CMLComment definition ---

Docstring::Docstring()
{
    kind = DOCSTRING_FRAGMENT;
}


Docstring::~Docstring()
{}


void Docstring::initType( void )
{
    behaviors().name( "Docstring" );
    behaviors().doc( DOCSTRING_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Docstring::getDisplayValue,
                        DOCSTRING_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}


Py::Object Docstring::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "parts" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "parts" ) == 0 )
        return parts;
    return getattr_methods( attrName );
}


Py::Object  Docstring::repr( void )
{
    return Py::String( "<Docstring " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representList( parts, "Parts" ) +
                       ">" );
}

Py::Object  Docstring::getDisplayValue( const Py::Tuple &  args )
{
    size_t          argCount( args.length() );
    std::string     buf;
    const char *    bufPointer;

    if ( argCount == 0 )
    {
        bufPointer = NULL;
    }
    else if ( argCount == 1 )
    {
        buf = Py::String( args[ 0 ] ).as_std_string();
        bufPointer = buf.c_str();
    }
    else
    {
        throwWrongBufArgument( "getDisplayValue" );
        throw std::exception();     // suppress compiler warning
    }

    Fragment *      bodyFragment( static_cast<Fragment *>(body.ptr()) );
    std::string     rawContent( bodyFragment->getContent( bufPointer ) );
    size_t          stripFrontCount( 1 );
    size_t          stripBackCount( 1 );

    if ( strncmp( rawContent.c_str(), "'''", 3 ) == 0 ||
         strncmp( rawContent.c_str(), "\"\"\"", 3 ) == 0 )
    {
        stripFrontCount = 3;
        stripBackCount = 3;
    }
    else if ( strncmp( rawContent.c_str(), "r'''", 4 ) == 0 ||
              strncmp( rawContent.c_str(), "r\"\"\"", 4 ) == 0 ||
              strncmp( rawContent.c_str(), "u'''", 4 ) == 0 ||
              strncmp( rawContent.c_str(), "u\"\"\"", 4 ) == 0 )
    {
        stripFrontCount = 4;
        stripBackCount = 3;
    }
    else if ( strncmp( rawContent.c_str(), "r'", 2 ) == 0 ||
              strncmp( rawContent.c_str(), "r\"", 2 ) == 0 ||
              strncmp( rawContent.c_str(), "u'", 2 ) == 0 ||
              strncmp( rawContent.c_str(), "u\"", 2 ) == 0 )
    {
        stripFrontCount = 2;
    }

    return Py::String( trimDocstring(
                            rawContent.substr(
                                stripFrontCount,
                                rawContent.length() - stripFrontCount - stripBackCount ) ) );
}


std::string  Docstring::trimDocstring( const std::string &  docstring )
{
    if (docstring.empty())
        return "";

    // Split lines, expand tabs;
    // Detect the min indent (first line doesn't count)
    size_t                          indent( INT_MAX );
    std::vector< std::string >      lines( splitLines( docstring ) );
    for ( std::vector< std::string >::iterator  k( lines.begin() );
          k != lines.end(); ++k )
    {
        *k = expandTabs( *k );
        if ( k != lines.begin() )
        {
            size_t      strippedSize( strlen( trimStart( k->c_str(), k->length() ) ) );
            if ( strippedSize > 0 )
                indent = std::min( indent, k->length() - strippedSize );
        }
    }

    // Remove indentation (first line is special)
    lines[ 0 ] = trim( lines[ 0 ].c_str(), lines[ 0 ].length() );
    if ( indent < INT_MAX )
    {
        std::vector< std::string >::iterator    k( lines.begin() );
        for ( ++k; k != lines.end(); ++k )
        {
            std::string     rightStripped( trimEnd( k->c_str(), k->length() ) );
            if ( rightStripped.length() > indent )
                *k = std::string( rightStripped.c_str() + indent );
            else
                *k = "";
        }
    }

    // Strip off trailing and leading blank lines
    ssize_t     startIndex( 0 );
    ssize_t     endIndex( lines.size() - 1 );

    for ( ssize_t    k( startIndex ); k <= endIndex; ++k )
    {
        startIndex = k;
        if ( lines[ k ].length() != 0 ) 
            break;
    }

    for ( ssize_t   k( endIndex ); k >= 0; --k )
    {
        endIndex = k;
        if ( lines[ k ].length() != 0 )
            break;
    }

    std::string     result;
    for (ssize_t    k( startIndex ); k <= endIndex; ++k )
    {
        if ( k != startIndex )
            result += "\n";
        result += lines[ k ];
    }

    return result;
}


// --- End of Docstring definition ---

Decorator::Decorator()
{
    kind = DECORATOR_FRAGMENT;
    arguments = Py::None();
}


Decorator::~Decorator()
{}


void Decorator::initType( void )
{
    behaviors().name( "Decorator" );
    behaviors().doc( DECORATOR_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Decorator::getDisplayValue,
                        DECORATOR_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}


Py::Object Decorator::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "name" ) );
        members.append( Py::String( "arguments" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "name" ) == 0 )
        return name;
    if ( strcmp( attrName, "arguments" ) == 0 )
        return arguments;
    return getattr_methods( attrName );
}


Py::Object  Decorator::repr( void )
{
    return Py::String( "<Decorator " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( name, "Name" ) +
                       "\n" + representFragmentPart( arguments, "Arguments" ) +
                       ">" );
}

Py::Object Decorator::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      nameFragment( static_cast<Fragment *>(name.ptr()) );
    Fragment *      lastFragment( nameFragment );
    if ( ! arguments.isNone() )
        lastFragment = static_cast<Fragment *>(arguments.ptr());

    // The required fragment is from the name till the ')' or just the name
    FragmentBase    f;
    f.parent = nameFragment->parent;
    f.begin = nameFragment->begin;
    f.end = lastFragment->end;
    f.beginLine = nameFragment->beginLine;
    f.beginPos = nameFragment->beginPos;
    f.endLine = lastFragment->endLine;
    f.endPos = lastFragment->endPos;

    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = f.getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = f.getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content, &f ) );
}

// --- End of Decorator definition ---


CodeBlock::CodeBlock()
{
    kind = CODEBLOCK_FRAGMENT;
    lastLine = -1;
    firstNode = NULL;
    lastNode = NULL;
}


CodeBlock::~CodeBlock()
{}



void CodeBlock::initType( void )
{
    behaviors().name( "CodeBlock" );
    behaviors().doc( CODEBLOCK_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &CodeBlock::getDisplayValue,
                        CODEBLOCK_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}


Py::Object CodeBlock::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    return getattr_methods( attrName );
}


Py::Object  CodeBlock::repr( void )
{
    return Py::String( "<CodeBlock " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() + ">" );
}

Py::Object  CodeBlock::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      bodyFragment( static_cast<Fragment *>(body.ptr()) );
    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = bodyFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = bodyFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content,
                                                       bodyFragment ) );
}


// --- End of CodeBlock definition ---


Annotation::Annotation()
{
    kind = ANNOTATION_FRAGMENT;
    separator = Py::None();
    text = Py::None();
}


Annotation::~Annotation()
{}


void Annotation::initType( void )
{
    behaviors().name( "Annotation" );
    behaviors().doc( ANNOTATION_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Annotation::getDisplayValue,
                        ANNOTATION_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}


Py::Object Annotation::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        members.append( Py::String( "separator" ) );
        members.append( Py::String( "text" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "separator" ) == 0 )
        return separator;
    if ( strcmp( attrName, "text" ) == 0 )
        return text;
    return getattr_methods( attrName );
}


Py::Object  Annotation::repr( void )
{
    return Py::String( "<Annotation " + FragmentBase::as_string() +
                       "\n" + representFragmentPart( separator, "Separator" ) +
                       "\n" + representFragmentPart( text, "Text" ) +
                       ">" );
}

Py::Object Annotation::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      textFragment( static_cast<Fragment *>(text.ptr()) );
    std::string     content;

    switch ( args.length() )
    {
        case 0:
            content = textFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = textFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted. The common shift should be shaved.
    return Py::String( alignBlock( content, textFragment ) );
}


// --- End of Annotation definition ---


Argument::Argument()
{
    kind = ARGUMENT_FRAGMENT;
    name = Py::None();
    annotation = Py::None();
    separator = Py::None();
    defaultValue = Py::None();
}

Argument::~Argument()
{}

void Argument::initType( void )
{
    behaviors().name( "Argument" );
    behaviors().doc( ARGUMENT_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Argument::getDisplayValue,
                        ARGUMENT_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}


Py::Object Argument::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        members.append( Py::String( "name" ) );
        members.append( Py::String( "annotation" ) );
        members.append( Py::String( "separator" ) );
        members.append( Py::String( "defaultValue" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "name" ) == 0 )
        return name;
    if ( strcmp( attrName, "annotation" ) == 0 )
        return annotation;
    if ( strcmp( attrName, "separator" ) == 0 )
        return separator;
    if ( strcmp( attrName, "defaultValue" ) == 0 )
        return defaultValue;
    return getattr_methods( attrName );
}

Py::Object  Argument::repr( void )
{
    return Py::String( "<Argument " + FragmentBase::as_string() +
                       "\n" + representFragmentPart( name, "Name" ) +
                       "\n" + representPart( annotation, "Annotation" ) +
                       "\n" + representFragmentPart( separator, "Separator" ) +
                       "\n" + representFragmentPart( defaultValue, "defaultValue" ) +
                       ">" );
}

Py::Object Argument::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      nameFragment( static_cast<Fragment *>(name.ptr()) );
    Fragment *      lastFragment;

    if ( ! defaultValue.isNone() )
    {
        lastFragment = static_cast<Fragment *>(defaultValue.ptr());
    }
    else if ( ! annotation.isNone() )
    {
        Annotation *    annot = static_cast<Annotation *>(annotation.ptr());
        lastFragment = static_cast<Fragment *>(annot->text.ptr());
    }
    else
    {
        // There is only an argument name
        lastFragment = static_cast<Fragment *>(name.ptr());
    }

    std::string     content;

    FragmentBase    f;
    f.parent = nameFragment->parent;
    f.begin = nameFragment->begin;
    f.end = lastFragment->end;
    f.beginLine = nameFragment->beginLine;
    f.beginPos = nameFragment->beginPos;
    f.endLine = lastFragment->endLine;
    f.endPos = lastFragment->endPos;

    switch ( args.length() )
    {
        case 0:
            content = f.getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = f.getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted. The common shift should be shaved.
    return Py::String( alignBlock( content, & f ) );
}


// --- End of Argument definition ---


Function::Function()
{
    kind = FUNCTION_FRAGMENT;
    asyncKeyword = Py::None();
    defKeyword = Py::None();
    name = Py::None();
    arguments = Py::None();
    annotation = Py::None();
    docstring = Py::None();
}


Function::~Function()
{}


void Function::initType( void )
{
    behaviors().name( "Function" );
    behaviors().doc( FUNCTION_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_noargs_method( "isAsync", &Function::isAsync,
                       FUNCTION_ISASYNC_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Function::getDisplayValue,
                        FUNCTION_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object Function::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "decorators" ) );
        members.append( Py::String( "asyncKeyword" ) );
        members.append( Py::String( "defKeyword" ) );
        members.append( Py::String( "name" ) );
        members.append( Py::String( "arguments" ) );
        members.append( Py::String( "argList" ) );
        members.append( Py::String( "annotation" ) );
        members.append( Py::String( "docstring" ) );
        members.append( Py::String( "suite" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "decorators" ) == 0 )
        return decors;
    if ( strcmp( attrName, "asyncKeyword" ) == 0 )
        return asyncKeyword;
    if ( strcmp( attrName, "defKeyword" ) == 0 )
        return defKeyword;
    if ( strcmp( attrName, "name" ) == 0 )
        return name;
    if ( strcmp( attrName, "arguments" ) == 0 )
        return arguments;
    if ( strcmp( attrName, "argList" ) == 0 )
        return argList;
    if ( strcmp( attrName, "annotation" ) == 0 )
        return annotation;
    if ( strcmp( attrName, "docstring" ) == 0 )
        return docstring;
    if ( strcmp( attrName, "suite" ) == 0 )
        return nsuite;
    return getattr_methods( attrName );
}

Py::Object  Function::repr( void )
{
    return Py::String( "<Function " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( asyncKeyword, "Async" ) +
                       "\n" + representFragmentPart( defKeyword, "Def" ) +
                       "\n" + representFragmentPart( name, "Name" ) +
                       "\n" + representFragmentPart( arguments, "Arguments" ) +
                       "\n" + representList( argList, "ArgList" ) +
                       "\n" + representPart( annotation, "Annotation" ) +
                       "\n" + representPart( docstring, "Docstring" ) +
                       "\n" + representList( decors, "Decorators" ) +
                       "\n" + representList( nsuite, "Suite" ) +
                       ">" );
}

Py::Object Function::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      nameFragment( static_cast<Fragment *>(name.ptr()) );
    Fragment *      argsFragment( static_cast<Fragment *>(arguments.ptr()) );
    std::string     content;

    // The required fragment is from the name till the ')'
    FragmentBase    f;
    f.parent = nameFragment->parent;
    f.begin = nameFragment->begin;
    f.end = argsFragment->end;
    f.beginLine = nameFragment->beginLine;
    f.beginPos = nameFragment->beginPos;
    f.endLine = argsFragment->endLine;
    f.endPos = argsFragment->endPos;

    switch ( args.length() )
    {
        case 0:
            content = f.getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = f.getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content, & f ) );
}


Py::Object  Function::isAsync( void )
{
    if ( asyncKeyword.isNone() )
        return Py::Boolean( false );
    return Py::Boolean( true );
}


// --- End of Function definition ---


Class::Class()
{
    kind = CLASS_FRAGMENT;
    name = Py::None();
    baseClasses = Py::None();
    docstring = Py::None();
}

Class::~Class()
{}

void Class::initType( void )
{
    behaviors().name( "Class" );
    behaviors().doc( CLASS_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Class::getDisplayValue,
                        CLASS_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object Class::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "decorators" ) );
        members.append( Py::String( "name" ) );
        members.append( Py::String( "baseClasses" ) );
        members.append( Py::String( "docstring" ) );
        members.append( Py::String( "suite" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "decorators" ) == 0 )
        return decors;
    if ( strcmp( attrName, "name" ) == 0 )
        return name;
    if ( strcmp( attrName, "baseClasses" ) == 0 )
        return baseClasses;
    if ( strcmp( attrName, "docstring" ) == 0 )
        return docstring;
    if ( strcmp( attrName, "suite" ) == 0 )
        return nsuite;
    return getattr_methods( attrName );
}

Py::Object  Class::repr( void )
{
    return Py::String( "<Class " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( name, "Name" ) +
                       "\n" + representPart( baseClasses, "BaseClasses" ) +
                       "\n" + representPart( docstring, "Docstring" ) +
                       "\n" + representList( decors, "Decorators" ) +
                       "\n" + representList( nsuite, "Suite" ) +
                       ">" );
}

Py::Object Class::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      nameFragment( static_cast<Fragment *>(name.ptr()) );
    Fragment *      lastFragment( nameFragment );
    if ( ! baseClasses.isNone() )
        lastFragment = static_cast<Fragment *>(baseClasses.ptr());

    // The required fragment is from the name till the ')' or just the name
    FragmentBase    f;
    f.parent = nameFragment->parent;
    f.begin = nameFragment->begin;
    f.end = lastFragment->end;
    f.beginLine = nameFragment->beginLine;
    f.beginPos = nameFragment->beginPos;
    f.endLine = lastFragment->endLine;
    f.endPos = lastFragment->endPos;

    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = f.getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = f.getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content, & f ) );
}

// --- End of Class definition ---

Break::Break()
{
    kind = BREAK_FRAGMENT;
}

Break::~Break()
{}

void Break::initType( void )
{
    behaviors().name( "Break" );
    behaviors().doc( BREAK_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Break::getDisplayValue,
                        BREAK_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object Break::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    return getattr_methods( attrName );
}

Py::Object  Break::repr( void )
{
    return Py::String( "<Break " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() + ">" );
}

Py::Object  Break::getDisplayValue( const Py::Tuple &  args )
{
    return Py::String( "break" );
}


// --- End of Break definition ---

Continue::Continue()
{
    kind = CONTINUE_FRAGMENT;
}

Continue::~Continue()
{}

void Continue::initType( void )
{
    behaviors().name( "Continue" );
    behaviors().doc( CONTINUE_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Continue::getDisplayValue,
                        CONTINUE_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object Continue::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    return getattr_methods( attrName );
}

Py::Object  Continue::repr( void )
{
    return Py::String( "<Continue " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() + ">" );
}


Py::Object  Continue::getDisplayValue( const Py::Tuple &  args )
{
    return Py::String( "continue" );
}


// --- End of Continue definition ---

Return::Return()
{
    kind = RETURN_FRAGMENT;
    value = Py::None();
}

Return::~Return()
{}

void Return::initType( void )
{
    behaviors().name( "Return" );
    behaviors().doc( RETURN_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Return::getDisplayValue,
                        RETURN_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object Return::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "value" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "value" ) == 0 )
        return value;
    return getattr_methods( attrName );
}

Py::Object  Return::repr( void )
{
    return Py::String( "<Return " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( value, "Value" ) +
                       ">" );
}

Py::Object Return::getDisplayValue( const Py::Tuple &  args )
{
    if ( value.isNone() )
        return Py::String();

    Fragment *      valFragment( static_cast<Fragment *>(value.ptr()) );
    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = valFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = valFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content, valFragment ) );
}

// --- End of Return definition ---

Raise::Raise()
{
    kind = RAISE_FRAGMENT;
    value = Py::None();
}

Raise::~Raise()
{}

void Raise::initType( void )
{
    behaviors().name( "Raise" );
    behaviors().doc( RAISE_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Raise::getDisplayValue,
                        RAISE_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object Raise::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "value" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "value" ) == 0 )
        return value;
    return getattr_methods( attrName );
}

Py::Object  Raise::repr( void )
{
    return Py::String( "<Raise " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( value, "Value" ) +
                       ">" );
}

Py::Object Raise::getDisplayValue( const Py::Tuple &  args )
{
    if ( value.isNone() )
        return Py::String();

    Fragment *      valFragment( static_cast<Fragment *>(value.ptr()) );
    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = valFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = valFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content, valFragment ) );
}

// --- End of Raise definition ---

Assert::Assert()
{
    kind = ASSERT_FRAGMENT;
    tst = Py::None();
    message = Py::None();
}

Assert::~Assert()
{}

void Assert::initType( void )
{
    behaviors().name( "Assert" );
    behaviors().doc( ASSERT_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Assert::getDisplayValue,
                        ASSERT_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object Assert::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "test" ) );
        members.append( Py::String( "message" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "test" ) == 0 )
        return tst;
    if ( strcmp( attrName, "message" ) == 0 )
        return message;
    return getattr_methods( attrName );
}

Py::Object  Assert::repr( void )
{
    return Py::String( "<Assert " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( tst, "Test" ) +
                       "\n" + representFragmentPart( message, "Message" ) +
                       ">" );
}

Py::Object Assert::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      tstFragment( static_cast<Fragment *>(tst.ptr()) );
    Fragment *      lastFragment( tstFragment );
    if ( ! message.isNone() )
        lastFragment = static_cast<Fragment *>(message.ptr());

    // The required fragment is from the name till the ')' or just the name
    FragmentBase    f;
    f.parent = tstFragment->parent;
    f.begin = tstFragment->begin;
    f.end = lastFragment->end;
    f.beginLine = tstFragment->beginLine;
    f.beginPos = tstFragment->beginPos;
    f.endLine = lastFragment->endLine;
    f.endPos = lastFragment->endPos;

    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = f.getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = f.getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content, & f ) );
}

// --- End of Assert definition ---

SysExit::SysExit()
{
    kind = SYSEXIT_FRAGMENT;
    arg = Py::None();
    actualArg = Py::None();
}

SysExit::~SysExit()
{}

void SysExit::initType( void )
{
    behaviors().name( "SysExit" );
    behaviors().doc( SYSEXIT_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &SysExit::getDisplayValue,
                        SYSEXIT_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object SysExit::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "argument" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "argument" ) == 0 )
        return actualArg;
    return getattr_methods( attrName );
}

Py::Object  SysExit::repr( void )
{
    return Py::String( "<SysExit " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( actualArg, "Argument" ) +
                       ">" );
}

Py::Object SysExit::getDisplayValue( const Py::Tuple &  args )
{
    if ( actualArg.isNone() )
        return Py::String( "" );

    Fragment *      argFragment( static_cast<Fragment *>(actualArg.ptr()) );
    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = argFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = argFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content,
                                                       argFragment ) );
}


// --- End of SysExit definition ---

While::While()
{
    kind = WHILE_FRAGMENT;
    condition = Py::None();
    elsePart = Py::None();
}

While::~While()
{}

void While::initType( void )
{
    behaviors().name( "While" );
    behaviors().doc( WHILE_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &While::getDisplayValue,
                        WHILE_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object While::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "condition" ) );
        members.append( Py::String( "suite" ) );
        members.append( Py::String( "elsePart" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "condition" ) == 0 )
        return condition;
    if ( strcmp( attrName, "suite" ) == 0 )
        return nsuite;
    if ( strcmp( attrName, "elsePart" ) == 0 )
        return elsePart;
    return getattr_methods( attrName );
}

Py::Object  While::repr( void )
{
    return Py::String( "<While " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( condition, "Condition" ) +
                       "\n" + representList( nsuite, "Suite" ) +
                       "\n" + representPart( elsePart, "ElsePart" ) +
                       ">" );
}

Py::Object While::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      condFragment( static_cast<Fragment *>(condition.ptr()) );
    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = condFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = condFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content, condFragment ) );
}


// --- End of While definition ---

For::For()
{
    kind = FOR_FRAGMENT;
    asyncKeyword = Py::None();
    forKeyword = Py::None();
    iteration = Py::None();
    elsePart = Py::None();
}

For::~For()
{}

void For::initType( void )
{
    behaviors().name( "For" );
    behaviors().doc( FOR_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_noargs_method( "isAsync", &For::isAsync,
                       FOR_ISASYNC_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &For::getDisplayValue,
                        FOR_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object For::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "asyncKeyword" ) );
        members.append( Py::String( "forKeyword" ) );
        members.append( Py::String( "iteration" ) );
        members.append( Py::String( "suite" ) );
        members.append( Py::String( "elsePart" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "asyncKeyword" ) == 0 )
        return asyncKeyword;
    if ( strcmp( attrName, "forKeyword" ) == 0 )
        return forKeyword;
    if ( strcmp( attrName, "iteration" ) == 0 )
        return iteration;
    if ( strcmp( attrName, "suite" ) == 0 )
        return nsuite;
    if ( strcmp( attrName, "elsePart" ) == 0 )
        return elsePart;
    return getattr_methods( attrName );
}

Py::Object  For::repr( void )
{
    return Py::String( "<For " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( asyncKeyword, "Async" ) +
                       "\n" + representFragmentPart( forKeyword, "For" ) +
                       "\n" + representFragmentPart( iteration, "Iteration" ) +
                       "\n" + representList( nsuite, "Suite" ) +
                       "\n" + representPart( elsePart, "ElsePart" ) +
                       ">" );
}

Py::Object For::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      itFragment( static_cast<Fragment *>(iteration.ptr()) );
    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = itFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = itFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content, itFragment ) );
}


Py::Object  For::isAsync( void )
{
    if ( asyncKeyword.isNone() )
        return Py::Boolean( false );
    return Py::Boolean( true );
}

// --- End of For definition ---

Import::Import()
{
    kind = IMPORT_FRAGMENT;
    fromPart = Py::None();
    whatPart = Py::None();
}

Import::~Import()
{}

void Import::initType( void )
{
    behaviors().name( "Import" );
    behaviors().doc( IMPORT_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Import::getDisplayValue,
                        IMPORT_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object Import::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "fromPart" ) );
        members.append( Py::String( "whatPart" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "fromPart" ) == 0 )
        return fromPart;
    if ( strcmp( attrName, "whatPart" ) == 0 )
        return whatPart;
    return getattr_methods( attrName );
}

Py::Object  Import::repr( void )
{
    return Py::String( "<Import " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( fromPart, "FromPart" ) +
                       "\n" + representFragmentPart( whatPart, "WhatPart" ) +
                       ">" );
}


Py::Object Import::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      bodyFragment( static_cast<Fragment *>(body.ptr()) );
    std::string     fromContent;
    std::string     whatContent;
    Fragment *      whatFragment( static_cast<Fragment *>(whatPart.ptr()) );
    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = bodyFragment->getContent( NULL );
            if ( ! fromPart.isNone() )
                fromContent = static_cast<Fragment *>(fromPart.ptr())->getContent( NULL );
            whatContent = whatFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = bodyFragment->getContent( buf.c_str() );
                if ( ! fromPart.isNone() )
                    fromContent = static_cast<Fragment *>(fromPart.ptr())->getContent( buf.c_str() );
                whatContent = whatFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    std::string     result;
    if ( ! fromPart.isNone() )
        result = "from " + fromContent + "\n";

    // The content of the what part may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( result +
                       alignBlockAndStripSideComments( whatContent,
                                                       whatFragment ) );
}


// --- End of Import definition ---


ElifPart::ElifPart()
{
    kind = ELIF_PART_FRAGMENT;
    condition = Py::None();
}

ElifPart::~ElifPart()
{}

void ElifPart::initType( void )
{
    behaviors().name( "ElifPart" );
    behaviors().doc( ELIFPART_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &ElifPart::getDisplayValue,
                        ELIFPART_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object ElifPart::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "condition" ) );
        members.append( Py::String( "suite" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "condition" ) == 0 )
        return condition;
    if ( strcmp( attrName, "suite" ) == 0 )
        return nsuite;
    return getattr_methods( attrName );
}

Py::Object  ElifPart::repr( void )
{
    return Py::String( "<ElifPart " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( condition, "Condition" ) +
                       "\n" + representList( nsuite, "Suite" ) +
                       ">" );
}

Py::Object  ElifPart::getDisplayValue( const Py::Tuple &  args )
{
    if (condition.isNone())
        return Py::String();

    Fragment *      condFragment( static_cast<Fragment *>(condition.ptr()) );
    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = condFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = condFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content,
                                                       condFragment ) );
}


// --- End of IfPart definition ---

If::If()
{
    kind = IF_FRAGMENT;
}

If::~If()
{}

void If::initType( void )
{
    behaviors().name( "If" );
    behaviors().doc( IF_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );

    behaviors().readyType();
}

Py::Object If::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "parts" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "parts" ) == 0 )
        return parts;
    return getattr_methods( attrName );
}

Py::Object  If::repr( void )
{
    return Py::String( "<If " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representList( parts, "Parts" ) +
                       ">" );
}


// --- End of If definition ---

With::With()
{
    kind = WITH_FRAGMENT;
    asyncKeyword = Py::None();
    withKeyword = Py::None();
    items = Py::None();
}

With::~With()
{}

void With::initType( void )
{
    behaviors().name( "With" );
    behaviors().doc( WITH_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_noargs_method( "isAsync", &With::isAsync,
                       WITH_ISASYNC_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &With::getDisplayValue,
                        WITH_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object With::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "asyncKeyword" ) );
        members.append( Py::String( "withKeyword" ) );
        members.append( Py::String( "items" ) );
        members.append( Py::String( "suite" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "asyncKeyword" ) == 0 )
        return asyncKeyword;
    if ( strcmp( attrName, "withKeyword" ) == 0 )
        return withKeyword;
    if ( strcmp( attrName, "items" ) == 0 )
        return items;
    if ( strcmp( attrName, "suite" ) == 0 )
        return nsuite;
    return getattr_methods( attrName );
}

Py::Object  With::repr( void )
{
    return Py::String( "<With " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( asyncKeyword, "Async" ) +
                       "\n" + representFragmentPart( withKeyword, "With" ) +
                       "\n" + representFragmentPart( items, "Items" ) +
                       "\n" + representList( nsuite, "Suite" ) +
                       ">" );
}

Py::Object With::getDisplayValue( const Py::Tuple &  args )
{
    Fragment *      itemsFragment( static_cast<Fragment *>(items.ptr()) );
    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = itemsFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = itemsFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content, itemsFragment ) );
}


Py::Object  With::isAsync( void )
{
    if ( asyncKeyword.isNone() )
        return Py::Boolean( false );
    return Py::Boolean( true );
}


// --- End of With definition ---

ExceptPart::ExceptPart()
{
    kind = EXCEPT_PART_FRAGMENT;
    clause = Py::None();
}

ExceptPart::~ExceptPart()
{}

void ExceptPart::initType( void )
{
    behaviors().name( "ExceptPart" );
    behaviors().doc( EXCEPTPART_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &ExceptPart::getDisplayValue,
                        EXCEPTPART_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object ExceptPart::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "clause" ) );
        members.append( Py::String( "suite" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "clause" ) == 0 )
        return clause;
    if ( strcmp( attrName, "suite" ) == 0 )
        return nsuite;
    return getattr_methods( attrName );
}

Py::Object  ExceptPart::repr( void )
{
    return Py::String( "<ExceptPart " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representFragmentPart( clause, "Clause" ) +
                       "\n" + representList( nsuite, "Suite" ) +
                       ">" );
}

Py::Object ExceptPart::getDisplayValue( const Py::Tuple &  args )
{
    if ( clause.isNone() )
        return Py::String();

    Fragment *      clauseFragment( static_cast<Fragment *>(clause.ptr()) );
    std::string     content;
    switch ( args.length() )
    {
        case 0:
            content = clauseFragment->getContent( NULL );
            break;
        case 1:
            {
                std::string  buf( Py::String( args[ 0 ] ).as_std_string() );
                content = clauseFragment->getContent( buf.c_str() );
                break;
            }
        default:
            throwWrongBufArgument( "getDisplayValue" );
    }

    // The content may be shifted and may have side comments.
    // The common shift should be shaved as well the comments
    return Py::String( alignBlockAndStripSideComments( content, clauseFragment ) );
}

// --- End of ExceptPart definition ---

Try::Try()
{
    kind = TRY_FRAGMENT;
    finallyPart = Py::None();
}

Try::~Try()
{}

void Try::initType( void )
{
    behaviors().name( "Try" );
    behaviors().doc( TRY_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &Try::getDisplayValue,
                        TRY_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object Try::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "exceptParts" ) );
        members.append( Py::String( "elsePart" ) );
        members.append( Py::String( "finallyPart" ) );
        members.append( Py::String( "suite" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "exceptParts" ) == 0 )
        return exceptParts;
    if ( strcmp( attrName, "elsePart" ) == 0 )
        return elsePart;
    if ( strcmp( attrName, "finallyPart" ) == 0 )
        return finallyPart;
    if ( strcmp( attrName, "suite" ) == 0 )
        return nsuite;
    return getattr_methods( attrName );
}

Py::Object  Try::repr( void )
{
    return Py::String( "<Try " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\n" + representList( nsuite, "Suite" ) +
                       "\n" + representList( exceptParts, "ExceptParts" ) +
                       "\n" + representPart( elsePart, "ElsePart" ) +
                       "\n" + representPart( finallyPart, "FinallyPart" ) +
                       ">" );
}

Py::Object  Try::getDisplayValue( const Py::Tuple &  args )
{
    return Py::String( "" );
}


// --- End of Try definition ---

ControlFlow::ControlFlow()
{
    kind = CONTROL_FLOW_FRAGMENT;

    bangLine = Py::None();
    encodingLine = Py::None();
    docstring = Py::None();
    body = Py::None();
}

ControlFlow::~ControlFlow()
{
    if ( content != NULL )
    {
        delete [] content;
        content = NULL;
    }
}

void ControlFlow::initType( void )
{
    behaviors().name( "ControlFlow" );
    behaviors().doc( CONTROLFLOW_DOC );
    behaviors().supportGetattr();
    behaviors().supportRepr();

    add_noargs_method( "getLineRange", &FragmentBase::getLineRange,
                       GETLINERANGE_DOC );
    add_noargs_method( "getAbsPosRange", &FragmentBase::getAbsPosRange,
                       GETABSPOSRANGE_DOC );
    add_noargs_method( "getParentIfID", &FragmentBase::getParentIfID,
                       GETPARENTIFID_DOC );
    add_varargs_method( "getContent", &FragmentBase::getContent,
                        GETCONTENT_DOC );
    add_varargs_method( "getLineContent", &FragmentBase::getLineContent,
                        GETLINECONTENT_DOC );
    add_varargs_method( "getDisplayValue", &ControlFlow::getDisplayValue,
                        CONTROLFLOW_GETDISPLAYVALUE_DOC );

    behaviors().readyType();
}

Py::Object ControlFlow::getattr( const char *  attrName )
{
    // Support for dir(...)
    if ( strcmp( attrName, "__members__" ) == 0 )
    {
        Py::List    members;
        FragmentBase::appendMembers( members );
        FragmentWithComments::appendMembers( members );
        members.append( Py::String( "bangLine" ) );
        members.append( Py::String( "encodingLine" ) );
        members.append( Py::String( "docstring" ) );
        members.append( Py::String( "suite" ) );
        members.append( Py::String( "isOK" ) );
        members.append( Py::String( "errors" ) );
        members.append( Py::String( "warnings" ) );
        return members;
    }

    Py::Object      retval;
    if ( FragmentBase::getAttribute( attrName, retval ) )
        return retval;
    if ( FragmentWithComments::getAttribute( attrName, retval ) )
        return retval;
    if ( strcmp( attrName, "bangLine" ) == 0 )
        return bangLine;
    if ( strcmp( attrName, "encodingLine" ) == 0 )
        return encodingLine;
    if ( strcmp( attrName, "docstring" ) == 0 )
        return docstring;
    if ( strcmp( attrName, "suite" ) == 0 )
        return nsuite;
    if ( strcmp( attrName, "isOK" ) == 0 )
        return Py::Boolean( errors.size() == 0 );
    if ( strcmp( attrName, "errors" ) == 0 )
        return errors;
    if ( strcmp( attrName, "warnings" ) == 0 )
        return warnings;
    return getattr_methods( attrName );
}

Py::Object  ControlFlow::repr( void )
{
    std::string     ok( "true" );
    if ( errors.size() != 0 )
        ok = "false";

    return Py::String( "<ControlFlow " + FragmentBase::as_string() +
                       "\n" + FragmentWithComments::as_string() +
                       "\nisOK: " + ok +
                       "\n" + representList( errors, "Errors" ) +
                       "\n" + representList( warnings, "Warnings" ) +
                       "\n" + representFragmentPart( bangLine, "BangLine" ) +
                       "\n" + representFragmentPart( encodingLine, "EncodingLine" ) +
                       "\n" + representPart( docstring, "Docstring" ) +
                       "\n" + representList( nsuite, "Suite" ) +
                       ">" );
}


void  ControlFlow::addWarning( int  line, int  column,
                               const std::string &  message )
{
    warnings.append( Py::TupleN( Py::Int( line ),
                                 Py::Int( column ),
                                 Py::String( message ) ) );
}


void  ControlFlow::addError( int  line, int  column,
                             const std::string &  message )
{
    errors.append( Py::TupleN( Py::Int( line ),
                               Py::Int( column ),
                               Py::String( message ) ) );
}


Py::Object  ControlFlow::getDisplayValue( const Py::Tuple &  args )
{
    std::string     header( "Shebang line: " );

    if ( bangLine.isNone() )
        header += "not specified";
    else
    {
        BangLine *      bl( static_cast< BangLine * >( bangLine.ptr() ) );
        header += Py::String( bl->getDisplayValue( args ) );
    }

    header += "\nEncoding: ";
    if ( encodingLine.isNone() )
        header += "not specified";
    else
    {
        EncodingLine *  el( static_cast< EncodingLine * >( encodingLine.ptr() ) );
        header += Py::String( el->getDisplayValue( args ) );
    }

    return Py::String( header );
}


