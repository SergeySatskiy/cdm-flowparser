/*
 * codimension - graphics python two-way code editor and analyzer
 * Copyright (C) 2010  Sergey Satskiy <sergey.satskiy@gmail.com>
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
 * $Id$
 *
 * Python extension module
 */


#include <Python.h>
#include "pycfLexer.h"
#include "pycfParser.h"

#include <regex.h>

#ifndef CDM_CF_PARSER_VERSION
#define CDM_CF_PARSER_VERSION       "trunk"
#endif
#define MAX_DOTTED_NAME_LENGTH      256


/* copied (and modified) from libantlr3 to avoid indirect function calls */
static inline ANTLR3_UINT32 getType( pANTLR3_BASE_TREE tree )
{
    pANTLR3_COMMON_TREE    theTree = (pANTLR3_COMMON_TREE)(tree->super);

    if (theTree->token == NULL) return 0;
    return theTree->token->getType(theTree->token);
}
static inline void *  vectorGet( pANTLR3_VECTOR vector, ANTLR3_UINT32 entry )
{
    if ( entry < vector->count )
        return vector->elements[entry].element;
    return NULL;
}





/* Holds the currently analysed scope */
enum Scope {
    GLOBAL_SCOPE,
    FUNCTION_SCOPE,
    CLASS_SCOPE,
    CLASS_METHOD_SCOPE,
    CLASS_STATIC_METHOD_SCOPE
};


/* The structure holds resolved callbacks for Python class methods */
struct instanceCallbacks
{
    PyObject *      onError;
    PyObject *      onLexerError;
    PyObject *      onBangLine;
    PyObject *      onEncodingLine;

    /*
    PyObject *      onEncoding;
    PyObject *      onGlobal;
    PyObject *      onFunction;
    PyObject *      onClass;
    PyObject *      onImport;
    PyObject *      onAs;
    PyObject *      onWhat;
    PyObject *      onClassAttribute;
    PyObject *      onInstanceAttribute;
    PyObject *      onDecorator;
    PyObject *      onDecoratorArgument;
    PyObject *      onDocstring;
    PyObject *      onArgument;
    PyObject *      onBaseClass;
    */
};

/* Forward declaration */
void walk( pANTLR3_BASE_TREE            tree,
           struct instanceCallbacks *   callbacks,
           int                          objectsLevel,
           enum Scope                   scope,
           const char *                 firstArgName,
           int                          entryLevel );


#define GET_CALLBACK( name )                                                \
    callbacks->name = PyObject_GetAttrString( instance, "_" #name );        \
    if ( (! callbacks->name) || (! PyCallable_Check(callbacks->name)) )     \
    {                                                                       \
        PyErr_SetString( PyExc_TypeError, "Cannot get _" #name "method" );  \
        return 1;                                                           \
    }


/* Helper function to extract and check method pointers */
static int
getInstanceCallbacks( PyObject *                  instance,
                      struct instanceCallbacks *  callbacks )
{
    GET_CALLBACK( onError );
    GET_CALLBACK( onLexerError );
    GET_CALLBACK( onBangLine );
    GET_CALLBACK( onEncodingLine );

    /*
    GET_CALLBACK( onEncoding );
    GET_CALLBACK( onGlobal );
    GET_CALLBACK( onClass );
    GET_CALLBACK( onFunction );
    GET_CALLBACK( onImport );
    GET_CALLBACK( onAs );
    GET_CALLBACK( onWhat );
    GET_CALLBACK( onClassAttribute );
    GET_CALLBACK( onInstanceAttribute );
    GET_CALLBACK( onDecorator );
    GET_CALLBACK( onDecoratorArgument );
    GET_CALLBACK( onDocstring );
    GET_CALLBACK( onArgument );
    GET_CALLBACK( onBaseClass );
    */

    return 0;
}


#if 0
/* Fills the given buffer
 * Returns: token of the first name tokens
 */
pANTLR3_COMMON_TOKEN  getDottedName( pANTLR3_BASE_TREE  tree,
                                     char *             name )
{
    ANTLR3_UINT32           n = tree->children->count;
    ANTLR3_UINT32           k;
    int                     len = 0;
    const char *            tail = "";
    pANTLR3_COMMON_TOKEN    firstToken = NULL;

    name[ 0 ] = '\0';
    for ( k = 0; k < n; ++k )
    {
        len += strlen( tail );
        pANTLR3_BASE_TREE   child = vectorGet( tree->children, k );

        if ( k != 0 )
            name[ len++ ] = '.';
        else
            firstToken = child->getToken( child );
        tail = (const char *)(child->toString( child )->chars);
        strcpy( name + len, tail );
    }
    return firstToken;
}


void checkForDocstring( pANTLR3_BASE_TREE             tree,
                        struct instanceCallbacks *    callbacks )
{
    if ( tree == NULL ) return;
    if ( getType( tree ) != TEST_LIST ) return;
    if ( tree->children->count < 1 ) return;

    tree = vectorGet( tree->children, 0 );
    if ( getType( tree ) != STRING_LITERAL ) return;

    tree = vectorGet( tree->children, 0 );
    PyObject_CallFunction( callbacks->onDocstring, "si",
                           (const char *)(tree->toString( tree )->chars),
                           tree->getLine( tree ) );
    return;
}


void  processWhat( pANTLR3_BASE_TREE            tree,
                   struct instanceCallbacks *   callbacks )
{
    ANTLR3_UINT32   n = tree->children->count;
    ANTLR3_UINT32   k;

    for ( k = 0; k < n; ++k )
    {
        pANTLR3_BASE_TREE   child = vectorGet( tree->children, k );

        if ( getType( child ) == AS )
        {
            pANTLR3_BASE_TREE  asChild = vectorGet( child->children, 0 );
            PyObject_CallFunction( callbacks->onAs, "s",
                                   (const char *)(asChild->toString( asChild )->chars) );
            continue;
        }

        /* Otherwise it is what is imported */
        pANTLR3_COMMON_TOKEN    token = child->getToken( child );

        PyObject_CallFunction( callbacks->onWhat, "siii",
                               (const char *)(child->toString( child )->chars),
                               token->line,
                               token->charPosition + 1, /* Make it 1-based */
                               (void *)token->start - token->input->data );
    }
    return;
}


void  processImport( pANTLR3_BASE_TREE            tree,
                     struct instanceCallbacks *   callbacks )
{
    ANTLR3_UINT32   n = tree->children->count;
    ANTLR3_UINT32   k;

    /* There could be many imports in a single import statement */
    for ( k = 0; k < n; ++k )
    {
        /* The children must be the dotted name or what exported */
        pANTLR3_BASE_TREE   t = vectorGet( tree->children, k );
        char                name[ MAX_DOTTED_NAME_LENGTH ];

        if ( getType( t ) == DOTTED_NAME )
        {
            pANTLR3_COMMON_TOKEN    token = getDottedName( t, name );
            PyObject_CallFunction( callbacks->onImport, "siii",
                                   name,
                                   token->line,
                                   token->charPosition + 1, /* Make it 1-based */
                                   (void *)(token->start) - token->input->data );
        }

        if ( getType( t ) == WHAT )
        {
            processWhat( t, callbacks );
        }

        if ( getType( t ) == AS )
        {
            pANTLR3_BASE_TREE  asChild = vectorGet( t->children, 0 );
            PyObject_CallFunction( callbacks->onAs, "s",
                                   (const char *)(asChild->toString( asChild )->chars) );
        }

    }
    return;
}


const char *  processArguments( pANTLR3_BASE_TREE    tree,
                                PyObject *           onArg )
{
    const char *    firstArgument = NULL;   /* For non-static class members only,
                                               so it cannot be * or ** */

    ANTLR3_UINT32   i;
    ANTLR3_UINT32   n = tree->children->count;
    for ( i = 0; i < n; ++i )
    {
        pANTLR3_BASE_TREE   arg = vectorGet( tree->children, i );
        switch ( getType( arg ) )
        {
            case NAME_ARG:
                {
                    const char *    name = (const char *)(arg->toString( arg )->chars);
                    PyObject_CallFunction( onArg, "s", name );
                    if ( i == 0 ) firstArgument = name;
                }
                break;
            case STAR_ARG:
                {
                    const char *    name = (const char *)(arg->toString( arg )->chars);
                    char            augName[ strlen( name ) + 2 ];
                    augName[0] = '*';
                    strcpy( augName + 1, name );
                    PyObject_CallFunction( onArg, "s", augName );
                }
                break;
            case DBL_STAR_ARG:
                {
                    const char *    name = (const char *)(arg->toString( arg )->chars);
                    char            augName[ strlen( name ) + 3 ];
                    augName[0] = '*';
                    augName[1] = '*';
                    strcpy( augName + 2, name );
                    PyObject_CallFunction( onArg, "s", augName );
                }
                break;
        }
    }
    return firstArgument;
}


int processDecor( pANTLR3_BASE_TREE            tree,
                  struct instanceCallbacks *   callbacks )
{
    int     isStaticMethod = 0;
    char    name[ MAX_DOTTED_NAME_LENGTH ];     /* decor name */

    pANTLR3_COMMON_TOKEN    token = getDottedName( tree->children->get( tree->children, 0 ),
                                                   name );
    PyObject_CallFunction( callbacks->onDecorator, "siii",
                           name,
                           token->line,
                           token->charPosition + 1, /* Make it 1-based */
                           (void *)token->start - token->input->data );
    if ( strcmp( name, "staticmethod" ) == 0 )
    {
        isStaticMethod = 1;
    }

    /* decor arguments */
    if ( tree->children->count > 1 )
    {
        /* There are arguments - process the ARGUMENTS node*/
        processArguments( vectorGet( tree->children, 1 ),
                          callbacks->onDecoratorArgument );
    }

    return isStaticMethod;
}


void  processClassDefinition( pANTLR3_BASE_TREE            tree,
                              struct instanceCallbacks *   callbacks,
                              int                          objectsLevel,
                              enum Scope                   scope,
                              int                          entryLevel )
{
    pANTLR3_BASE_TREE       nameChild = vectorGet( tree->children, 0 );
    ANTLR3_UINT32           n = tree->children->count;
    ANTLR3_UINT32           k;
    pANTLR3_COMMON_TOKEN    token = nameChild->getToken( nameChild );

    /*
     * Positions of the 'def' keyword and the function name are saved in one 32
     * bit field 'charPosition'. See the grammar for details.
     * The code below extracts them as separate values.
     */

    ++objectsLevel;
    PyObject_CallFunction( callbacks->onClass, "siiiiii",
                           (nameChild->toString( nameChild ))->chars,
                           /* Function name line and pos */
                           token->line & 0xFFFF,
                           (token->charPosition & 0xFFFF) + 1, /* To make it 1-based */
                           (void *)token->start - token->input->data,
                           /* Keyword 'def' line and pos */
                           token->line >> 16,
                           (token->charPosition >> 16) + 1,    /* To make it 1-based */
                           objectsLevel );

    for ( k = 1; k < n; ++k )
    {
        pANTLR3_BASE_TREE   t = vectorGet( tree->children, k );
        switch ( getType( t ) )
        {
            case DECOR:
                processDecor( t, callbacks );
                continue;
            case CLASS_INHERITANCE:
                {
                    ANTLR3_UINT32   n = t->children->count;
                    ANTLR3_UINT32   k;
                    for ( k = 0; k < n; ++k )
                    {
                        pANTLR3_BASE_TREE   base = vectorGet( t->children, k );
                        PyObject_CallFunction( callbacks->onBaseClass, "s",
                                               (const char *)(base->toString( base )->chars) );
                    }
                }
                continue;
            case BODY:
                checkForDocstring( vectorGet( t->children, 0 ), callbacks );
                walk( t, callbacks, objectsLevel, CLASS_SCOPE, NULL, entryLevel );
                return;     /* Body child is the last */
        }
    }
    return;
}


void  processFuncDefinition( pANTLR3_BASE_TREE            tree,
                             struct instanceCallbacks *   callbacks,
                             int                          objectsLevel,
                             enum Scope                   scope,
                             int                          entryLevel )
{
    pANTLR3_BASE_TREE       nameChild = vectorGet( tree->children, 0 );
    ANTLR3_UINT32           n = tree->children->count;
    ANTLR3_UINT32           k;
    int                     isStaticMethod = 0;
    const char *            firstArgumentName = NULL; /* for class methods only */
    pANTLR3_COMMON_TOKEN    token = nameChild->getToken( nameChild );


    /*
     * Positions of the 'def' keyword and the function name are saved in one 32
     * bit field 'charPosition'. See the grammar for details.
     * The code below extracts them as separate values.
     */

    ++objectsLevel;
    PyObject_CallFunction( callbacks->onFunction, "siiiiii",
                           (nameChild->toString( nameChild ))->chars,
                           /* Function name line and pos */
                           token->line & 0xFFFF,
                           (token->charPosition & 0xFFFF) + 1, /* To make it 1-based */
                           (void *)token->start - token->input->data,
                           /* Keyword 'def' line and pos */
                           token->line >> 16,
                           (token->charPosition >> 16) + 1,    /* To make it 1-based */
                           objectsLevel );

    for ( k = 1; k < n; ++k )
    {
        pANTLR3_BASE_TREE   t = vectorGet( tree->children, k );
        switch ( getType( t ) )
        {
            case DECOR:
                isStaticMethod += processDecor( t, callbacks );
                continue;
            case ARGUMENTS:
                firstArgumentName = processArguments( t, callbacks->onArgument );
                continue;
            case BODY:
                checkForDocstring( vectorGet( t->children, 0 ), callbacks );
                {
                    enum Scope  newScope = FUNCTION_SCOPE; /* Avoid the compiler complains */
                    switch ( scope )
                    {
                        case GLOBAL_SCOPE:
                        case FUNCTION_SCOPE:
                        case CLASS_METHOD_SCOPE:
                        case CLASS_STATIC_METHOD_SCOPE:
                            newScope = FUNCTION_SCOPE;
                            break;
                        case CLASS_SCOPE:
                            /* It could be a static method if there is
                             * the '@staticmethod' decorator */
                            if ( isStaticMethod != 0 ) newScope = CLASS_STATIC_METHOD_SCOPE;
                            else                       newScope = CLASS_METHOD_SCOPE;
                            break;
                    }
                    walk( t, callbacks, objectsLevel, newScope,
                          firstArgumentName, entryLevel );
                }
                return;     /* Body child is the last */
        }
    }
    return;
}

void processAssign( pANTLR3_BASE_TREE   tree,
                    PyObject *          onVariable,
                    int                 objectsLevel )
{
    if ( tree->children->count == 0 )
        return;

    /* Step to the LHS part */
    tree = vectorGet( tree->children, 0 );

    /* iterate over the LHS names */
    ANTLR3_UINT32   i;
    ANTLR3_UINT32   n = tree->children->count;

    for ( i = 0; i < n; ++i )
    {
        pANTLR3_BASE_TREE   child = vectorGet( tree->children, i );

        if ( getType( child ) == HEAD_NAME )
        {
            if ( i == (n-1) )
            {
                child = vectorGet( child->children, 0 );

                pANTLR3_COMMON_TOKEN    token = child->getToken( child );
                PyObject_CallFunction( onVariable, "siiii",
                                       (child->toString( child ))->chars,
                                       token->line,
                                       token->charPosition + 1, /* Make it 1-based */
                                       (void *)token->start - token->input->data,
                                       objectsLevel );
                return;
            }

            /* Not last child - check the next */
            pANTLR3_BASE_TREE   nextChild = vectorGet( tree->children, i + 1 );
            if ( getType( nextChild ) == HEAD_NAME )
            {
                child = vectorGet( child->children, 0 );

                pANTLR3_COMMON_TOKEN    token = child->getToken( child );
                PyObject_CallFunction( onVariable, "siiii",
                                       (child->toString( child ))->chars,
                                       token->line,
                                       token->charPosition + 1, /* Make it 1-based */
                                       (void *)token->start - token->input->data,
                                       objectsLevel );
            }
        }
    }
    return;
}

void processInstanceMember( pANTLR3_BASE_TREE           tree,
                            struct instanceCallbacks *  callbacks,
                            const char *                firstArgName,
                            int                         objectsLevel )
{
    if ( firstArgName == NULL )         return;
    if ( tree->children->count == 0 )   return;

    /* Step to the LHS part */
    tree = vectorGet( tree->children, 0 );

    /* iterate over the LHS names */
    ANTLR3_UINT32   i;
    ANTLR3_UINT32   n = tree->children->count;

    for ( i = 0; i < n; ++i )
    {
        pANTLR3_BASE_TREE   child = vectorGet( tree->children, i );

        if ( getType( child ) != HEAD_NAME )
            continue;

        /* Check that the beginning of the name matches the method first
         * argument */
        pANTLR3_BASE_TREE   nameNode = vectorGet( child->children, 0 );
        if ( strcmp( firstArgName,
                     (const char *)nameNode->toString( nameNode )->chars ) != 0 )
            continue;

        /* OK, the beginning matches. Check that it has the trailing part */
        if ( i == (n-1) )
            continue;

        /* Do the step */
        child = vectorGet( tree->children, i+1 );
        if ( getType( child ) != TRAILER_NAME )
            continue;

        /* Check that there is no node after the trailer or the next one is the
         * HEAD_NAME */
        if ( (i+1) == (n-1) )
        {
            /* There is no more. Do the callback. */
            child = vectorGet( child->children, 0 );

            pANTLR3_COMMON_TOKEN    token = child->getToken( child );
            PyObject_CallFunction( callbacks->onInstanceAttribute, "siiii",
                                   (child->toString( child ))->chars,
                                   token->line,
                                   token->charPosition + 1, /* Make it 1-based */
                                   (void *)token->start - token->input->data,
                                   objectsLevel );
            return;
        }

        ++i;
        pANTLR3_BASE_TREE   lookAhead = vectorGet( tree->children, i+1 );
        if ( getType( lookAhead ) != HEAD_NAME )
            continue;

        /* Here it is, we should get it */
        child = vectorGet( child->children, 0 );

        pANTLR3_COMMON_TOKEN    token = child->getToken( child );
        PyObject_CallFunction( callbacks->onInstanceAttribute, "siiii",
                               (child->toString( child ))->chars,
                               token->line,
                               token->charPosition + 1, /* Make it 1-based */
                               (void *)token->start - token->input->data,
                               objectsLevel );
    }
    return;
}


void walk( pANTLR3_BASE_TREE            tree,
           struct instanceCallbacks *   callbacks,
           int                          objectsLevel,
           enum Scope                   scope,
           const char *                 firstArgName,
           int                          entryLevel )
{
    ++entryLevel;   // For module docstring only

    switch ( getType( tree ) )
    {
        case IMPORT_STMT:
            processImport( tree, callbacks );
            return;
        case CLASS_DEF:
            processClassDefinition( tree, callbacks,
                                    objectsLevel, scope, entryLevel );
            return;
        case FUNC_DEF:
            processFuncDefinition( tree, callbacks,
                                   objectsLevel, scope, entryLevel );
            return;
        case ASSIGN:
            if ( scope == GLOBAL_SCOPE )
                processAssign( tree, callbacks->onGlobal, objectsLevel );
            if ( scope == CLASS_SCOPE )
                processAssign( tree, callbacks->onClassAttribute, objectsLevel );
            if ( scope == CLASS_METHOD_SCOPE )
                processInstanceMember( tree, callbacks, firstArgName, objectsLevel );

            /* The other scopes are not interesting */
            return;

            /* Nodes which children and not of interest */
        case DEL_STMT:
        case PASS_STMT:
        case BREAK_STMT:
        case CONTINUE_STMT:
        case RETURN_STMT:
        case RAISE_STMT:
        case YIELD_STMT:
        case PRINT_STMT:
        case ASSERT_STMT:
        case EXEC_STMT:
        case GLOBAL_STMT:
            return;

            /* The rest needs to walk all the children */
        default:
            break;
    }

    // Walk the children
    if ( tree->children != NULL )
    {
        ANTLR3_UINT32   i;
        ANTLR3_UINT32   n = tree->children->count;

        for ( i = 0; i < n; i++ )
        {
            pANTLR3_BASE_TREE   t = vectorGet( tree->children, i );
            if ( (entryLevel == 1) && (i == 0) )
            {
                /* This could be a module docstring */
                checkForDocstring( t, callbacks );
            }
            walk( t, callbacks, objectsLevel,
                  scope, firstArgName, entryLevel );
        }
    }
    return;
}


/* Note: the lineNumber and lineStart cannot be calculated
 *       in searchForCoding(...) from ctx. It seems to me that
 *       the ctx has been updated at the time searchForCoding(...)
 *       is called. So these values are initialized before the rule
 *       and passed to searchForEncoding(...)
 */
void  searchForCoding( ppycfLexer     ctx,
                       char *         lineStart,
                       ANTLR3_UINT32  lineNumber )
{
    if ( ctx->onEncoding == NULL )
        return; /* Analysis is disabled after first found encoding */

    /* prepare regexp
     * I don't know why the original expression does not work here:
     * coding[=:]\s*([-\w.]+)
     */
    char *      pattern = "coding[=:]\\s*";
    regex_t     preg;
    regmatch_t  pmatch[1];

    if ( regcomp( &preg, pattern, 0 ) != 0 )
        return; /* Compile error */


    /* Find the end of the line and put \0 there */
    char *      endofline = lineStart;
    char        current;
    for ( ; ; ++endofline )
    {
        current = *endofline;
        if ( current == '\0' || current == '\n' ||
             current == '\r' )
            break;
    }

    /* replace the endofline char with '\0' */
    *endofline = '\0';

    /* check if the regexp matched */
    if ( regexec( &preg, lineStart, 1, pmatch, 0 ) == 0 )
    {
        /* The first char after the match is the first encoding char */
        char *  begin = lineStart + pmatch[0].rm_eo;
        char *  end = begin;
        char    last;

        /* Advance the end pointer */
        for ( ; ; ++end )
        {
            last = *end;
            if ( last == '\0' || isspace( last ) )
                break;
        }

        *end = '\0';
        PyObject_CallFunction( (PyObject*)ctx->onEncoding, "siii",
                               begin, lineNumber,
                               begin - lineStart + 1, /* Make it 1-based */
                               begin - (char *)ctx->pLexer->input->data );
        *end = last;
        ctx->onEncoding = NULL;
    }
    regfree( &preg );

    /* revert back the last line symbol */
    *endofline = current;
    return;
}
#endif


static int      unknownError = 0;

// The code is taken from the libantlr3 and modified to collect the message
// in a buffer and then call the python onError callback
void onError( pANTLR3_BASE_RECOGNIZER    recognizer,
              pANTLR3_UINT8 *            tokenNames )
{
    pANTLR3_PARSER          parser;
    pANTLR3_TREE_PARSER     tparser;
    pANTLR3_INT_STREAM      is;
    pANTLR3_STRING          ttext;
    pANTLR3_STRING          ftext;
    pANTLR3_EXCEPTION       ex;
    pANTLR3_COMMON_TOKEN    theToken;
    pANTLR3_BASE_TREE       theBaseTree;
    pANTLR3_COMMON_TREE     theCommonTree;

    size_t                  buffer_size = 4096;
    char                    buffer[ buffer_size ];
    size_t                  length = 0;

    // Retrieve some info for easy reading.
    ex    = recognizer->state->exception;
    ttext = NULL;


    // See if there is a 'filename' we can use
    if ( ex->streamName == NULL )
    {
        if ( ((pANTLR3_COMMON_TOKEN)(ex->token))->type == ANTLR3_TOKEN_EOF )
        {
            length = snprintf( buffer, buffer_size, "-end of input-(" );
        }
        else
        {
            length = snprintf( buffer, buffer_size, "-unknown source-(" );
        }
    }
    else
    {
        ftext = ex->streamName->to8( ex->streamName );
        length = snprintf( buffer, buffer_size, "%s(", ftext->chars );
    }

    // Next comes the line number

    if ( length < buffer_size )
        length += snprintf( buffer + length, buffer_size - length, "%d) ",
                            recognizer->state->exception->line );
    if ( length < buffer_size )
        length += snprintf( buffer + length, buffer_size - length, " : error %d : %s",
                            recognizer->state->exception->type,
                            (pANTLR3_UINT8) (recognizer->state->exception->message) );


    // How we determine the next piece is dependent on which thing raised the
    // error.
    switch ( recognizer->type )
    {
    case ANTLR3_TYPE_PARSER:

        // Prepare the knowledge we know we have
        parser   = (pANTLR3_PARSER) (recognizer->super);
        tparser  = NULL;
        is       = parser->tstream->istream;
        theToken = (pANTLR3_COMMON_TOKEN)(recognizer->state->exception->token);
        ttext    = theToken->toString( theToken );

        if ( length < buffer_size )
            length += snprintf( buffer + length, buffer_size - length,
                                ", at offset %d",
                                recognizer->state->exception->charPositionInLine );
        if  ( theToken != NULL )
        {
            if ( theToken->type == ANTLR3_TOKEN_EOF )
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length, ", at <EOF>" );
            }
            else
            {
                // Guard against null text in a token
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        "\n    near %s\n    ",
                                        ttext == NULL ? (pANTLR3_UINT8)"<no text for the token>" : ttext->chars );
            }
        }
        break;

    case ANTLR3_TYPE_TREE_PARSER:

        tparser     = (pANTLR3_TREE_PARSER) (recognizer->super);
        parser      = NULL;
        is          = tparser->ctnstream->tnstream->istream;
        theBaseTree = (pANTLR3_BASE_TREE)(recognizer->state->exception->token);
        ttext       = theBaseTree->toStringTree( theBaseTree );

        if  (theBaseTree != NULL)
        {
            theCommonTree = (pANTLR3_COMMON_TREE) theBaseTree->super;

            if ( theCommonTree != NULL )
            {
                theToken = (pANTLR3_COMMON_TOKEN) theBaseTree->getToken( theBaseTree );
            }
            if ( length < buffer_size )
                length += snprintf( buffer + length, buffer_size - length,
                                    ", at offset %d", theBaseTree->getCharPositionInLine( theBaseTree ) );
            if ( length < buffer_size )
                length += snprintf( buffer + length, buffer_size - length,
                                    ", near %s", ttext->chars );
        }
        break;

    default:

        if ( length < buffer_size )
            length += snprintf( buffer + length, buffer_size - length,
                                "Base recognizer function displayRecognitionError called by unknown parser type - provide override for this function\n" );
        if ( recognizer->userData != NULL )
        {
            PyObject_CallFunction( (PyObject *)(recognizer->userData), "s", buffer );
        }
        else
        {
            unknownError += 1;
        }
        return;
    }

    // Although this function should generally be provided by the implementation, this one
    // should be as helpful as possible for grammar developers and serve as an example
    // of what you can do with each exception type. In general, when you make up your
    // 'real' handler, you should debug the routine with all possible errors you expect
    // which will then let you be as specific as possible about all circumstances.
    //
    // Note that in the general case, errors thrown by tree parsers indicate a problem
    // with the output of the parser or with the tree grammar itself. The job of the parser
    // is to produce a perfect (in traversal terms) syntactically correct tree, so errors
    // at that stage should really be semantic errors that your own code determines and handles
    // in whatever way is appropriate.
    //
    switch ( ex->type )
    {
    case ANTLR3_UNWANTED_TOKEN_EXCEPTION:

        // Indicates that the recognizer was fed a token which seesm to be
        // spurious input. We can detect this when the token that follows
        // this unwanted token would normally be part of the syntactically
        // correct stream. Then we can see that the token we are looking at
        // is just something that should not be there and throw this exception.
        if ( tokenNames == NULL )
        {
            if ( length < buffer_size )
                length += snprintf( buffer + length, buffer_size - length,
                                    " : Extraneous input..." );
        }
        else
        {
            if ( ex->expecting == ANTLR3_TOKEN_EOF )
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        " : Extraneous input - expected <EOF>\n" );
            }
            else
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        " : Extraneous input - expected %s ...\n", tokenNames[ ex->expecting ] );
            }
        }
        break;

    case ANTLR3_MISSING_TOKEN_EXCEPTION:

        // Indicates that the recognizer detected that the token we just
        // hit would be valid syntactically if preceeded by a particular 
        // token. Perhaps a missing ';' at line end or a missing ',' in an
        // expression list, and such like.
        if ( tokenNames == NULL )
        {
            if ( length < buffer_size )
                length += snprintf( buffer + length, buffer_size - length,
                                    " : Missing token (%d)...\n", ex->expecting );
        }
        else
        {
            if ( ex->expecting == ANTLR3_TOKEN_EOF )
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        " : Missing <EOF>\n" );
            }
            else
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        " : Missing %s \n", tokenNames[ ex->expecting ] );
            }
        }
        break;

    case ANTLR3_RECOGNITION_EXCEPTION:

        // Indicates that the recognizer received a token
        // in the input that was not predicted. This is the basic exception type 
        // from which all others are derived. So we assume it was a syntax error.
        // You may get this if there are not more tokens and more are needed
        // to complete a parse for instance.
        //
        if ( length < buffer_size )
            length += snprintf( buffer + length, buffer_size - length,
                                " : syntax error...\n" );
        break;

    case ANTLR3_MISMATCHED_TOKEN_EXCEPTION:

        // We were expecting to see one thing and got another. This is the
        // most common error if we coudl not detect a missing or unwanted token.
        // Here you can spend your efforts to
        // derive more useful error messages based on the expected
        // token set and the last token and so on. The error following
        // bitmaps do a good job of reducing the set that we were looking
        // for down to something small. Knowing what you are parsing may be
        // able to allow you to be even more specific about an error.
        if ( tokenNames == NULL )
        {
            if ( length < buffer_size )
                length += snprintf( buffer + length, buffer_size - length,
                                    " : syntax error...\n" );
        }
        else
        {
            if ( ex->expecting == ANTLR3_TOKEN_EOF )
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        " : expected <EOF>\n" );
            }
            else
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        " : expected %s ...\n", tokenNames[ ex->expecting ] );
            }
        }
        break;

    case ANTLR3_NO_VIABLE_ALT_EXCEPTION:

        // We could not pick any alt decision from the input given
        // so god knows what happened - however when you examine your grammar,
        // you should. It means that at the point where the current token occurred
        // that the DFA indicates nowhere to go from here.
        if ( length < buffer_size )
            length += snprintf( buffer + length, buffer_size - length,
                                " : cannot match to any predicted input...\n" );

        break;

    case ANTLR3_MISMATCHED_SET_EXCEPTION:

        {
            ANTLR3_UINT32   count;
            ANTLR3_UINT32   bit;
            ANTLR3_UINT32   size;
            ANTLR3_UINT32   numbits;
            pANTLR3_BITSET  errBits;

            // This means we were able to deal with one of a set of
            // possible tokens at this point, but we did not see any
            // member of that set.
            if ( length < buffer_size )
                length += snprintf( buffer + length, buffer_size - length,
                                    " : unexpected input...\n  expected one of : " );

            // What tokens could we have accepted at this point in the
            // parse?
            count   = 0;
            errBits = antlr3BitsetLoad( ex->expectingSet );
            numbits = errBits->numBits( errBits );
            size    = errBits->size( errBits );

            if  (size > 0)
            {
                // However many tokens we could have dealt with here, it is usually
                // not useful to print ALL of the set here. I arbitrarily chose 8
                // here, but you should do whatever makes sense for you of course.
                // No token number 0, so look for bit 1 and on.
                for ( bit = 1; bit < numbits && count < 8 && count < size; bit++ )
                {
                    // TODO: This doesn;t look right - should be asking if the bit is set!!
                    if ( tokenNames[ bit ] )
                    {
                        if ( length < buffer_size )
                            length += snprintf( buffer + length, buffer_size - length,
                                                "%s%s", count > 0 ? ", " : "", tokenNames[ bit ] );
                        count++;
                    }
                }
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        "\n" );
            }
            else
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        "Actually dude, we didn't seem to be expecting anything here, or at least\n" );
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        "I could not work out what I was expecting, like so many of us these days!\n" );
            }
        }
        break;

    case ANTLR3_EARLY_EXIT_EXCEPTION:

        // We entered a loop requiring a number of token sequences
        // but found a token that ended that sequence earlier than
        // we should have done.
        if ( length < buffer_size )
            length += snprintf( buffer + length, buffer_size - length,
                                " : missing elements...\n" );
        break;

    default:

        // We don't handle any other exceptions here, but you can
        // if you wish. If we get an exception that hits this point
        // then we are just going to report what we know about the
        // token.
        if ( length < buffer_size )
            length += snprintf( buffer + length, buffer_size - length,
                                " : syntax not recognized...\n" );
        break;
    }

    if ( recognizer->userData != NULL )
    {
        PyObject_CallFunction( (PyObject *)(recognizer->userData), "s", buffer );
    }
    else
    {
        unknownError += 1;
    }

    // Here you have the token that was in error which if this is
    // the standard implementation will tell you the line and offset
    // and also record the address of the start of the line in the
    // input stream. You could therefore print the source line and so on.
    // Generally though, I would expect that your lexer/parser will keep
    // its own map of lines and source pointers or whatever as there
    // are a lot of specific things you need to know about the input
    // to do something like that.
    // Here is where you do it though :-).
}


// The function is taken from the libantlr3 and modified to collect the message
// in a buffer and then call the python onLexerError callback.
// Original function: antlr3lexer.c:407 displayRecognitionError(...)
void
onLexerError( pANTLR3_BASE_RECOGNIZER   recognizer,
              pANTLR3_UINT8 *           tokenNames)
{
    pANTLR3_LEXER           lexer;
    pANTLR3_EXCEPTION       ex;
    pANTLR3_STRING          ftext;

    size_t                  buffer_size = 4096;
    char                    buffer[ buffer_size ];
    size_t                  length = 0;

    lexer = (pANTLR3_LEXER)(recognizer->super);
    ex    = lexer->rec->state->exception;

    // See if there is a 'filename' we can use
    //
    if (ex->name == NULL)
    {
        length = snprintf( buffer, buffer_size, "-unknown source-(" );
    }
    else
    {
        ftext = ex->streamName->to8(ex->streamName);
        length = snprintf( buffer, buffer_size, "%s(", ftext->chars );
    }

    if ( length < buffer_size )
        length += snprintf( buffer + length, buffer_size - length, "%d) ",
                            recognizer->state->exception->line);
    if ( length < buffer_size )
        length += snprintf( buffer + length, buffer_size - length,
                            ": lexer error %d :\n\t%s at offset %d, ",
                            ex->type,
                            (pANTLR3_UINT8)(ex->message),
                            ex->charPositionInLine + 1 );
    {
        ANTLR3_INT32    width;

        width = ANTLR3_UINT32_CAST(( (pANTLR3_UINT8)(lexer->input->data) +
                                     (lexer->input->size(lexer->input) )) -
                                     (pANTLR3_UINT8)(ex->index));

        if (width >= 1)
        {
            if (isprint(ex->c))
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        "near '%c' :\n", ex->c );
            }
            else
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        "near char(%#02X) :\n", (ANTLR3_UINT8)(ex->c) );
            }
            if ( length < buffer_size )
                length += snprintf( buffer + length, buffer_size - length,
                                    "\t%.*s\n", width > 20 ? 20 : width ,((pANTLR3_UINT8)ex->index) );
        }
        else
        {
            if ( length < buffer_size )
                length += snprintf( buffer + length, buffer_size - length,
                                    "(end of input).\n\t This indicates a poorly specified lexer RULE\n\t or unterminated input element such as: \"STRING[\"]\n");
            if ( length < buffer_size )
                length += snprintf( buffer + length, buffer_size - length,
                                    "\t The lexer was matching from line %d, offset %d, which\n\t ",
                                   (ANTLR3_UINT32)(lexer->rec->state->tokenStartLine),
                                   (ANTLR3_UINT32)(lexer->rec->state->tokenStartCharPositionInLine) );
            width = ANTLR3_UINT32_CAST(((pANTLR3_UINT8)(lexer->input->data)+(lexer->input->size(lexer->input))) - (pANTLR3_UINT8)(lexer->rec->state->tokenStartCharIndex));

            if (width >= 1)
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        "looks like this:\n\t\t%.*s\n", width > 20 ? 20 : width ,(pANTLR3_UINT8)(lexer->rec->state->tokenStartCharIndex));
            }
            else
            {
                if ( length < buffer_size )
                    length += snprintf( buffer + length, buffer_size - length,
                                        "is also the end of the line, so you must check your lexer rules\n");
            }
        }
    }

    if ( recognizer->userData != NULL )
    {
        PyObject_CallFunction( (PyObject *)(recognizer->userData), "s", buffer );
    }
}


static PyObject *
parse_input( pANTLR3_INPUT_STREAM           input,
             struct instanceCallbacks *     callbacks )
{
    if ( input == NULL )
    {
        PyErr_SetString( PyExc_RuntimeError, "Cannot open file/memory buffer" );
        return NULL;
    }

    ppycfLexer lxr = pycfLexerNew( input );
    if ( lxr == NULL )
    {
        input->close( input );
        PyErr_SetString( PyExc_RuntimeError, "Cannot create lexer" );
        return NULL;
    }

    pANTLR3_COMMON_TOKEN_STREAM tstream = antlr3CommonTokenStreamSourceNew( ANTLR3_SIZE_HINT,
                                                                            TOKENSOURCE( lxr ) );
    if ( tstream == NULL )
    {
        input->close( input );
        lxr->free( lxr );
        PyErr_SetString( PyExc_RuntimeError, "Cannot create token stream" );
        return NULL;
    }
    tstream->discardOffChannelToks( tstream, ANTLR3_TRUE );

    // Create parser
    ppycfParser psr = pycfParserNew( tstream );
    if ( psr == NULL )
    {
        input->close( input );
        tstream->free( tstream );
        lxr->free( lxr );
        PyErr_SetString( PyExc_RuntimeError, "Cannot create parser" );
        return NULL;
    }

    /* Memorize callbacks for coding and errors */
//    lxr->onEncoding = callbacks->onEncoding;

    lxr->pLexer->rec->userData = callbacks->onLexerError;
    lxr->pLexer->rec->displayRecognitionError = onLexerError;
    psr->pParser->rec->userData = callbacks->onError;
    psr->pParser->rec->displayRecognitionError = onError;

    /* Bug in the run-time library */
    lxr->pLexer->input->charPositionInLine = 0;


    /* Parse... */
    pANTLR3_BASE_TREE  tree = psr->file_input( psr ).tree;

    if ( tree == NULL )
    {
        input->close( input );
        tstream->free( tstream );
        lxr->free( lxr );
        PyErr_SetString( PyExc_RuntimeError, "Cannot parse python code" );
        return NULL;
    }

    /* Walk the tree and populate the python structures */
    walk( tree, callbacks, -1, GLOBAL_SCOPE, NULL, 0 );

    /* cleanup */
    psr->free( psr );
    tstream->free( tstream );
    lxr->free( lxr );
    input->close( input );

    if ( unknownError != 0 )
    {
        // This is a fallback which should not happened
        PyObject_CallFunction( callbacks->onError, "s",
                               "Unknown error." );
        unknownError = 0;
    }

    return Py_BuildValue( "i", 0 );
}



/* Parses the given code. Returns 0 if all is OK */
static char pycf_from_mem_doc[] = "Get control flow module info from memory";
static PyObject *
pycf_from_mem( PyObject *  self,      /* unused */
               PyObject *  args )
{
    PyObject *                  callbackClass;
    char *                      content;
    struct instanceCallbacks    callbacks;

    /* Parse the passed arguments */
    if ( ! PyArg_ParseTuple( args, "Os", & callbackClass, & content ) )
    {
        PyErr_SetString( PyExc_TypeError, "Incorrect arguments. "
                                          "Expected: callback class "
                                          "instance and buffer with python code" );
        return NULL;
    }

    /* Check the passed argument */
    if ( ! content )
    {
        PyErr_SetString( PyExc_TypeError, "Incorrect memory buffer" );
        return NULL;
    }
    if ( (! callbackClass) || (! PyInstance_Check(callbackClass)) )
    {
        PyErr_SetString( PyExc_TypeError, "Incorrect callback class instance" );
        return NULL;
    }

    /* Get pointers to the members */
    if ( getInstanceCallbacks( callbackClass, & callbacks ) != 0 )
    {
        return NULL;
    }

    /* Start the parser business */
    pANTLR3_INPUT_STREAM    input = antlr3NewAsciiStringInPlaceStream( (pANTLR3_UINT8) content,
                                                                       strlen( content ), NULL );

    /* Dirty hack:
     * it's a problem if a comment is the last one in the file and it does not
     * have EOL at the end. It's easier to add EOL here (if EOL is not the last
     * character) than to change the grammar.
     * The \0 byte is used for temporary injection of EOL.
     */
    int     eolAddedAt = 0;
    if ( input != NULL )
    {
        if ( input->sizeBuf > 0 )
        {
            if ( ((char*)(input->data))[ input->sizeBuf - 1 ] != '\n' )
            {
                eolAddedAt = input->sizeBuf;
                ((char*)(input->data))[ eolAddedAt ] = '\n';
                input->sizeBuf += 1;
            }
        }
    }

    PyObject *      result = parse_input( input, & callbacks );

    /* Revert the hack changes back if needed.
     * Input is closed here so the original content pointer is used.
     */
    if ( eolAddedAt != 0 )
    {
        content[ eolAddedAt ] = 0;
    }
    return result;
}



static PyMethodDef _cdm_cf_parser_methods[] =
{
    { "getControlFlowFromMemory", pycf_from_mem, METH_VARARGS, pycf_from_mem_doc },
    { NULL, NULL, 0, NULL }
};


#if PY_MAJOR_VERSION < 3
    /* Python 2 initialization */
    void init_cdmcfparser( void )
    {
        PyObject *  module = Py_InitModule( "_cdmcfparser", _cdm_cf_parser_methods );
        PyModule_AddStringConstant( module, "version", CDM_CF_PARSER_VERSION );
    }
#else
    /* Python 3 initialization */
    static struct PyModuleDef _cdm_cf_parser_module =
    {
        PyModuleDef_HEAD_INIT,
        "_cdmcfparser",
        NULL,
        -1,
        _cdm_cf_parser_methods
    };

    PyMODINIT_FUNC
    PyInit__cdmpycfparser( void )
    {
        PyObject *  module;
        module = PyModule_Create( & _cdm_cf_parser_module );
        PyModule_AddStringConstant( module, "version", CDM_CF_PARSER_VERSION );
        return module;
    }
#endif

