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
 * Python control flow parser implementation
 */


#include <string.h>
#include <list>

#include "cflowparser.hpp"
#include "cflowfragmenttypes.hpp"
#include "cflowfragments.hpp"
#include "cflowcomments.hpp"
#include "cflowutils.hpp"

/*
 * The python grammar definition conflicts with the standard library
 * declarations on some platforms, particularly on MacOS: python uses
 * #define test 305
 * while the library has
 * bitset<_Size>::test(size_t __pos) const
 * Thus the python headers must be after certain PyCXX library includes which
 * use the standard library.
 */
#include <Python.h>
#include <node.h>
#include <grammar.h>
#include <parsetok.h>
#include <graminit.h>
#include <errcode.h>
#include <token.h>


extern grammar      _PyParser_Grammar;  /* From graminit.c */


static FragmentBase *
walk( Context *             context,
      node *                tree,
      FragmentBase *        parent,
      Py::List &            flow,
      bool                  docstrProcessed );



/* Copied and adjusted from 
 * static void err_input(perrdetail *err)
 */
static void getErrorMessage( perrdetail *  err,
                             int &  line, int &  column,
                             std::string &  message )
{
    line = err->lineno;
    column = err->offset;

    switch ( err->error )
    {
        case E_ERROR:
            message = "execution error";
            return;
        case E_SYNTAX:
            if ( err->expected == INDENT )
                message = "expected an indented block";
            else if ( err->token == INDENT )
                message = "unexpected indent";
            else if (err->token == DEDENT)
                message = "unexpected unindent";
            else
                message = "invalid syntax";
            break;
        case E_TOKEN:
            message = "invalid token";
            break;
        case E_EOFS:
            message = "EOF while scanning triple-quoted string literal";
            break;
        case E_EOLS:
            message = "EOL while scanning string literal";
            break;
        case E_INTR:
            message = "keyboard interrupt";
            goto cleanup;
        case E_NOMEM:
            message = "no memory";
            goto cleanup;
        case E_EOF:
            message = "unexpected EOF while parsing";
            break;
        case E_TABSPACE:
            message = "inconsistent use of tabs and spaces in indentation";
            break;
        case E_OVERFLOW:
            message = "expression too long";
            break;
        case E_DEDENT:
            message = "unindent does not match any outer indentation level";
            break;
        case E_TOODEEP:
            message = "too many levels of indentation";
            break;
        case E_DECODE:
            message = "decode error";
            break;
        case E_LINECONT:
            message = "unexpected character after line continuation character";
            break;
        default:
            {
                char    code[ 32 ];
                sprintf( code, "%d", err->error );
                message = "unknown parsing error (error code " +
                           std::string( code ) + ")";
                break;
            }
    }

    if ( err->text != NULL )
        message += std::string( "\n" ) + err->text;

    cleanup:
    if (err->text != NULL)
    {
        PyObject_FREE(err->text);
        err->text = NULL;
    }
    return;
}

/* Provides the total number of lines in the code */
static int getTotalLines( node *  tree )
{
    if ( tree == NULL )
        return -1;

    if ( tree->n_type != file_input )
        tree = &(tree->n_child[ 0 ]);

    assert( tree->n_type == file_input );
    for ( int k = 0; k < tree->n_nchildren; ++k )
    {
        node *  child = &(tree->n_child[ k ]);
        if ( child->n_type == ENDMARKER )
            return child->n_lineno;
    }
    return -1;
}


static node *  findLastPart( node *  tree )
{
    while ( tree->n_nchildren > 0 )
        tree = & (tree->n_child[ tree->n_nchildren - 1 ]);
    return tree;
}

static node *  findChildOfType( node *  from, int  type )
{
    for ( int  k = 0; k < from->n_nchildren; ++k )
        if ( from->n_child[ k ].n_type == type )
            return & (from->n_child[ k ]);
    return NULL;
}

static node *
findChildOfTypeAndValue( node *  from, int  type, const char *  val )
{
    for ( int  k = 0; k < from->n_nchildren; ++k )
        if ( from->n_child[ k ].n_type == type )
            if ( strcmp( from->n_child[ k ].n_str, val ) == 0 )
                return & (from->n_child[ k ]);
    return NULL;
}

/* Searches for a certain  node among the first children */
static node *
skipToNode( node *  tree, int nodeType )
{
    if ( tree == NULL )
        return NULL;

    for ( ; ; )
    {
        if ( tree->n_type == nodeType )
            return tree;
        if ( tree->n_nchildren < 1 )
            return NULL;
        tree = & ( tree->n_child[ 0 ] );
    }
    return NULL;
}

/* returns 1, 2, 3 or 4,
   i.e. the number of leading quotes used in a string literal part */
static size_t getStringLiteralPrefixLength( node *  tree )
{
    /* tree must be of STRING type */
    assert( tree->n_type == STRING );
    if ( strncmp( tree->n_str, "\"\"\"", 3 ) == 0 )
        return 3;
    if ( strncmp( tree->n_str, "'''", 3 ) == 0 )
        return 3;
    if ( strncmp( tree->n_str, "r\"\"\"", 4 ) == 0 )
        return 4;
    if ( strncmp( tree->n_str, "r'''", 4 ) == 0 )
        return 4;
    if ( strncmp( tree->n_str, "u\"\"\"", 4 ) == 0 )
        return 4;
    if ( strncmp( tree->n_str, "u'''", 4 ) == 0 )
        return 4;
    if ( strncmp( tree->n_str, "f\"\"\"", 4 ) == 0 )
        return 4;
    if ( strncmp( tree->n_str, "f'''", 4 ) == 0 )
        return 4;
    if ( strncmp( tree->n_str, "r\"", 2 ) == 0 )
        return 2;
    if ( strncmp( tree->n_str, "r'", 2 ) == 0 )
        return 2;
    if ( strncmp( tree->n_str, "u\"", 2 ) == 0 )
        return 2;
    if ( strncmp( tree->n_str, "u'", 2 ) == 0 )
        return 2;
    if ( strncmp( tree->n_str, "f\"", 2 ) == 0 )
        return 2;
    if ( strncmp( tree->n_str, "f'", 2 ) == 0 )
        return 2;
    return 1;
}


static void
getNewLineParts( const char *  str,
                 std::deque<const char *>  &  parts,
                 int &  newLineCount,
                 int &  charCount )
{
    newLineCount = 0;
    charCount = 0;

    bool    found = false;
    while ( * str != '\0' )
    {
        if ( * str == '\r' )
        {
            if ( * (str + 1 ) == '\n' )
            {
                ++str;
                ++charCount;
            }
            found = true;
        }
        else if ( * str == '\n' )
            found = true;

        if ( found )
        {
            ++newLineCount;
            parts.push_back( str );
            found = false;
        }

        ++str;
        ++charCount;
    }
}


static void
updateBegin( Fragment *  f, node *  n, Context *   context )
{
    #if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 8
        // Python 3.8 has the first line and column set correct
        f->beginLine = n->n_lineno;
        f->beginPos = n->n_col_offset + 1;
        f->begin = context->lineShifts[ f->beginLine ] + n->n_col_offset;
    #else
        // Python 3.7 and below have -1 for multiline string literals
        if ( n->n_col_offset == -1 )
        {
            // Bad case: it is a multiline string literal so need to guess
            // all the values
            node *      lastPart = skipToNode( n, STRING );
            if ( lastPart )
            {
                if ( lastPart->n_col_offset == -1 )
                {
                    std::deque< const char * >  newLines;
                    int                         newLineCount;
                    int                         charCount;

                    getNewLineParts( lastPart->n_str, newLines,
                                     newLineCount, charCount );
                    f->beginLine = n->n_lineno - newLineCount;
                    f->begin = context->lineShifts[ n->n_lineno ] +
                               strlen( newLines.back() + 1 ) - charCount;
                    f->beginPos = f->begin -
                                  context->lineShifts[ f->beginLine ] + 1;
                    return;
                }
            }
            // This should not really happened: fall through
        }

        // Easy case: the proper info is in the node
        f->beginLine = n->n_lineno;
        f->beginPos = n->n_col_offset + 1;
        f->begin = context->lineShifts[ f->beginLine ] + n->n_col_offset;
    #endif
}


static void
updateEnd( Fragment *  f, node *  n, Context *   context )
{
    if ( n->n_str == NULL ) {
        f->end = context->lineShifts[ n->n_lineno ] + n->n_col_offset;
        f->endLine = n->n_lineno;
        f->endPos = n->n_col_offset;
        return;
    }

    if ( n->n_type == STRING )
    {
        if ( getStringLiteralPrefixLength( n ) >= 3 )
        {
            std::deque< const char * >  newLines;
            int                         newLineCount;
            int                         charCount;

            getNewLineParts( n->n_str, newLines, newLineCount, charCount );

            #if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 8
                // Python 3.8 has the first line available for multiline
                // string literals
                f->endLine = n->n_lineno + newLineCount;
            #else
                // Python 3.7 has only the end line correct for multiline
                // string literals
                f->endLine = n->n_lineno;
            #endif

            if ( newLineCount == 0 )
            {
                f->endPos = n->n_col_offset + charCount;
            }
            else
            {
                const char *    lastNewLine = newLines.back();
                f->endPos = strlen( lastNewLine  + 1 );
            }
            f->end = context->lineShifts[ f->endLine ] + f->endPos - 1;
            return;
        }
    }

    int     lastPartLength = strlen( n->n_str );
    f->end = context->lineShifts[ n->n_lineno ] +
             n->n_col_offset + lastPartLength - 1;
    f->endLine = n->n_lineno;
    f->endPos = n->n_col_offset + lastPartLength;
}


// It also discards the comment from the deque if it is a bang line
static FragmentBase *
checkForBangLine( const char *  buffer,
                  ControlFlow *  controlFlow,
                  std::deque< CommentLine > &  comments )
{
    if ( comments.empty() )
        return NULL;

    CommentLine &       comment( comments.front() );
    if ( comment.line == 1 && comment.end - comment.begin > 1 &&
         buffer[ comment.begin + 1 ] == '!' )
    {
        // That's a bang line
        BangLine *          bangLine( new BangLine );
        bangLine->parent = controlFlow;
        bangLine->begin = comment.begin;
        bangLine->end = comment.end;
        bangLine->beginLine = 1;
        bangLine->beginPos = comment.pos;
        bangLine->endLine = 1;
        bangLine->endPos = bangLine->beginPos + ( bangLine->end -
                                                  bangLine->begin );
        controlFlow->bangLine = Py::asObject( bangLine );
        controlFlow->updateBeginEnd( bangLine );

        // Discard the shebang comment
        comments.pop_front();

        return bangLine;
    }
    return NULL;
}


// It also discards the comment from the deque
static FragmentBase *
processEncoding( const char *   buffer,
                 node *         tree,
                 ControlFlow *  controlFlow,
                 std::deque< CommentLine > &  comments )
{
    /* Unfortunately, the parser does not provide the position of the encoding
     * so it needs to be calculated
     */

    /* Another problem is that the python parser can replace what is found
       in the source code to a 'normal' name. The rules in the python source
       code are (Parser/tokenizer.c):
       'utf-8', 'utf-8-...' -> 'utf-8'
       'latin-1', 'iso-8859-1', 'iso-latin-1', 'latin-1-...',
       'iso-8859-1-...', 'iso-latin-1-...' -> 'iso-8859-1'

       Moreover, the first 12 characters may be converted as follows:
       '_' -> '-'
       all the other -> tolower()
    */

    if ( comments.empty() )
        return NULL;

    // It could be that the very first line starts with '#' however it is
    // not a hash bang line. In this case the encoding is in the second line.
    const CommentLine *     comment( & comments.front() );
    std::string             content( & buffer[ comment->begin ],
                                     comment->end - comment->begin + 1 );
    CommentLine             temp;
    bool                    needInsertBack( false );
    if ( strstr( content.c_str(), "coding" ) == NULL )
    {
        temp = comments.front();
        comments.pop_front();
        comment = & comments.front();
        needInsertBack = true;
    }

    EncodingLine *      encodingLine( new EncodingLine );

    encodingLine->normalizedName = Py::String( tree->n_str );
    encodingLine->parent = controlFlow;
    encodingLine->begin = comment->begin;
    encodingLine->end = comment->end;
    encodingLine->beginLine = comment->line;
    encodingLine->beginPos = comment->pos;
    encodingLine->endLine = comment->line;
    encodingLine->endPos = encodingLine->beginPos + ( encodingLine->end -
                                                      encodingLine->begin );
    controlFlow->encodingLine = Py::asObject( encodingLine );
    controlFlow->updateBeginEnd( encodingLine );

    comments.pop_front();
    if ( needInsertBack )
        comments.push_front( temp );
    return encodingLine;
}


// Detects the leading comments block last line. -1 if none found
static int
detectLeadingBlock( Context * context, int  limit, INT_TYPE  blockShift = 0 )
{
    if ( context->comments->empty() )
        return -1;

    const CommentLine &     first = context->comments->front();
    if ( first.line >= limit )
        return -1;

    int     lastInBlock( first.line );
    for ( std::deque< CommentLine >::const_iterator
            k = context->comments->begin();
            k != context->comments->end(); ++k )
    {
        if ( k->line >= limit )
            break;

        if ( k->pos < blockShift )
            break;

        if ( k->line > lastInBlock + 1 )
            break;

        lastInBlock = k->line;
    }
    return lastInBlock;
}


// Parent is not set here
static Fragment *
createCommentFragment( const CommentLine &  comment )
{
    Fragment *      part( new Fragment );
    part->begin = comment.begin;
    part->end = comment.end;
    part->beginLine = comment.line;
    part->beginPos = comment.pos;
    part->endLine = comment.line;
    part->endPos = comment.pos + ( comment.end - comment.begin );
    return part;
}


static void
addLeadingCMLComment( Context *  context,
                      CMLComment *  leadingCML,
                      bool  consumeAllAsLeading,
                      int  leadingLastLine,
                      int  firstStatementLine,
                      FragmentBase *  statementAsParent,
                      FragmentWithComments *  statement,
                      FragmentBase *  flowAsParent,
                      Py::List &  flow )
{
    leadingCML->extractProperties( context );
    if ( leadingLastLine + 1 == firstStatementLine ||
         consumeAllAsLeading )
    {
        statementAsParent->updateBeginEnd( leadingCML );
        statement->leadingCMLComments.append( Py::asObject( leadingCML ) );
    }
    else
    {
        flowAsParent->updateBeginEnd( leadingCML );
        flow.append( Py::asObject( leadingCML ) );
    }
    return;
}


static void
injectOneLeadingComment( Context *  context,
                         Py::List &  flow,
                         FragmentBase *  flowAsParent,
                         FragmentWithComments *  statement, // could be NULL
                         FragmentBase *  statementAsParent, // could be NULL
                         int  firstStatementLine,
                         bool  consumeAllAsLeading,
                         int  leadingLastLine )
{
    CMLComment *    leadingCML = NULL;
    Comment *       leading = NULL;

    while ( ! context->comments->empty() )
    {
        CommentLine &       comment = context->comments->front();
        if ( comment.line > leadingLastLine )
            break;

        if ( comment.type == CML_COMMENT )
        {
            if ( leadingCML != NULL )
            {
                addLeadingCMLComment( context, leadingCML, consumeAllAsLeading,
                                      leadingLastLine, firstStatementLine,
                                      statementAsParent, statement,
                                      flowAsParent, flow );
                leadingCML = NULL;
            }

            Fragment *      part( createCommentFragment( comment ) );
            if ( leadingLastLine + 1 == firstStatementLine ||
                 consumeAllAsLeading )
                part->parent = statementAsParent;
            else
                part->parent = flowAsParent;

            leadingCML = new CMLComment;
            leadingCML->parent = part->parent;
            leadingCML->updateBeginEnd( part );
            leadingCML->parts.append( Py::asObject( part ) );
        }


        if ( comment.type == CML_COMMENT_CONTINUE )
        {
            if ( leadingCML == NULL )
            {
                // Bad thing: someone may deleted the proper
                // cml comment beginning so the comment is converted into a
                // regular one. The regular comment will be handled below.
                context->flow->addWarning( comment.line, -1,
                                           "Continue of the CML comment "
                                           "without the beginning. "
                                           "Treat it as a regular comment." );
                comment.type = REGULAR_COMMENT;
            }
            else
            {
                if ( leadingCML->endLine + 1 != comment.line )
                {
                    // Bad thing: whether someone deleted the beginning of
                    // the cml comment or inserted an empty line between.
                    // So convert the comment into a regular one.
                    context->flow->addWarning( comment.line, -1,
                                               "Continue of the CML comment "
                                               "without the beginning. "
                                               "Treat it as a regular comment." );
                    comment.type = REGULAR_COMMENT;
                }
                else
                {
                    Fragment *      part( createCommentFragment( comment ) );
                    if ( leadingLastLine + 1 == firstStatementLine ||
                         consumeAllAsLeading )
                        part->parent = statementAsParent;
                    else
                        part->parent = flowAsParent;

                    leadingCML->updateEnd( part );
                    leadingCML->parts.append( Py::asObject( part ) );
                }
            }
        }

        if ( comment.type == REGULAR_COMMENT )
        {
            if ( leadingCML != NULL )
            {
                addLeadingCMLComment( context, leadingCML, consumeAllAsLeading,
                                      leadingLastLine, firstStatementLine,
                                      statementAsParent, statement,
                                      flowAsParent, flow );
                leadingCML = NULL;
            }

            Fragment *      part( createCommentFragment( comment ) );
            if ( leadingLastLine + 1 == firstStatementLine ||
                 consumeAllAsLeading )
                part->parent = statementAsParent;
            else
                part->parent = flowAsParent;

            if ( leading == NULL )
            {
                leading = new Comment;
                leading->updateBegin( part );
            }
            leading->parts.append( Py::asObject( part ) );
            leading->updateEnd( part );
        }

        context->comments->pop_front();
    }

    if ( leadingCML != NULL )
    {
        addLeadingCMLComment( context, leadingCML, consumeAllAsLeading,
                              leadingLastLine, firstStatementLine,
                              statementAsParent, statement,
                              flowAsParent, flow );
        leadingCML = NULL;
    }
    if ( leading != NULL )
    {
        if ( leadingLastLine + 1 == firstStatementLine ||
             consumeAllAsLeading )
        {
            statementAsParent->updateBeginEnd( leading );
            statement->leadingComment = Py::asObject( leading );
        }
        else
        {
            flowAsParent->updateBeginEnd( leading );
            flow.append( Py::asObject( leading ) );
        }
        leading = NULL;
    }
}



static void
injectLeadingComments( Context *  context,
                       Py::List &  flow,
                       FragmentBase *  flowAsParent,
                       FragmentWithComments *  statement, // could be NULL
                       FragmentBase *  statementAsParent, // could be NULL
                       int  firstStatementLine,
                       bool  consumeAllAsLeading )
{
    int     leadingLastLine = detectLeadingBlock( context,
                                                  firstStatementLine );

    while ( leadingLastLine != -1 )
    {
        injectOneLeadingComment( context, flow, flowAsParent,
                                 statement, statementAsParent,
                                 firstStatementLine, consumeAllAsLeading,
                                 leadingLastLine );
        leadingLastLine = detectLeadingBlock( context, firstStatementLine );
    }
    return;
}


static void
addSideCMLCommentContinue( Context *  context,
                           CMLComment *  sideCML,
                           CommentLine &  comment,
                           FragmentBase *  statementAsParent )
{
    if ( sideCML == NULL )
    {
        // Bad thing: someone may deleted the proper
        // cml comment beginning so the comment is converted into a
        // regular one. The regular comment will be handled below.
        context->flow->addWarning( comment.line, -1,
                    "Continue of the CML comment without the "
                    "beginning. Treat it as a regular comment." );
        comment.type = REGULAR_COMMENT;
        return;
    }

    // Check if there is the proper beginning
    if ( sideCML->endLine + 1 != comment.line )
    {
        // Bad thing: whether someone deleted the beginning of
        // the cml comment or inserted an empty line between.
        // So convert the comment into a regular one.
        context->flow->addWarning( comment.line, -1,
                    "Continue of the CML comment without the beginning "
                    "in the previous line. Treat it as a regular comment." );
        comment.type = REGULAR_COMMENT;
        return;
    }

    // All is fine, let's add the CML continue
    Fragment *      part( createCommentFragment( comment ) );
    part->parent = statementAsParent;
    sideCML->updateEnd( part );
    sideCML->parts.append( Py::asObject( part ) );
    return;
}


static void
addSideCMLComment( Context *  context,
                   CMLComment *  sideCML,
                   FragmentBase *  statementAsParent,
                   FragmentWithComments *  statement,
                   FragmentBase *  flowAsParent )
{
    sideCML->extractProperties( context );
    statement->sideCMLComments.append( Py::asObject( sideCML ) );
    statementAsParent->updateEnd( sideCML );
    flowAsParent->updateEnd( sideCML );
    return;
}


static void
injectSideComments( Context *  context,
                    FragmentWithComments *  statement,
                    FragmentBase *  statementAsParent,
                    FragmentBase *  flowAsParent )
{
    CMLComment *        sideCML = NULL;
    Comment *           side = NULL;
    int                 lastCommentLine = -1;
    int                 lastCommentPos = -1;

    while ( ! context->comments->empty() )
    {
        CommentLine &       comment = context->comments->front();
        if ( comment.line > statementAsParent->endLine )
            break;

        lastCommentLine = comment.line;
        lastCommentPos = comment.pos;

        if ( comment.type == CML_COMMENT )
        {
            if ( sideCML != NULL )
            {
                addSideCMLComment( context, sideCML, statementAsParent,
                                   statement, flowAsParent );
                sideCML = NULL;
            }

            Fragment *      part( createCommentFragment( comment ) );
            part->parent = statementAsParent;

            sideCML = new CMLComment;
            sideCML->parent = part->parent;
            sideCML->updateBeginEnd( part );
            sideCML->parts.append( Py::asObject( part ) );
        }

        if ( comment.type == CML_COMMENT_CONTINUE )
        {
            // It may change the comment type to a REGULAR_COMMENT one
            addSideCMLCommentContinue( context, sideCML, comment,
                                       statementAsParent );
        }

        if ( comment.type == REGULAR_COMMENT )
        {
            Fragment *      part( createCommentFragment( comment ) );
            part->parent = statementAsParent;

            if ( side == NULL )
            {
                side = new Comment;
                side->updateBegin( part );
            }
            side->parts.append( Py::asObject( part ) );
            side->updateEnd( part );
        }

        context->comments->pop_front();
    }

    // Collect trailing comments which could be a continuation of the last side
    // comment
    while ( ! context->comments->empty() )
    {
        CommentLine &       comment = context->comments->front();
        if ( comment.line != lastCommentLine + 1 )
            break;
        if ( comment.pos != lastCommentPos )
            break;

        // It could be that the next line comment starts at the same column
        // however there is some other statement at that line. So we need to
        // stop if so.
        const char *  lineBegin( context->buffer +
                                 context->lineShifts[ comment.line ] );
        const char *  commentBegin( context->buffer + comment.begin );
        bool          nextStatement( false );
        while ( lineBegin != commentBegin )
        {
            if ( *lineBegin != ' ' && *lineBegin != '\t' )
            {
                nextStatement = true;
                break;
            }
            ++lineBegin;
        }
        if ( nextStatement )
            break;

        lastCommentLine = comment.line;

        if ( comment.type == CML_COMMENT )
        {
            if ( sideCML != NULL )
            {
                addSideCMLComment( context, sideCML, statementAsParent,
                                   statement, flowAsParent );
                sideCML = NULL;
            }

            Fragment *      part( createCommentFragment( comment ) );
            part->parent = statementAsParent;

            sideCML = new CMLComment;
            sideCML->parent = part->parent;
            sideCML->updateBeginEnd( part );
            sideCML->parts.append( Py::asObject( part ) );
        }

        if ( comment.type == CML_COMMENT_CONTINUE )
        {
            // It may change the comment type to a REGULAR_COMMENT one
            addSideCMLCommentContinue( context, sideCML, comment,
                                       statementAsParent );
        }

        if ( comment.type == REGULAR_COMMENT )
        {
            Fragment *      part( createCommentFragment( comment ) );
            part->parent = statementAsParent;

            if ( side == NULL )
            {
                side = new Comment;
                side->updateBegin( part );
            }
            side->parts.append( Py::asObject( part ) );
            side->updateEnd( part );
        }

        context->comments->pop_front();
    }


    // Insert the collected comments
    if ( sideCML != NULL )
    {
        addSideCMLComment( context, sideCML, statementAsParent,
                           statement, flowAsParent );
        sideCML = NULL;
    }
    if ( side != NULL )
    {
        statement->sideComment = Py::asObject( side );
        statementAsParent->updateEnd( side );
        flowAsParent->updateEnd( side );
        side = NULL;
    }
    return;
}


// Injects comments to the control flow or to the statement
// The injected comments are deleted from the deque
static void
injectComments( Context *  context,
                Py::List &  flow,
                FragmentBase *  flowAsParent,
                FragmentWithComments *  statement,
                FragmentBase *  statementAsParent,
                bool  consumeAllAsLeading = false )
{
    injectLeadingComments( context, flow, flowAsParent, statement,
                           statementAsParent,
                           statementAsParent->beginLine,
                           consumeAllAsLeading );
    injectSideComments( context, statement, statementAsParent,
                        flowAsParent );
    return;
}


static FragmentBase *
processBreak( Context *  context,
              node *  tree, FragmentBase *  parent,
              Py::List &  flow )
{
    assert( tree->n_type == break_stmt );
    Break *         br( new Break );
    br->parent = parent;

    Fragment *      body( new Fragment );
    body->parent = br;
    updateBegin( body, tree, context );
    body->end = body->begin + 4;        // 4 = strlen( "break" ) - 1
    body->endLine = tree->n_lineno;
    body->endPos = body->beginPos + 4;  // 4 = strlen( "break" ) - 1

    br->updateBeginEnd( body );
    br->body = Py::asObject( body );
    injectComments( context, flow, parent, br, br );
    flow.append( Py::asObject( br ) );
    return br;
}


static FragmentBase *
processContinue( Context *  context,
                 node *  tree, FragmentBase *  parent,
                 Py::List &  flow )
{
    assert( tree->n_type == continue_stmt );
    Continue *      cont( new Continue );
    cont->parent = parent;

    Fragment *      body( new Fragment );
    body->parent = cont;
    updateBegin( body, tree, context );
    body->end = body->begin + 7;        // 7 = strlen( "continue" ) - 1
    body->endLine = tree->n_lineno;
    body->endPos = body->beginPos + 7;  // 7 = strlen( "continue" ) - 1

    cont->updateBeginEnd( body );
    cont->body = Py::asObject( body );
    injectComments( context, flow, parent, cont, cont );
    flow.append( Py::asObject( cont ) );
    return cont;
}


static FragmentBase *
processAssert( Context *  context,
               node *  tree, FragmentBase *  parent,
               Py::List &  flow )
{
    assert( tree->n_type == assert_stmt );
    Assert *            a( new Assert );
    a->parent = parent;

    Fragment *      body( new Fragment );
    body->parent = a;
    updateBegin( body, tree, context );
    body->end = body->begin + 5;        // 5 = strlen( "assert" ) - 1
    body->endLine = tree->n_lineno;
    body->endPos = body->beginPos + 5;  // 5 = strlen( "assert" ) - 1

    a->updateBegin( body );

    // One test node must be there. The second one may not be there
    node *      firstTestNode = findChildOfType( tree, test );
    assert( firstTestNode != NULL );

    Fragment *      tst( new Fragment );
    node *          testLastPart = findLastPart( firstTestNode );

    tst->parent = a;
    updateBegin( tst, firstTestNode, context );
    updateEnd( tst, testLastPart, context );

    a->tst = Py::asObject( tst );

    // If a comma is there => there is a message part
    node *      commaNode = findChildOfType( tree, COMMA );
    if ( commaNode != NULL )
    {
        Fragment *      message( new Fragment );

        // Message test node must follow the comma node
        node *          secondTestNode = commaNode + 1;
        node *          secondTestLastPart = findLastPart( secondTestNode );

        message->parent = a;
        updateBegin( message, secondTestNode, context );
        updateEnd( message, secondTestLastPart, context );

        a->updateEnd( message );
        a->message = Py::asObject( message );
    }
    else
        a->updateEnd( tst );

    a->body = Py::asObject( body );
    injectComments( context, flow, parent, a, a );
    flow.append( Py::asObject( a ) );
    return a;
}



static FragmentBase *
processRaise( Context *  context,
              node *  tree, FragmentBase *  parent,
              Py::List &  flow )
{
    assert( tree->n_type == raise_stmt );
    Raise *         r( new Raise );
    r->parent = parent;

    Fragment *      body( new Fragment );
    body->parent = r;
    updateBegin( body, tree, context );
    body->end = body->begin + 4;        // 4 = strlen( "raise" ) - 1
    body->endLine = tree->n_lineno;
    body->endPos = body->beginPos + 4;  // 4 = strlen( "raise" ) - 1

    r->updateBegin( body );

    node *      testNode = findChildOfType( tree, test );
    if ( testNode != NULL )
    {
        Fragment *      val( new Fragment );
        node *          lastPart = findLastPart( testNode );

        val->parent = r;
        updateBegin( val, testNode, context );
        updateEnd( val, lastPart, context );

        r->updateEnd( val );
        r->value = Py::asObject( val );
    }
    else
        r->updateEnd( body );

    r->body = Py::asObject( body );
    injectComments( context, flow, parent, r, r );
    flow.append( Py::asObject( r ) );
    return r;
}


static FragmentBase *
processReturn( Context *  context, node *  tree,
               FragmentBase *  parent, Py::List &  flow )
{
    assert( tree->n_type == return_stmt );
    Return *        ret( new Return );
    ret->parent = parent;

    Fragment *      body( new Fragment );
    body->parent = ret;
    updateBegin( body, tree, context );
    body->end = body->begin + 5;        // 5 = strlen( "return" ) - 1
    body->endLine = tree->n_lineno;
    body->endPos = body->beginPos + 5;  // 5 = strlen( "return" ) - 1

    ret->updateBegin( body );

    #if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 8
        node *  testlistNode = findChildOfType( tree, testlist_star_expr );
    #else
        node *  testlistNode = findChildOfType( tree, testlist );
    #endif
    if ( testlistNode != NULL )
    {
        Fragment *      val( new Fragment );
        node *          lastPart = findLastPart( testlistNode );

        val->parent = ret;
        updateBegin( val, testlistNode, context );
        updateEnd( val, lastPart, context );

        ret->updateEnd( val );
        ret->value = Py::asObject( val );
    }
    else
        ret->updateEnd( body );

    ret->body = Py::asObject( body );
    injectComments( context, flow, parent, ret, ret );
    flow.append( Py::asObject( ret ) );
    return ret;
}


// Handles 'else' and 'elif' clauses for various statements: 'if' branches,
// 'else' parts of 'while', 'for', 'try'
static ElifPart *
processElifPart( Context *  context, Py::List &  flow,
                 node *  tree, FragmentBase *  parent )
{
    assert( tree->n_type == NAME );

    bool            isIf( strcmp( tree->n_str, "if" ) == 0 );

    assert( strcmp( tree->n_str, "else" ) == 0 ||
            strcmp( tree->n_str, "elif" ) == 0 ||
            isIf );

    ElifPart *      elifPart( new ElifPart );
    elifPart->parent = parent;

    node *      current = tree + 1;
    node *      colonNode = NULL;
    #if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 8
        if ( current->n_type == namedexpr_test )
    #else
        if ( current->n_type == test )
    #endif
    {
        // This is an elif part, i.e. there is a condition part
        node *      last = findLastPart( current );
        Fragment *  condition( new Fragment );
        condition->parent = elifPart;
        updateBegin( condition, current, context );
        updateEnd( condition, last, context );

        elifPart->condition = Py::asObject( condition );

        colonNode = current + 1;
    }
    else
    {
        assert( current->n_type == COLON );
        colonNode = current;
    }

    node *          suiteNode = colonNode + 1;
    Fragment *      body( new Fragment );
    body->parent = elifPart;
    updateBegin( body, tree, context );
    updateEnd( body, colonNode, context );
    elifPart->updateBeginEnd( body );
    elifPart->body = Py::asObject( body );

    // If it is not an 'if' statement, then all the comments should be consumed
    // as leading
    injectComments( context, flow, parent, elifPart, elifPart, ! isIf );
    FragmentBase *  lastAdded = walk( context, suiteNode, elifPart,
                                      elifPart->nsuite, false );
    if ( lastAdded == NULL )
        elifPart->updateEnd( body );
    else
        elifPart->updateEnd( lastAdded );
    return elifPart;
}


static FragmentBase *
processIf( Context *  context,
           node *  tree, FragmentBase *  parent,
           Py::List &  flow )
{
    assert( tree->n_type == if_stmt );

    If *        ifStatement( new If );
    ifStatement->parent = parent;

    for ( int k = 0; k < tree->n_nchildren; ++k )
    {
        node *  child = &(tree->n_child[ k ]);
        if ( child->n_type == NAME )
        {
            ElifPart *  elifPart = processElifPart( context, flow, child,
                                                    ifStatement );
            ifStatement->updateBegin( elifPart );
            ifStatement->parts.append( Py::asObject( elifPart ) );
        }
    }

    flow.append( Py::asObject( ifStatement ) );
    return ifStatement;
}


static ExceptPart *
processExceptPart( Context *  context, Py::List &  flow,
                   node *  tree, FragmentBase *  parent )
{
    assert( tree->n_type == except_clause ||
            tree->n_type == NAME );

    ExceptPart *    exceptPart( new ExceptPart );
    exceptPart->parent = parent;

    Fragment *      body( new Fragment );
    body->parent = exceptPart;

    // ':' node is the very next one
    node *          colonNode = tree + 1;
    updateBegin( body, tree, context );
    updateEnd( body, colonNode, context );
    exceptPart->updateBeginEnd( body );
    exceptPart->body = Py::asObject( body );

    // If it is NAME => it is 'finally' or 'else'
    // The clause could only be in the 'except' case
    if ( tree->n_type == except_clause )
    {
        node *      testNode = findChildOfType( tree, test );
        if ( testNode != NULL )
        {
            node *      last = findLastPart( tree );
            Fragment *  clause( new Fragment );

            clause->parent = exceptPart;
            updateBegin( clause, testNode, context );
            updateEnd( clause, last, context );
            exceptPart->clause = Py::asObject( clause );
        }
    }

    injectComments( context, flow, parent,
                    exceptPart, exceptPart, true );

    // 'suite' node follows the colon node
    node *          suiteNode = colonNode + 1;
    FragmentBase *  lastAdded = walk( context,
                                      suiteNode, exceptPart,
                                      exceptPart->nsuite, false );
    if ( lastAdded == NULL )
        exceptPart->updateEnd( body );
    else
        exceptPart->updateEnd( lastAdded );
    return exceptPart;
}


static FragmentBase *
processTry( Context *  context,
            node *  tree, FragmentBase *  parent,
            Py::List &  flow )
{
    assert( tree->n_type == try_stmt );

    Try *       tryStatement( new Try );
    tryStatement->parent = parent;

    Fragment *      body( new Fragment );
    node *          tryColonNode = findChildOfType( tree, COLON );
    body->parent = tryStatement;
    updateBegin( body, tree, context );
    updateEnd( body, tryColonNode, context );
    tryStatement->body = Py::asObject( body );
    tryStatement->updateBeginEnd( body );

    injectComments( context, flow, parent,
                    tryStatement, tryStatement );

    // suite
    node *          trySuiteNode = tryColonNode + 1;
    FragmentBase *  lastAdded = walk( context,
                                      trySuiteNode, tryStatement,
                                      tryStatement->nsuite, false );
    if ( lastAdded == NULL )
        tryStatement->updateEnd( body );
    else
        tryStatement->updateEnd( lastAdded );


    // except, finally, else parts
    for ( int k = 0; k < tree->n_nchildren; ++k )
    {
        node *  child = &(tree->n_child[ k ]);
        if ( child->n_type == except_clause )
        {
            ExceptPart *    exceptPart = processExceptPart( context, flow,
                                                            child,
                                                            tryStatement );
            tryStatement->exceptParts.append( Py::asObject( exceptPart ) );
            continue;
        }
        if ( child->n_type == NAME )
        {
            if ( strcmp( child->n_str, "else" ) == 0 )
            {
                // I am not too sure object of what type is better to see here.
                // The options are: ElifPart or ExceptPart
                // Elif part is better because it is unified among all 'else's,
                // in 'for', 'while' and 'try'.
                // ExceptPart is better because it is more specific for 'try'
                // For the time being Elif part is chosen. To switch to
                // ExceptPart use:
                // ExceptPart * elsePart = processExceptPart(...) with the same
                // arguments.
                ElifPart *      elsePart = processElifPart( context, flow,
                                                            child,
                                                            tryStatement );
                tryStatement->elsePart = Py::asObject( elsePart );
                continue;
            }
            if ( strcmp( child->n_str, "finally" ) == 0 )
            {
                ExceptPart *    finallyPart = processExceptPart( context, flow,
                                                                 child,
                                                                 tryStatement );
                tryStatement->finallyPart = Py::asObject( finallyPart );
            }
        }
    }

    flow.append( Py::asObject( tryStatement ) );
    return tryStatement;
}


static FragmentBase *
processWhile( Context *  context,
              node *  tree, FragmentBase *  parent,
              Py::List &  flow )
{
    assert( tree->n_type == while_stmt );

    While *     w( new While);
    w->parent = parent;

    Fragment *      body( new Fragment );
    node *          colonNode = findChildOfType( tree, COLON );
    node *          whileNode = findChildOfType( tree, NAME );

    body->parent = w;
    updateBegin( body, whileNode, context );
    updateEnd( body, colonNode, context );
    w->body = Py::asObject( body );
    w->updateBeginEnd( body );

    // condition
    #if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 8
        node *          testNode = findChildOfType( tree, namedexpr_test );
    #else
        node *          testNode = findChildOfType( tree, test );
    #endif
    node *          lastPart = findLastPart( testNode );
    Fragment *      condition( new Fragment );

    condition->parent = w;
    updateBegin( condition, testNode, context );
    updateEnd( condition, lastPart, context );
    w->condition = Py::asObject( condition );

    injectComments( context, flow, parent, w, w );

    // suite
    node *          suiteNode = findChildOfType( tree, suite );
    FragmentBase *  lastAdded = walk( context, suiteNode, w, w->nsuite,
                                      false );
    if ( lastAdded == NULL )
        w->updateEnd( body );
    else
        w->updateEnd( lastAdded );

    // else part
    node *          elseNode = findChildOfTypeAndValue( tree, NAME, "else" );
    if ( elseNode != NULL )
    {
        ElifPart *      elsePart = processElifPart( context, flow, elseNode, w );
        w->elsePart = Py::asObject( elsePart );
    }

    flow.append( Py::asObject( w ) );
    return w;
}


static FragmentBase *
processWith( Context *  context,
             node *  tree, FragmentBase *  parent,
             Py::List &  flow )
{
    assert( tree->n_type == with_stmt || tree->async_stmt );

    node *      asyncNode = NULL;
    if ( tree->n_type != with_stmt )
    {
        asyncNode = & ( tree->n_child[ 0 ] );
        tree = & ( tree->n_child[ 1 ] );
    }
    assert( tree->n_type == with_stmt );


    With *      w( new With );
    w->parent = parent;

    Fragment *      body( new Fragment );
    node *          colonNode = findChildOfType( tree, COLON );
    node *          whithNode = findChildOfType( tree, NAME );

    body->parent = w;

    if ( asyncNode != NULL )
    {
        Fragment *      async( new Fragment );
        async->parent = w;
        updateBegin( async, asyncNode, context );
        updateEnd( async, asyncNode, context );
        w->asyncKeyword = Py::asObject( async );

        // Need to update the body begin too
        updateBegin( body, asyncNode, context );
    }
    else
    {
        updateBegin( body, whithNode, context );
    }

    updateEnd( body, colonNode, context );
    w->body = Py::asObject( body );
    w->updateBeginEnd( body );

    // with keyword
    Fragment *      withKeyword( new Fragment );
    withKeyword->parent = w;
    updateBegin( withKeyword, whithNode, context );
    updateEnd( withKeyword, whithNode, context );
    w->withKeyword = Py::asObject( withKeyword );

    // items
    node *      firstWithItem = findChildOfType( tree, with_item );
    node *      lastWithItem = NULL;
    for ( int  k = 0; k < tree->n_nchildren; ++k )
    {
        node *  child = &(tree->n_child[ k ]);
        if ( child->n_type == with_item )
            lastWithItem = child;
    }

    Fragment *      items( new Fragment );
    node *          lastPart = findLastPart(lastWithItem);
    items->parent = w;
    updateBegin( items, firstWithItem, context );
    updateEnd( items, lastPart, context );
    w->items = Py::asObject( items );

    injectComments( context, flow, parent, w, w );

    // suite
    node *          suiteNode = findChildOfType( tree, suite );
    FragmentBase *  lastAdded = walk( context, suiteNode, w, w->nsuite,
                                      false );
    if ( lastAdded == NULL )
        w->updateEnd( body );
    else
        w->updateEnd( lastAdded );

    flow.append( Py::asObject( w ) );
    return w;
}


static FragmentBase *
processFor( Context *  context,
            node *  tree, FragmentBase *  parent,
            Py::List &  flow )
{
    assert( tree->n_type == for_stmt || tree->async_stmt );

    node *      asyncNode = NULL;
    if ( tree->n_type != for_stmt )
    {
        asyncNode = & ( tree->n_child[ 0 ] );
        tree = & ( tree->n_child[ 1 ] );
    }
    assert( tree->n_type == for_stmt );


    For *       f( new For );
    f->parent = parent;

    Fragment *      body( new Fragment );
    node *          colonNode = findChildOfType( tree, COLON );
    node *          forNode = findChildOfType( tree, NAME );

    body->parent = f;

    if ( asyncNode != NULL )
    {
        Fragment *      async( new Fragment );
        async->parent = f;
        updateBegin( async, asyncNode, context );
        updateEnd( async, asyncNode, context );
        f->asyncKeyword = Py::asObject( async );

        // Need to update the body begin too
        updateBegin( body, asyncNode, context );
    }
    else
    {
        updateBegin( body, forNode, context );
    }

    updateEnd( body, colonNode, context );
    f->body = Py::asObject( body );
    f->updateBeginEnd( body );

    // for keyword
    Fragment *      forKeyword( new Fragment );
    forKeyword->parent = f;
    updateBegin( forKeyword, forNode, context );
    updateEnd( forKeyword, forNode, context );
    f->forKeyword = Py::asObject( forKeyword );

    // Iteration
    node *          exprlistNode = findChildOfType( tree, exprlist );
    node *          testlistNode = findChildOfType( tree, testlist );
    node *          lastPart = findLastPart( testlistNode );
    Fragment *      iteration( new Fragment );

    iteration->parent = f;
    updateBegin( iteration, exprlistNode, context );
    updateEnd( iteration, lastPart, context );
    f->iteration = Py::asObject( iteration );

    injectComments( context, flow, parent, f, f );

    // suite
    node *          suiteNode = findChildOfType( tree, suite );
    FragmentBase *  lastAdded = walk( context, suiteNode, f, f->nsuite,
                                      false );
    if ( lastAdded == NULL )
        f->updateEnd( body );
    else
        f->updateEnd( lastAdded );

    // else part
    node *          elseNode = findChildOfTypeAndValue( tree, NAME, "else" );
    if ( elseNode != NULL )
    {
        ElifPart *      elsePart = processElifPart( context, flow, elseNode, f );
        f->elsePart = Py::asObject( elsePart );
    }

    flow.append( Py::asObject( f ) );
    return f;
}


static FragmentBase *
processImport( Context *  context,
               node *  tree, FragmentBase *  parent,
               Py::List &  flow )
{
    assert( tree->n_type == import_stmt );
    assert( tree->n_nchildren == 1 );


    Import *        import( new Import );
    import->parent = parent;

    Fragment *      body( new Fragment );
    node *          lastPart = findLastPart( tree );

    body->parent = import;
    updateBegin( body, tree, context );
    updateEnd( body, lastPart, context );

    /* There must be one child of type import_from or import_name */
    tree = & ( tree->n_child[ 0 ] );
    if ( tree->n_type == import_from )
    {
        Fragment *  fromFragment( new Fragment );
        Fragment *  whatFragment( new Fragment );

        node *      fromPartBegin = findChildOfType( tree, ELLIPSIS );
        if ( fromPartBegin == NULL )
        {
            fromPartBegin = findChildOfType( tree, DOT );
            if ( fromPartBegin == NULL )
                fromPartBegin = findChildOfType( tree, dotted_name );
        }
        assert( fromPartBegin != NULL );

        fromFragment->parent = import;
        whatFragment->parent = import;

        updateBegin( fromFragment, fromPartBegin, context );

        node *      lastFromPart = NULL;
        if ( fromPartBegin->n_type == DOT ||
             fromPartBegin->n_type == ELLIPSIS )
        {
            // it could be:
            // DOT ... DOT or
            // DOT ... DOT dotted_name
            lastFromPart = findChildOfType( tree, dotted_name );
            if ( lastFromPart == NULL )
            {
                // This is DOT ... DOT
                lastFromPart = fromPartBegin;
                while ( (lastFromPart+1)->n_type == DOT ||
                        (lastFromPart+1)->n_type == ELLIPSIS )
                    ++lastFromPart;
            }
            else
            {
                lastFromPart = findLastPart( lastFromPart );
            }
        }
        else
        {
            lastFromPart = findLastPart( fromPartBegin );
        }

        updateEnd( fromFragment, lastFromPart, context );

        node *      whatPart = findChildOfTypeAndValue( tree, NAME, "import" );
        assert( whatPart != NULL );

        ++whatPart;     // the very next after import is the first of the what part
        updateBegin( whatFragment, whatPart, context );
        updateEnd( whatFragment, lastPart, context );

        import->fromPart = Py::asObject( fromFragment );
        import->whatPart = Py::asObject( whatFragment );

        // Check if there is exit imported from sys
        if ( fromPartBegin->n_type == dotted_name )
        {
            if ( fromPartBegin->n_nchildren == 1 )
            {
                node *  fromNode = &(fromPartBegin->n_child[ 0 ]);
                if ( strcmp( fromNode->n_str, "sys" ) == 0 )
                {
                    node *  importAsNames = findChildOfType( tree, import_as_names );
                    if ( importAsNames != NULL )
                    {
                        for ( int  k = 0; k < importAsNames->n_nchildren; ++k )
                        {
                            node *  child = &(importAsNames->n_child[ k ]);
                            if ( child->n_type == import_as_name )
                            {
                                node *  nameNode = &(child->n_child[ 0 ]);
                                if ( strcmp( nameNode->n_str, "exit" ) == 0 )
                                {
                                    if ( child->n_nchildren == 1 )
                                    {
                                        context->sysExit.insert( "exit" );
                                    }
                                    else if ( child->n_nchildren == 3 )
                                    {
                                        node *  asChild = &(child->n_child[ 2 ]);
                                        context->sysExit.insert( asChild->n_str );
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        // It could be * imported
                        node *  starImported = findChildOfType( tree, STAR );
                        if ( starImported != NULL )
                            context->sysExit.insert( "exit" );
                    }
                }
            }
        }
    }
    else
    {
        assert( tree->n_type == import_name );
        import->fromPart = Py::None();

        Fragment *      whatFragment( new Fragment );
        node *          firstWhat = findChildOfType( tree, dotted_as_names );
        assert( firstWhat != NULL );

        whatFragment->parent = import;
        updateBegin( whatFragment, firstWhat, context );

        // The end matches the body
        whatFragment->end = body->end;
        whatFragment->endLine = body->endLine;
        whatFragment->endPos = body->endPos;

        import->whatPart = Py::asObject( whatFragment );

        // Check if there are imports of sys
        for ( int  k = 0; k < firstWhat->n_nchildren; ++k )
        {
            node *  child = &(firstWhat->n_child[ k ]);
            if ( child->n_type == dotted_as_name )
            {
                node *  nameNode = &(child->n_child[ 0 ]);
                nameNode = &(nameNode->n_child[ 0 ]);
                if ( nameNode->n_type == NAME && strcmp( nameNode->n_str, "sys" ) == 0 )
                {
                    if ( child->n_nchildren == 3 )
                    {
                        node *  asNameNode = &(child->n_child[ 2 ]);
                        context->sysExit.insert( asNameNode->n_str + std::string( ".exit" ) );
                    }
                    else
                    {
                        context->sysExit.insert( "sys.exit" );
                    }
                }
            }
        }
    }

    import->updateBeginEnd( body );
    import->body = Py::asObject( body );
    injectComments( context, flow, parent, import, import );
    flow.append( Py::asObject( import ) );
    return import;
}

static void
processDecor( Context *  context, Py::List &  flow,
              FragmentBase *  parent,
              node *  tree, std::list<Decorator *> &  decors )
{
    assert( tree->n_type == decorator );

    node *      atNode = findChildOfType( tree, AT );
    node *      nameNode = findChildOfType( tree, dotted_name );
    node *      lparNode = findChildOfType( tree, LPAR );
    assert( atNode != NULL );
    assert( nameNode != NULL );

    Decorator *     decor( new Decorator );
    Fragment *      nameFragment( new Fragment );
    node *          lastNameNode = findLastPart( nameNode );

    nameFragment->parent = decor;
    updateBegin( nameFragment, nameNode, context );
    updateEnd( nameFragment, lastNameNode, context );
    decor->name = Py::asObject( nameFragment );

    Fragment *      body( new Fragment );
    body->parent = decor;
    updateBegin( body, atNode, context );

    if ( lparNode == NULL )
    {
        // Decorator without arguments
        updateEnd( body, lastNameNode, context );
    }
    else
    {
        // Decorator with arguments
        node *          rparNode = findChildOfType( tree, RPAR );
        Fragment *      argsFragment( new Fragment );

        argsFragment->parent = decor;
        updateBegin( argsFragment, lparNode, context );
        updateEnd( argsFragment, rparNode, context );
        decor->arguments = Py::asObject( argsFragment );
        updateEnd( body, rparNode, context );
    }

    decor->body = Py::asObject( body );
    decor->updateBeginEnd( body );

    // If it is not the first decorator then all the leading comments should be
    // consumed.
    injectComments( context, flow, parent, decor, decor, ! decors.empty() );
    decors.push_back( decor );
    return;
}


static std::list<Decorator *>
processDecorators( Context *  context, Py::List &  flow,
                   FragmentBase *  parent, node *  tree )
{
    assert( tree->n_type == decorators );

    int                         n = tree->n_nchildren;
    node *                      child;
    std::list<Decorator *>      decors;

    for ( int  k = 0; k < n; ++k )
    {
        child = & ( tree->n_child[ k ] );
        if ( child->n_type == decorator )
        {
            processDecor( context, flow, parent, child, decors );
        }
    }
    return decors;
}


// None or a SysExit instance
static FragmentBase *
checkForSysExit( Context *          context,
                 node *             tree,
                 Py::List &         flow,
                 FragmentBase *     parent )
{
    if ( tree == NULL )
        return NULL;
    if ( tree->n_type != small_stmt )
        return NULL;

    // Note: the python grammar has been changed between 3.4 and 3.5
    // 3.4 and lower had the 'power' node preceeding the 'atom' node.
    // The newer versions have a new 'atom_expr' node. So there is a
    // define here; the variable name is kept as 'powerNode' though
    // in python > 3.5 the name atomExprNode would fit better.
    #if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 5
        node *      powerNode( skipToNode( tree, atom_expr ) );
    #else
        node *      powerNode( skipToNode( tree, power ) );
    #endif
    if ( powerNode == NULL )
        return NULL;

    // The 'power' must have:
    // - the first child 'atom'
    // - the 'atom' must have 1 child of type NAME
    // - followed by one or more 'trailer'
    // - the last 'trailer' must have the the first child LPAR

    // There could be only one '.' so the number of trailers is 1 or 2
    // the first child is 'atom' and then trailers
    if ( powerNode->n_nchildren < 2 || powerNode->n_nchildren > 3 )
        return NULL;

    node *      atomNode = & ( powerNode->n_child[ 0 ] );
    if ( atomNode->n_type != atom )
        return NULL;
    if ( atomNode->n_nchildren != 1 )
        return NULL;
    if ( atomNode->n_child[ 0 ].n_type != NAME )
        return NULL;

    node *      lastTrailer = & ( powerNode->n_child[ powerNode->n_nchildren - 1 ] );
    if ( lastTrailer->n_type != trailer )
        return NULL;
    if ( lastTrailer->n_nchildren < 2 )
        return NULL;
    if ( lastTrailer->n_child[ 0 ].n_type != LPAR )
        return NULL;

    // Now collect the string as the statement may look like
    std::string     statement( atomNode->n_child[ 0 ].n_str );
    if ( powerNode->n_nchildren == 3 )
    {
        node *      trailerNode = & ( powerNode->n_child[ 1 ] );
        if ( trailerNode->n_type != trailer )
            return NULL;
        if ( trailerNode->n_nchildren != 2 )
            return NULL;
        if ( trailerNode->n_child[ 0 ].n_type != DOT )
            return NULL;
        if ( trailerNode->n_child[ 1 ].n_type != NAME )
            return NULL;
        statement += "." + std::string( trailerNode->n_child[ 1 ].n_str );
    }

    // Check if the pattern is in the sys.exit patterns
    if ( context->sysExit.find( statement ) != context->sysExit.end() )
    {
        node *      lparNode = findChildOfType( lastTrailer, LPAR );
        node *      rparNode = findChildOfType( lastTrailer, RPAR );
        node *      arglistNode = findChildOfType( lastTrailer, arglist );

        SysExit *       sysExit( new SysExit );
        sysExit->parent = parent;

        Fragment *      body( new Fragment );
        body->parent = sysExit;
        updateBegin( body, atomNode, context );
        updateEnd( body, rparNode, context );
        sysExit->body = Py::asObject( body );

        if ( arglistNode != NULL )
        {
            node *      lastPartNode = findLastPart( arglistNode );
            Fragment *  actualArg( new Fragment );

            actualArg->parent = parent;
            updateBegin( actualArg, arglistNode, context );
            updateEnd( actualArg, lastPartNode, context );
            sysExit->actualArg = Py::asObject( actualArg );
        }

        Fragment *  arg( new Fragment );
        arg->parent = sysExit;
        updateBegin( arg, lparNode, context );
        updateEnd( arg, rparNode, context );
        sysExit->arg = Py::asObject( arg );

        sysExit->updateBeginEnd( body );

        // NB: no comments injection!
        // It has to be done after the comments are injected for the currently
        // accumulated code block

        return sysExit;
    }

    // It is not a sys.exit(...) statement
    return NULL;
}


// NULL or a Docstring instance
static Docstring *
checkForDocstring( Context *  context, node *  tree )
{
    context->lastDocstring = NULL;

    if ( tree == NULL )
        return NULL;

    node *      child = NULL;
    int         n = tree->n_nchildren;
    for ( int  k = 0; k < n; ++k )
    {
        /* need to skip NEWLINE and INDENT till stmt if so */
        child = & ( tree->n_child[ k ] );
        if ( child->n_type == NEWLINE )
            continue;
        if ( child->n_type == INDENT )
            continue;
        if ( child->n_type == stmt || child->n_type == simple_stmt )
            break;

        return NULL;
    }

    child = skipToNode( child, atom );
    if ( child == NULL )
        return NULL;

    Docstring *     docstr( new Docstring );
    Fragment *      body( new Fragment );
    body->parent = docstr;

    /* Atom has to have children of the STRING type only */
    node *          stringChild;

    n = child->n_nchildren;
    for ( int  k = 0; k < n; ++k )
    {
        stringChild = & ( child->n_child[ k ] );
        if ( stringChild->n_type != STRING )
        {
            delete docstr;
            delete body;
            return NULL;
        }

        // This is a docstring part
        Fragment *      part( new Fragment );
        part->parent = docstr;

        updateBegin( part, stringChild, context );
        updateEnd( part, stringChild, context );

        // In the vast majority of cases a docstring consists of a single part
        // so there is no need to optimize via updateBegin() & updateEnd()
        docstr->updateBeginEnd( part );
        docstr->parts.append( Py::asObject( part ) );
        body->updateBeginEnd( part );
    }

    docstr->body = Py::asObject( body );
    context->lastDocstring = docstr;
    return docstr;
}


static Annotation *
processAnnotation( Context *    context,
                   node *       separator,
                   node *       annotation )
{
    if ( separator == NULL || annotation == NULL )
        return NULL;

    Annotation *    ann( new Annotation );

    Fragment *      sep( new Fragment );
    sep->parent = ann;
    updateBegin( sep, separator, context );
    updateEnd( sep, separator, context );
    ann->separator = Py::asObject( sep );

    Fragment *      text( new Fragment );
    node *          lastPart( findLastPart( annotation ) );

    text->parent = ann;
    updateBegin( text, annotation, context );
    updateEnd( text, lastPart, context );
    ann->text = Py::asObject( text );

    ann->updateEnd( text );
    ann->updateBegin( sep );
    return ann;
}


static int
processFunctionArgument( Context *      context,
                         Function *     func,
                         node *         arguments,
                         int            index)
{
    // One of the cases here:
    // - regular argument (name [+ annot] [+default])
    // - STAR (* name [+ annot])
    // - DOUBLESTAR (* name [ + annot])
    // - STAR (*)

    node *      tfpdefNode( & arguments->n_child[ index ] );
    node *      argBegin( tfpdefNode );
    node *      nameNode( argBegin );
    if ( tfpdefNode->n_type == STAR )
    {
        // Step further only if there is a following tfpdef node
        if ( index + 1 < arguments->n_nchildren )
        {
            node *  nextNode = & arguments->n_child[ index + 1 ];
            if ( nextNode->n_type == tfpdef )
            {
                ++index;
                tfpdefNode = nextNode;
            }
        }
    }
    else if ( tfpdefNode->n_type == DOUBLESTAR )
    {
        ++index;
        tfpdefNode = & arguments->n_child[ index ];
    }

    if ( tfpdefNode->n_type == tfpdef )
    {
        // The NAME node is always the first child of the tfpdef node
        nameNode = & tfpdefNode->n_child[ 0 ];
    }

    Argument *      arg( new Argument );
    arg->parent = func;

    Fragment *  name( new Fragment );
    name->parent = arg;
    updateBegin( name, argBegin, context );
    updateEnd( name, nameNode, context );
    arg->name = Py::asObject( name );

    arg->updateEnd( name );
    arg->updateBegin( name );

    // See if there is an annotation
    node *      colonNode( findChildOfType( tfpdefNode, COLON ) );
    if ( colonNode != NULL )
    {
        // That's the annotation
        node *      testNode ( findChildOfType( tfpdefNode, test ) );
        if ( testNode != NULL )
        {
            Annotation *        ann = processAnnotation( context,
                                                         colonNode, testNode );
            if ( ann != NULL )
            {
                ann->parent = arg;
                arg->annotation = Py::asObject( ann );
                arg->updateEnd( ann );
            }
        }
    }

    // See for the default value
    ++index;
    if ( index < arguments->n_nchildren )
    {
        node *      child( & arguments->n_child[ index ] );
        if ( child->n_type == EQUAL )
        {
            // The default value is here
            ++index;
            node *      testNode( & arguments->n_child[ index ] );
            if ( testNode->n_type == test )
            {
                Fragment *      sep( new Fragment );
                Fragment *      defValue( new Fragment );
                node *          lastPart( findLastPart( testNode ) );

                sep->parent = arg;
                updateBegin( sep, child, context );
                updateEnd( sep, child, context );
                arg->separator = Py::asObject( sep );

                defValue->parent = arg;
                updateBegin( defValue, testNode, context );
                updateEnd( defValue, lastPart, context );
                arg->defaultValue = Py::asObject( defValue );

                arg->updateEnd( defValue );

                ++index;
            }
        }
    }

    func->argList.append( Py::asObject( arg ) );
    return index;
}


static FragmentBase *
processFuncDefinition( Context *                    context,
                       node *                       tree,
                       FragmentBase *               parent,
                       Py::List &                   flow,
                       std::list<Decorator *> &     decors )
{
    assert( tree->n_type == funcdef || tree->n_type == async_funcdef ||
            tree->n_type == async_stmt );
    assert( tree->n_nchildren > 1 );

    node *      asyncNode = NULL;
    if ( tree->n_type != funcdef )
    {
        asyncNode = & ( tree->n_child[ 0 ] );
        tree = & ( tree->n_child[ 1 ] );
    }
    assert( tree->n_type == funcdef );


    node *      defNode = & ( tree->n_child[ 0 ] );
    node *      nameNode = & ( tree->n_child[ 1 ] );
    node *      colonNode = findChildOfType( tree, COLON );
    node *      annotSeparator = findChildOfType( tree, RARROW );

    assert( colonNode != NULL );

    Function *      func( new Function );
    func->parent = parent;

    Fragment *      body( new Fragment );
    body->parent = func;

    if ( asyncNode != NULL )
    {
        Fragment *      async( new Fragment );
        async->parent = func;
        updateBegin( async, asyncNode, context );
        updateEnd( async, asyncNode, context );
        func->asyncKeyword = Py::asObject( async );

        // Need to update the body begin too
        updateBegin( body, asyncNode, context );
    }
    else
    {
        updateBegin( body, defNode, context );
    }

    updateEnd( body, colonNode, context );
    func->body = Py::asObject( body );

    if ( annotSeparator != NULL )
    {
        node *      annotNode = findChildOfType( tree, test );
        if ( annotNode != NULL )
        {
            Annotation *  ann = processAnnotation( context, annotSeparator,
                                                   annotNode );
            if ( ann != NULL )
            {
                ann->parent = func;
                func->annotation = Py::asObject( ann );
            }
        }
    }

    Fragment *      def( new Fragment );
    def->parent = func;
    updateBegin( def, defNode, context );
    updateEnd( def, defNode, context );
    func->defKeyword = Py::asObject( def );

    Fragment *      name( new Fragment );
    name->parent = func;
    updateBegin( name, nameNode, context );
    updateEnd( name, nameNode, context );
    func->name = Py::asObject( name );

    node *      params = findChildOfType( tree, parameters );
    node *      lparNode = findChildOfType( params, LPAR );
    node *      rparNode = findChildOfType( params, RPAR );
    Fragment *  args( new Fragment );
    args->parent = func;
    updateBegin( args, lparNode, context );
    updateEnd( args, rparNode, context );
    func->arguments = Py::asObject( args );

    node *      argsNode = findChildOfType( params, typedargslist );
    if ( argsNode != NULL )
    {
        /* The function has arguments */
        int         k = 0;
        node *      child;
        while ( k < argsNode->n_nchildren )
        {
            child = & ( argsNode->n_child[ k ] );
            if ( child->n_type == tfpdef ||
                 child->n_type == STAR ||
                 child->n_type == DOUBLESTAR )
            {
                // It adds an argument to the func->argList
                k = processFunctionArgument( context, func, argsNode, k );
            }
            else
                ++k;
        }
    }

    func->updateEnd( body );
    func->updateBegin( body );

    // Comments must be injected before decorators can update the begin of the
    // function. Otherwise leading comments could be injected as side ones
    // If there are decorators then all the leading comments should be consumed
    injectComments( context, flow, parent, func, func, ! decors.empty() );

    if ( ! decors.empty() )
    {
        for ( std::list<Decorator *>::iterator  k = decors.begin();
              k != decors.end(); ++k )
        {
            Decorator *     dec = *k;
            dec->parent = func;
            func->decors.append( Py::asObject( dec ) );
        }
        func->updateBegin( *(decors.begin()) );
        decors.clear();
    }

    // Handle docstring if so
    node *      suiteNode = findChildOfType( tree, suite );
    assert( suiteNode != NULL );

    Docstring *  docstr = checkForDocstring( context, suiteNode );
    if ( docstr != NULL )
    {
        docstr->parent = func;
        injectComments( context, func->nsuite, func, docstr, docstr );
        func->docstring = Py::asObject( docstr );

        // It could be that a docstring is the only item in the function suite
        func->updateEnd( docstr );
    }

    // Walk nested nodes
    FragmentBase *  lastAdded = walk( context, suiteNode, func,
                                      func->nsuite, docstr != NULL );
    if ( lastAdded == NULL )
        func->updateEnd( body );
    else
        func->updateEnd( lastAdded );
    flow.append( Py::asObject( func ) );
    return func;
}


static FragmentBase *
processClassDefinition( Context *                    context,
                        node *                       tree,
                        FragmentBase *               parent,
                        Py::List &                   flow,
                        std::list<Decorator *> &     decors )
{
    assert( tree->n_type == classdef );
    assert( tree->n_nchildren > 1 );

    node *      defNode = & ( tree->n_child[ 0 ] );
    node *      nameNode = & ( tree->n_child[ 1 ] );
    node *      colonNode = findChildOfType( tree, COLON );

    assert( colonNode != NULL );

    Class *      cls( new Class );
    cls->parent = parent;

    Fragment *      body( new Fragment );
    body->parent = cls;
    updateBegin( body, defNode, context );
    updateEnd( body, colonNode, context );
    cls->body = Py::asObject( body );

    Fragment *      name( new Fragment );
    name->parent = cls;
    updateBegin( name, nameNode, context );
    updateEnd( name, nameNode, context );
    cls->name = Py::asObject( name );

    node *      lparNode = findChildOfType( tree, LPAR );
    if ( lparNode != NULL )
    {
        // There is a list of base classes
        node *      rparNode = findChildOfType( tree, RPAR );
        Fragment *  baseClasses( new Fragment );

        baseClasses->parent = cls;
        updateBegin( baseClasses, lparNode, context );
        updateEnd( baseClasses, rparNode, context );
        cls->baseClasses = Py::asObject( baseClasses );
    }

    cls->updateEnd( body );
    cls->updateBegin( body );

    // Comments must be injected before decorators can update the begin of the
    // class. Otherwise leading comments could be injected as side ones
    // If there are decorators then all the leading comments should be consumed
    injectComments( context, flow, parent, cls, cls, ! decors.empty() );

    if ( ! decors.empty() )
    {
        for ( std::list<Decorator *>::iterator  k = decors.begin();
              k != decors.end(); ++k )
        {
            Decorator *     dec = *k;
            dec->parent = cls;
            cls->decors.append( Py::asObject( dec ) );
        }
        cls->updateBegin( *(decors.begin()) );
        decors.clear();
    }

    // Handle docstring if so
    node *      suiteNode = findChildOfType( tree, suite );
    assert( suiteNode != NULL );

    Docstring *  docstr = checkForDocstring( context, suiteNode );
    if ( docstr != NULL )
    {
        docstr->parent = cls;
        injectComments( context, cls->nsuite, cls, docstr, docstr );
        cls->docstring = Py::asObject( docstr );

        // It could be that a docstring is the only item in the class suite
        cls->updateEnd( docstr );
    }

    // Walk nested nodes
    FragmentBase *  lastAdded = walk( context, suiteNode, cls, cls->nsuite,
                                      docstr != NULL );

    if ( lastAdded == NULL )
        cls->updateEnd( body );
    else
        cls->updateEnd( lastAdded );

    flow.append( Py::asObject( cls ) );
    return cls;
}




// Receives small_stmt
// Provides the meaningful node to process or NULL
static node *
getSmallStatementNodeToProcess( node *  tree )
{
    assert( tree->n_type == small_stmt );

    // small_stmt: (expr_stmt | print_stmt  | del_stmt | pass_stmt | flow_stmt
    //              | import_stmt | global_stmt | exec_stmt | assert_stmt)
    // flow_stmt: break_stmt | continue_stmt | return_stmt | raise_stmt |
    //            yield_stmt
    if ( tree->n_nchildren <= 0 )
        return NULL;

    node *      child = & ( tree->n_child[ 0 ] );
    if ( child->n_type == flow_stmt )
    {
        if ( child->n_nchildren <= 0 )
            return NULL;
        // Return first flow_stmt child
        return & ( child->n_child[ 0 ] );
    }
    return child;
}


// Receives stmt
// Provides the meaningful node to process or NULL
static node *
getStmtNodeToProcess( node *  tree )
{
    // stmt: simple_stmt | compound_stmt
    assert( tree->n_type == stmt );

    if ( tree->n_nchildren <= 0 )
        return NULL;

    // simple_stmt: small_stmt (';' small_stmt)* [';'] NEWLINE
    tree = & ( tree->n_child[ 0 ] );
    if ( tree->n_type == simple_stmt )
        return tree;

    // It is a compound statement
    // compound_stmt: if_stmt | while_stmt | for_stmt | try_stmt | with_stmt |
    //                funcdef | classdef | decorated
    // decorated: decorators (classdef | funcdef)
    assert( tree->n_type == compound_stmt );
    if ( tree->n_nchildren <= 0 )
        return NULL;
    return & ( tree->n_child[ 0 ] );
}

// Receives stmt or small_stmt
// Provides the meaningful node to process or NULL
static node *
getNodeToProcess( node *  tree )
{
    assert( tree->n_type == stmt ||
            tree->n_type == small_stmt ||
            tree->n_type == simple_stmt );

    if ( tree->n_type == simple_stmt )
        return tree;
    if ( tree->n_type == small_stmt )
        return getSmallStatementNodeToProcess( tree );
    return getStmtNodeToProcess( tree );
}


static FragmentBase *
addCodeBlock( Context *  context,
              CodeBlock **  codeBlock,
              Py::List &    flow,
              FragmentBase *  parent )
{
    if ( *codeBlock == NULL )
        return NULL;

    CodeBlock *     p( *codeBlock );

    Fragment *      body( new Fragment );
    body->parent = p;

    node *          firstNode = (node *)(p->firstNode);
    node *          lastItem = NULL;

    updateBegin( body, firstNode, context );

    node *          lastNode = findLastPart( (node *)(p->lastNode) );

    updateEnd( body, lastNode, context );

    p->updateBeginEnd( body );
    p->body = Py::asObject( body );

    injectComments( context, flow, parent, p, p );
    flow.append( Py::asObject( p ) );
    *codeBlock = NULL;
    return p;
}


// Creates the code block and sets the beginning and the end of the block
static CodeBlock *
createCodeBlock( node *  tree, FragmentBase *  parent, Context *  context )
{
    CodeBlock *     codeBlock( new CodeBlock );
    codeBlock->parent = parent;

    codeBlock->firstNode = tree;
    codeBlock->lastNode = tree;

    node *      last = findLastPart( tree );

    Fragment    temp;
    updateEnd( &temp, last, context );

    codeBlock->lastLine = temp.endLine;
    return codeBlock;
}


// Adds a statement to the code block and updates the end of the block
static void
addToCodeBlock( CodeBlock *  codeBlock, node *  tree, Context *  context )
{
    codeBlock->lastNode = tree;

    node *      last = findLastPart( tree );

    Fragment    temp;
    updateEnd( &temp, last, context );

    codeBlock->lastLine = temp.endLine;
}


// The function is used for Python 3.7 and below
static int
getStringFirstLine( node *  n )
{
    n = findLastPart( n );
    if ( n->n_type != STRING || n->n_str == NULL )
        return n->n_lineno;

    std::deque< const char * >  newLines;
    int                         newLineCount;
    int                         charCount;

    getNewLineParts( n->n_str, newLines, newLineCount, charCount );
    return n->n_lineno - newLineCount;
}


static int
getNextLineAfter( node *  start, int  lineNumber )
{
    for ( int  i = 0; i < start->n_nchildren; ++i )
    {
        node *      child = & ( start->n_child[ i ] );
        if ( child->n_lineno > lineNumber )
            return child->n_lineno;
        int nestedLineNo = getNextLineAfter( child, lineNumber );
        if ( nestedLineNo != INT_MAX )
            return nestedLineNo;
    }
    return INT_MAX;
}


static void
injectTrailingComments( Context *       context,
                        FragmentBase *  parent,
                        INT_TYPE        lastProcessedLine,
                        INT_TYPE        blockShift )
{
    if ( context->comments->empty() )
        return;

    size_t      flowStackSize = context->flowStack.size();
    if ( flowStackSize <= 1 )
        return;     // That's the global scope

    // find out a line number till which the comments should be checked.
    // Limit line is searched in trees above.
    int     nextStatementLine( INT_MAX );
    int     treeLevel( flowStackSize - 2 );

    while ( treeLevel >= 0 )
    {
        node *      upperTree = context->nodeStack[ treeLevel ];

        nextStatementLine = getNextLineAfter( upperTree, lastProcessedLine );
        if ( nextStatementLine != INT_MAX )
            break;  // Found the limit line

        --treeLevel;
    }

    Py::List *      flowToAddTo( context->flowStack[ flowStackSize - 1 ] );

    // Add to the flow on top
    while ( ! context->comments->empty() )
    {
        CommentLine &       comment = context->comments->front();
        if ( comment.line >= nextStatementLine )
            break;

        if ( comment.pos < blockShift )
            break;

        int     leadingLastLine = detectLeadingBlock( context,
                                                      nextStatementLine,
                                                      blockShift );

        injectOneLeadingComment( context, *flowToAddTo, parent,
                                 NULL, NULL, -1, false, leadingLastLine );
    }
}


static FragmentBase *
walk( Context *                    context,
      node *                       tree,
      FragmentBase *               parent,
      Py::List &                   flow,
      bool                         docstrProcessed )
{
    CodeBlock *         codeBlock = NULL;
    FragmentBase *      lastAdded = NULL;
    int                 statementCount = 0;

    context->flowStack.push_back( &flow );
    context->nodeStack.push_back( tree );

    for ( int  i = 0; i < tree->n_nchildren; ++i )
    {
        node *      child = & ( tree->n_child[ i ] );
        if ( child->n_type != stmt  && child->n_type != simple_stmt )
            continue;

        ++statementCount;

        node *      nodeToProcess = getNodeToProcess( child );
        if ( nodeToProcess == NULL )
            continue;

        switch ( nodeToProcess->n_type )
        {
            case simple_stmt:
                // need to walk over the small_stmt
                for ( int  k = 0; k < nodeToProcess->n_nchildren; ++k )
                {
                    node *      simpleChild = & ( nodeToProcess->n_child[ k ] );
                    if ( simpleChild->n_type != small_stmt )
                        continue;

                    if ( k != 0 )
                        ++statementCount;

                    node *      nodeToProcess = getNodeToProcess( simpleChild );
                    if ( nodeToProcess == NULL )
                        continue;

                    switch ( nodeToProcess->n_type )
                    {
                        case import_stmt:
                            addCodeBlock( context, & codeBlock, flow, parent );
                            lastAdded = processImport( context, nodeToProcess,
                                                       parent, flow );
                            continue;
                        case assert_stmt:
                            addCodeBlock( context, & codeBlock, flow, parent );
                            lastAdded = processAssert( context, nodeToProcess,
                                                       parent, flow );
                            continue;
                        case break_stmt:
                            addCodeBlock( context, & codeBlock, flow, parent );
                            lastAdded = processBreak( context, nodeToProcess,
                                                      parent, flow );
                            continue;
                        case continue_stmt:
                            addCodeBlock( context, & codeBlock, flow, parent );
                            lastAdded = processContinue( context, nodeToProcess,
                                                         parent, flow );
                            continue;
                        case return_stmt:
                            addCodeBlock( context, & codeBlock, flow, parent );
                            lastAdded = processReturn( context, nodeToProcess,
                                                       parent, flow );
                            continue;
                        case raise_stmt:
                            addCodeBlock( context, & codeBlock, flow, parent );
                            lastAdded = processRaise( context, nodeToProcess,
                                                      parent, flow );
                            continue;
                        default: ;
                    }

                    FragmentBase *  sysExit( checkForSysExit( context,
                                                              simpleChild,
                                                              flow,
                                                              parent ) );
                    if ( sysExit != NULL )
                    {
                        addCodeBlock( context, & codeBlock, flow, parent );

                        // NB: the 'checkForSysExit() does not inject comments
                        // because they first must be injected for the current
                        // code block. The current block comments are injected
                        // in the addCodeBlock() function so here the comments
                        // are injected for sys.exit() only
                        injectComments( context, flow, parent,
                                        static_cast<SysExit *>(sysExit),
                                        sysExit );
                        flow.append( Py::asObject(
                                            static_cast<SysExit *>(sysExit) ) );
                        lastAdded = sysExit;
                        continue;
                    }

                    // Some other statement
                    if ( statementCount == 1 && docstrProcessed )
                        continue;   // That's a docstring

                    // Not a docstring => add it to the code block
                    if ( codeBlock == NULL )
                    {
                        codeBlock = createCodeBlock( nodeToProcess, parent, context );
                    }
                    else
                    {
                        #if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 8
                            // Python 3.8 always has the correct first line
                            int     realFirstLine = nodeToProcess->n_lineno;
                        #else
                            // Python 3.7 and below: if it a multiline string
                            // literal then we do not have the first line.
                            // We only have the last line
                            int     realFirstLine;
                            if ( nodeToProcess->n_col_offset == -1 )
                                realFirstLine = getStringFirstLine( nodeToProcess );
                            else
                                realFirstLine = nodeToProcess->n_lineno;
                        #endif

                        if ( realFirstLine - codeBlock->lastLine > 1 )
                        {
                            lastAdded = addCodeBlock( context, & codeBlock, flow,
                                                      parent );
                            codeBlock = createCodeBlock( nodeToProcess, parent, context );
                        }
                        else
                        {
                            addToCodeBlock( codeBlock, nodeToProcess, context );
                        }
                    }
                }
                continue;
            case async_stmt:
                {
                    addCodeBlock( context, & codeBlock, flow, parent );
                    node *      asyncStmtNode = & ( nodeToProcess->n_child[ 1 ] );
                    if ( asyncStmtNode->n_type == funcdef )
                    {
                        std::list<Decorator *>      noDecors;
                        lastAdded = processFuncDefinition( context, nodeToProcess,
                                                           parent, flow,
                                                           noDecors );
                    }
                    else if ( asyncStmtNode->n_type == with_stmt )
                    {
                        lastAdded = processWith( context, nodeToProcess,
                                                 parent, flow );
                    }
                    else if ( asyncStmtNode->n_type == for_stmt )
                    {
                        lastAdded = processFor( context, nodeToProcess,
                                                parent, flow );
                    }
                }
                continue;
            case if_stmt:
                addCodeBlock( context, & codeBlock, flow, parent );
                lastAdded = processIf( context, nodeToProcess, parent, flow );
                continue;
            case while_stmt:
                addCodeBlock( context, & codeBlock, flow, parent );
                lastAdded = processWhile( context, nodeToProcess, parent, flow );
                continue;
            case for_stmt:
                addCodeBlock( context, & codeBlock, flow, parent );
                lastAdded = processFor( context, nodeToProcess, parent, flow );
                continue;
            case try_stmt:
                addCodeBlock( context, & codeBlock, flow, parent );
                lastAdded = processTry( context, nodeToProcess, parent, flow );
                continue;
            case with_stmt:
                addCodeBlock( context, & codeBlock, flow, parent );
                lastAdded = processWith( context, nodeToProcess, parent, flow );
                continue;
            case funcdef:
                {
                    std::list<Decorator *>      noDecors;
                    addCodeBlock( context, & codeBlock, flow, parent );
                    lastAdded = processFuncDefinition( context, nodeToProcess,
                                                       parent, flow,
                                                       noDecors );
                }
                continue;
            case classdef:
                {
                    std::list<Decorator *>      noDecors;
                    addCodeBlock( context, & codeBlock, flow, parent );
                    lastAdded = processClassDefinition( context, nodeToProcess,
                                                        parent, flow,
                                                        noDecors );
                }
                continue;
            case decorated:
                {
                    // funcdef or classdef follows
                    if ( nodeToProcess->n_nchildren < 2 )
                        continue;

                    node *  decorsNode = & ( nodeToProcess->n_child[ 0 ] );
                    node *  classOrFuncNode = & ( nodeToProcess->n_child[ 1 ] );

                    if ( decorsNode->n_type != decorators )
                        continue;

                    std::list<Decorator *>      decors =
                            processDecorators( context, flow, parent,
                                               decorsNode );

                    if ( classOrFuncNode->n_type == funcdef )
                    {
                        addCodeBlock( context, & codeBlock, flow, parent );
                        lastAdded = processFuncDefinition( context,
                                                           classOrFuncNode,
                                                           parent, flow,
                                                           decors );
                    }
                    else if ( classOrFuncNode->n_type == classdef )
                    {
                        addCodeBlock( context, & codeBlock, flow, parent );
                        lastAdded = processClassDefinition( context,
                                                            classOrFuncNode,
                                                            parent, flow,
                                                            decors );
                    }
                    else if ( classOrFuncNode->n_type == async_funcdef )
                    {
                        addCodeBlock( context, & codeBlock, flow, parent );
                        lastAdded = processFuncDefinition( context,
                                                           classOrFuncNode,
                                                           parent, flow,
                                                           decors );
                    }
                }
                continue;
        }
    }

    // Add block if needed
    if ( codeBlock != NULL )
    {
        lastAdded = addCodeBlock( context, & codeBlock, flow, parent );
    }

    // There could be trailing comments that belong to the upper level flow
    if ( lastAdded == NULL )
    {
        if ( context->lastDocstring != NULL )
        {
            injectTrailingComments( context, parent,
                                    context->lastDocstring->endLine,
                                    context->lastDocstring->beginPos );
        }
        else
        {
            // This is the case when a file is empty or has comments only
        }
    }
    else
    {
        injectTrailingComments( context, parent,
                                lastAdded->endLine, lastAdded->beginPos );
    }

    context->nodeStack.pop_back();
    context->flowStack.pop_back();
    return lastAdded;
}



static int
getLastFileCommentLine( Context * context, int  expectedFirstLine )
{
    // expectedFirstLine is 1 or the next line after hashbang or encoding line.
    // There is one peculiar case to consider:
    // - there is no hashbang line, but the '#' character is there
    // - there is encoding line and it is at line 2
    // => here the expected line will be 3 but the first comment will be at
    //    line 1. It was decided to do the following in this case:
    // - if the line 1 comment is not empty, then the comment is considered as
    //   for a file one. if the comment is empty then it is discarded and the
    //   comment for a file is searched as there was no that empty comment.

    if ( context->comments->empty() )
        return -1;      // No comment for the file

    const CommentLine &     first = context->comments->front();
    if ( first.line > expectedFirstLine )
        return -1;      // No comment for the file


    if ( first.line < expectedFirstLine )
    {
        // Special case described above
        std::string     content( & context->buffer[ first.begin ],
                                 first.end - first.begin + 1 );
        trimInplace( content );
        if ( content == "#" )
        {
            context->comments->pop_front();

            if ( context->comments->empty() )
                return -1;      // No comment for the file
            if ( context->comments->front().line > expectedFirstLine )
                return -1;      // No comment for the file
        }
        else
        {
            // The very first line is not empty, so use it for the file comment
            return first.line;
        }
    }

    // Check the lines after the encoding and hashbang
    int     lastInBlock( expectedFirstLine );
    for ( std::deque< CommentLine >::const_iterator
            k = context->comments->begin();
            k != context->comments->end(); ++k )
    {
        if ( k->line > lastInBlock + 1 )
            break;

        lastInBlock = k->line;
    }

    return lastInBlock;
}



Py::Object  parseInput( const char *  buffer, const char *  fileName,
                        bool  serialize )
{
    ControlFlow *           controlFlow = new ControlFlow();

    if ( serialize )
        controlFlow->content = buffer;

    perrdetail          error;
    PyCompilerFlags     flags = { 0 };
    node *              tree = PyParser_ParseStringFlagsFilename(
                                    buffer, fileName, &_PyParser_Grammar,
                                    file_input, &error, flags.cf_flags );

    if ( tree == NULL )
    {
        int             line;
        int             column;
        std::string     message;

        getErrorMessage( & error, line, column, message );
        controlFlow->addError( line, column, message );
        PyErr_Clear();
    }
    else
    {
        /* Walk the tree and populate the python structures */
        node *      root = tree;
        int         totalLines = getTotalLines( tree );
        int         fileCommentFirstLine = 1;

        assert( totalLines >= 0 );
        int                         lineShifts[ totalLines + 1 ];
        std::deque< CommentLine >   comments;

        getLineShiftsAndComments( buffer, lineShifts, comments );
        FragmentBase *      bang = checkForBangLine( buffer, controlFlow,
                                                     comments );
        if ( bang != NULL )
            fileCommentFirstLine = bang->beginLine + 1;

        if ( root->n_type == encoding_decl )
        {
            FragmentBase *  encoding = processEncoding( buffer, tree,
                                                        controlFlow, comments );
            root = & (root->n_child[ 0 ]);
            if ( encoding != NULL )
                fileCommentFirstLine = encoding->beginLine + 1;
        }


        assert( root->n_type == file_input );


        // Walk the syntax tree
        Context         context;
        context.flow = controlFlow;
        context.buffer = buffer;
        context.lineShifts = lineShifts;
        context.comments = & comments;

        // A file may also have leading comments
        int     lastFileCommentLine = getLastFileCommentLine( & context,
                                                              fileCommentFirstLine );
        if ( lastFileCommentLine != -1 )
        {
            // A leading comment for a file has been detected. Inject it to the
            // context object.
            injectLeadingComments( & context, controlFlow->nsuite,
                                   controlFlow, controlFlow, controlFlow,
                                   lastFileCommentLine + 1, false );
        }

        // Check for the docstring
        Docstring *  docstr = checkForDocstring( & context, root );
        if ( docstr != NULL )
        {
            docstr->parent = controlFlow;
            injectComments( & context, controlFlow->nsuite,
                            controlFlow, docstr, docstr );
            controlFlow->docstring = Py::asObject( docstr );
            controlFlow->updateBeginEnd( docstr );
        }

        walk( & context, root, controlFlow,
              controlFlow->nsuite, docstr != NULL );
        PyNode_Free( tree );

        // Inject trailing comments if so
        injectLeadingComments( & context, controlFlow->nsuite,
                               controlFlow, NULL, NULL, INT_MAX, false );

        if ( controlFlow->nsuite.size() > 0 )
        {
            // If there is nothing in the file => body is None
            // Here: there is something, so create the real body fragment, i.e.
            // everything except the 2 special purpose comment lines and
            // possible a comment for the file
            Fragment *      body( new Fragment );
            body->parent = controlFlow;

            Py::Object      fo = controlFlow->nsuite.front();
            body->begin = Py::Long( fo.getAttr( "begin" ) );
            body->beginLine = Py::Long( fo.getAttr( "beginLine" ) );
            body->beginPos = Py::Long( fo.getAttr( "beginPos" ) );

            // The control flow end had been already properly updated
            body->updateEnd( controlFlow );

            controlFlow->body = Py::asObject( body );
        }
    }

    return Py::asObject( controlFlow );
}

