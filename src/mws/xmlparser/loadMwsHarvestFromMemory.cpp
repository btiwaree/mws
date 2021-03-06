/*

Copyright (C) 2010-2013 KWARC Group <kwarc.info>

This file is part of MathWebSearch.

MathWebSearch is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MathWebSearch is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MathWebSearch.  If not, see <http://www.gnu.org/licenses/>.

*/
/**
  * @brief File containing the implementation of the loadMwsHarvestFromMemory
  * function
  *
  * @file loadMwsHarvestFromMemory.cpp
  * @author Daniel Hasegan
  * @date 10 Aug 2012
  *
  * License: GPL v3
  *
  */

// System includes

#include <stdlib.h>                    // C general purpose library
#include <stdio.h>                     // C Standrad Input Output
#include <string.h>                    // C string library
#include <libxml/tree.h>               // LibXML tree headers
#include <libxml/parser.h>             // LibXML parser headers
#include <libxml/parserInternals.h>    // LibXML parser internals API
#include <libxml/threads.h>            // LibXML thread handling API
#include <sys/types.h>                 // Primitive System datatypes
#include <sys/stat.h>                  // POSIX File characteristics
#include <fcntl.h>                     // File control operations
#include <sstream>                     // C++ stringstream header

// Local includes

#include "ParserTypes.hpp"             // Common Mws Parsers datatypes
#include "common/utils/DebugMacros.hpp"// Debug macros
#include "common/utils/macro_func.h"   // Macro functions
#include "mws/dbc/PageDbHandle.hpp"
#include "mws/dbc/PageDbConn.hpp"

// Macros

#define MWSHARVEST_MAIN_NAME           "mws:harvest"
#define MWSHARVEST_EXPR_NAME           "mws:expr"
#define MWSHARVEST_EXPR_ATTR_URI_NAME  "url"
#define MWSHARVEST_EXPR_URI_DEFAULT    ""

// Namespaces

using namespace std;
using namespace mws;

/**
  * @brief This function is called before the SAX handler starts parsing the
  * document
  *
  * @param user_data is a structure which holds the state of the parser.
  */
static void
my_startDocument(void* user_data)
{
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_IN;
#endif

    MwsHarvest_SaxUserData* data = (MwsHarvest_SaxUserData*) user_data;

    data->unknownDepth           = 0;
    data->currentToken           = NULL;
    data->currentTokenRoot       = NULL;
    data->state                  = MWSHARVESTSTATE_DEFAULT;
    data->prevState              = MWSHARVESTSTATE_DEFAULT;
    data->errorDetected          = false;
    data->parsedExpr             = 0;
    data->warnings               = 0;
    data->exprUri                = "";

#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_OUT;
#endif
}


/**
  * @brief This function is called after the SAX handler has finished parsing
  * the document
  *
  * @param user_data is a structure which holds the state of the parser.
  */
static void
my_endDocument(void* user_data)
{
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_IN;
#endif

    MwsHarvest_SaxUserData* data = (MwsHarvest_SaxUserData*) user_data;
    if (data->errorDetected)
    {
        fprintf(stderr, "Ending document due to errors\n");
    }

#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_OUT;
#endif
}


/**
  * @brief This function is called when the SAX handler encounters the
  * beginning of an element.
  *
  * @param user_data is a structure which holds the state of the parser.
  * @param name      is the name of the element which triggered the callback.
  * @param attrs     is an array of attributes and values, alternatively
  *                  placed.
  */
static void
my_startElement(void*           user_data,
                const xmlChar*  name,
                const xmlChar** attrs)
{
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_IN;
#endif

    MwsHarvest_SaxUserData* data = (MwsHarvest_SaxUserData*) user_data;

    switch (data->state)
    {
        case MWSHARVESTSTATE_DEFAULT:
            if (strcmp((char*) name, MWSHARVEST_MAIN_NAME) == 0)
            {
                data->state = MWSHARVESTSTATE_IN_MWS_HARVEST;
                // Parsing the attributes
                while (NULL != attrs && NULL != attrs[0])
                {
                    // NO ATTRIBUTES EXPECTED TODO HANDLE THEM
                    attrs = &attrs[2];
                }
            }
            else
            {
                data->warnings++;
                // Saving the state
                data->prevState = data->state;
                // Going to an unkown state
                data->state = MWSHARVESTSTATE_UNKNOWN;
                data->unknownDepth = 1;
            }
            break;

        case MWSHARVESTSTATE_IN_MWS_HARVEST:
            if (strcmp((char*) name, MWSHARVEST_EXPR_NAME) == 0)
            {
                data->state = MWSHARVESTSTATE_IN_MWS_EXPR;
                // Parsing the attributes
                while (NULL != attrs && NULL != attrs[0])
                {
                    if (strcmp((char*) attrs[0],
                                MWSHARVEST_EXPR_ATTR_URI_NAME) == 0)
                    {
                        data->exprUri = (char*) attrs[1];
                    }
                    else
                    {
                        // Invalid attribute
                        data->warnings++;
                        // TODO Move to logging
                        fprintf(stderr, "Unexpected attribute \"%s\"\n",
                                attrs[0]);
                        // Setting exprUri to default
                        data->exprUri = MWSHARVEST_EXPR_URI_DEFAULT;
                    }

                    attrs = &attrs[2];
                }
            }
            else
            {
                data->warnings++;
                // Saving the state
                data->prevState = data->state;
                // Going to an unkown state
                data->state = MWSHARVESTSTATE_UNKNOWN;
                data->unknownDepth = 1;
            }
            break;

        case MWSHARVESTSTATE_IN_MWS_EXPR:
            //Building the token
            if (data->currentToken != NULL)
            {
                data->currentToken = data->currentToken->newChildNode();
            }
            else
            {
                data->currentTokenRoot = CmmlToken::newRoot(true);
                data->currentToken = data->currentTokenRoot;
            }
            // Adding the tag name
            data->currentToken->setTag((char*) name);
            // Adding the attributes
            while (NULL != attrs && NULL != attrs[0])
            {
                data->currentToken->addAttribute((char*) attrs[0],
                                                 (char*) attrs[1]);
                attrs = &attrs[2];
            }
            break;

        case MWSHARVESTSTATE_UNKNOWN:
            data->unknownDepth++;
            break;
    }

#if DEBUG
    printf("Beginning of element : %s \n", name);
#endif
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_OUT;
#endif
}


/**
  * @brief This function is called when the SAX handler encounters the
  * end of an element.
  *
  * @param user_data is a structure which holds the state of the parser.
  * @param name      is the name of the element which triggered the callback.
  */
static void
my_endElement(void*          user_data,
              const xmlChar* name)
{
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_IN;
#endif

    MwsHarvest_SaxUserData* data = (MwsHarvest_SaxUserData*) user_data;

    switch (data->state)
    {
        case MWSHARVESTSTATE_DEFAULT:
            // Shouldn't happen
            // LOGGER!
            fprintf(stderr, "Unexpected Default State for end element \"%s\"",
                    (char*) name);
            break;

        case MWSHARVESTSTATE_IN_MWS_HARVEST:
            data->state = MWSHARVESTSTATE_DEFAULT;
            break;

        case MWSHARVESTSTATE_IN_MWS_EXPR:
            if (data->currentToken == NULL)
            {
                data->state = MWSHARVESTSTATE_IN_MWS_HARVEST;
            }
            else if (data->currentToken->isRoot())
            {
                list<CmmlToken*>::const_reverse_iterator rIt;
                stack<CmmlToken*>                        subtermStack;
                CmmlToken*                               currentSubterm;

                // Using a stack to insert all subterms by
                // going depth first through the CmmlToken
                subtermStack.push(data->currentToken);

                while (!subtermStack.empty())
                {
                    // Retrieving the subterm
                    currentSubterm = subtermStack.top();
                    subtermStack.pop();

                    // Inserting the children subterms
                    for (rIt  = currentSubterm->getChildNodes().rbegin();
                         rIt != currentSubterm->getChildNodes().rend();
                         rIt ++)
                    {
                        subtermStack.push(*rIt);
                    }
                    // Inserting the currentSubterm into the database
                    PageDbConn* conn = data->dbhandle->createConnection();
                    if (data->indexNode->insertData(currentSubterm,
                                            conn,
                                            data->exprUri.c_str(),
                                            currentSubterm->getXpath().c_str()))
                    {
                        data->parsedExpr++;
                    }
                    delete conn;
                }

                delete data->currentTokenRoot;
                data->currentToken = NULL;
                data->currentTokenRoot = NULL;
            }
            else
            {
                data->currentToken = data->currentToken->getParentNode();
            }
            break;

        case MWSHARVESTSTATE_UNKNOWN:
            data->unknownDepth--;
            if (data->unknownDepth == 0)
                data->state = data->prevState;
            break;
    }

#if DEBUG
    printf("Ending  of  element  : %s\n", (char*)name);
#endif
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_OUT;
#endif
}


static void
my_characters(void *user_data,
              const xmlChar *ch,
              int len)
{
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_IN;
#endif

    MwsHarvest_SaxUserData* data = (MwsHarvest_SaxUserData*) user_data;

    if (data->state == MWSHARVESTSTATE_IN_MWS_EXPR &&  // Valid state
        data->currentToken != NULL)                    // Valid token
    {
        data->currentToken->appendTextContent((char*) ch, len);
    }

#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_OUT;
#endif
}


static xmlEntityPtr
my_getEntity(void*          user_data,
             const xmlChar* name)
{
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_IN;
#endif
    UNUSED(user_data);
    // STUB from http://www.jamesh.id.au/articles/libxml-sax/libxml-sax.html
    // Also see http://xmlsoft.org/entities.html
    // GetPredefined will only work for &gt; &amp; ...
    xmlEntity* result = xmlGetPredefinedEntity(name);
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_OUT;
#endif
    return result;
} 


static void
my_warning(void*       user_data,
           const char* msg,
           ...)
{
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_IN;
#endif

    va_list args;
    MwsHarvest_SaxUserData* data = (MwsHarvest_SaxUserData*) user_data;

    data->warnings++;
    
    va_start(args, msg);
        vfprintf(stderr, msg, args);
    va_end(args);

#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_OUT;
#endif
}


static void
my_error(void*       user_data,
         const char* msg,
         ...)
{
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_IN;
#endif
    va_list args;
    MwsHarvest_SaxUserData* data = (MwsHarvest_SaxUserData*) user_data;

    data->errorDetected = true;
    if (data->currentTokenRoot)
    {
        delete data->currentTokenRoot;
        data->currentTokenRoot = NULL;
        data->currentToken = NULL;
    }
    va_start(args, msg);
        vfprintf(stderr, msg, args);
    va_end(args);

#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_OUT;
#endif
}


// ATM all errors are caught by error, so this is useless
static void
my_fatalError(void*       user_data,
              const char* msg,
              ...)
{
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_IN;
#endif
    UNUSED(user_data);

    va_list args;

    va_start(args, msg);
        vfprintf(stderr, msg, args);
    va_end(args);

#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_OUT;
#endif
}


// Implementation

namespace mws
{

pair<int,int>
loadMwsHarvestFromMemory(mws::MwsIndexNode* indexNode, char * mem, PageDbHandle *dbhandle)
{
#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_IN;
#endif

    MwsHarvest_SaxUserData user_data;
    xmlSAXHandler          saxHandler;
    xmlParserCtxtPtr       ctxtPtr;
    int                    ret;

    // Initializing the user_data and return value
    user_data.indexNode = indexNode;
    user_data.dbhandle = dbhandle;
    ret                 = -1;

    // Initializing the SAX Handler
    memset(&saxHandler, 0, sizeof(xmlSAXHandler));

    // Registering Sax callbacks with defined ones

    //internalSubsetSAXFunc        internalSubset;
    //isStandaloneSAXFunc          isStandalone;
    //hasInternalSubsetSAXFunc     hasInternalSubset;
    //hasExternalSubsetSAXFunc     hasExternalSubset;
    //resolveEntitySAXFunc         resolveEntity;
    saxHandler.getEntity     = my_getEntity;               // STUB
    //entityDeclSAXFunc            entityDecl;
    //notationDeclSAXFunc          notationDecl;
    //attributeDeclSAXFunc         attributeDecl;
    //elementDeclSAXFunc           elementDecl;
    //unparsedEntityDeclSAXFunc    unparsedEntityDecl;
    //setDocumentLocatorSAXFunc    setDocumentLocator;
    saxHandler.startDocument = my_startDocument;
    saxHandler.endDocument   = my_endDocument;
    saxHandler.startElement  = my_startElement;
    saxHandler.endElement    = my_endElement;
    //referenceSAXFunc             reference;
    saxHandler.characters    = my_characters;
    //ignorableWhitespaceSAXFunc   ignorableWhitespace;
    //processingInstructionSAXFunc processingInstruction;
    //commentSAXFunc               comment;
    saxHandler.warning       = my_warning;
    saxHandler.error         = my_error;
    saxHandler.fatalError    = my_fatalError;

    // Locking libXML -- to allow multi-threaded use
    xmlLockLibrary();
    
    // Creating the IOParser context
    if ((ctxtPtr = xmlCreatePushParserCtxt(&saxHandler,
                                         &user_data,
                                         mem,
                                         strlen(mem),
                                         NULL))
            == NULL)
    {
        fprintf(stderr, "Error while creating the ParserContext\n");
    }
    // Parsing the document
    else if ((ret = xmlParseDocument(ctxtPtr))
            == -1)
    {
        fprintf(stderr, "Parsing XML document failed\n");
    }

    // Freeing the parser context
    if (ctxtPtr)
        xmlFreeParserCtxt(ctxtPtr);

    // Unlocking libXML -- to allow multi-threaded use
    xmlUnlockLibrary();

#ifdef TRACE_FUNC_CALLS
    LOG_TRACE_OUT;
#endif
    return make_pair(ret, user_data.parsedExpr);
}

}
