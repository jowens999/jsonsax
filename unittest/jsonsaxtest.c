/*
  Copyright (c) 2012 John-Anthony Owens

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "jsonsax.h"

static int s_failureCount = 0;
static int s_failMalloc = 0;
static int s_failRealloc = 0;
static int s_failHandler = 0;
static int s_misbehaveInHandler = 0;
static size_t s_blocksAllocated = 0;
static size_t s_bytesAllocated = 0;

typedef struct tag_ParserState
{
    JSON_Error    error;
    JSON_Location errorLocation;
    JSON_Encoding inputEncoding;
} ParserState;

static void InitParserState(ParserState* pState)
{
    pState->error = JSON_Error_None;
    pState->errorLocation.byte = 0;
    pState->errorLocation.line = 0;
    pState->errorLocation.column = 0;
    pState->errorLocation.depth = 0;
    pState->inputEncoding = JSON_UnknownEncoding;
}

static void GetParserState(JSON_Parser parser, ParserState* pState)
{
    pState->error = JSON_Parser_GetError(parser);
    if (JSON_Parser_GetErrorLocation(parser, &pState->errorLocation) != JSON_Success)
    {
        pState->errorLocation.byte = 0;
        pState->errorLocation.line = 0;
        pState->errorLocation.column = 0;
        pState->errorLocation.depth = 0;
    }
    pState->inputEncoding = JSON_Parser_GetInputEncoding(parser);
}

static int ParserStatesAreIdentical(const ParserState* pState1, const ParserState* pState2)
{
    return (pState1->error == pState2->error &&
            pState1->errorLocation.byte == pState2->errorLocation.byte &&
            pState1->errorLocation.line == pState2->errorLocation.line &&
            pState1->errorLocation.column == pState2->errorLocation.column &&
            pState1->errorLocation.depth == pState2->errorLocation.depth &&
            pState1->inputEncoding == pState2->inputEncoding);
}

static int CheckParserState(JSON_Parser parser, const ParserState* pExpectedState)
{
    int isValid;
    ParserState actualState;
    GetParserState(parser, &actualState);
    isValid = ParserStatesAreIdentical(pExpectedState, &actualState);
    if (!isValid)
    {
        printf("FAILURE: parser state does not match\n"
               "  STATE                                 EXPECTED     ACTUAL\n"
               "  JSON_Parser_GetError()                %8d   %8d\n"
               "  JSON_Parser_GetErrorLocation() byte   %8d   %8d\n"
               "  JSON_Parser_GetErrorLocation() line   %8d   %8d\n"
               "  JSON_Parser_GetErrorLocation() column %8d   %8d\n"
               "  JSON_Parser_GetErrorLocation() depth  %8d   %8d\n"
               "  JSON_Parser_GetInputEncoding()        %8d   %8d\n"
               ,
               (int)pExpectedState->error, (int)actualState.error,
               (int)pExpectedState->errorLocation.byte, (int)actualState.errorLocation.byte,
               (int)pExpectedState->errorLocation.line, (int)actualState.errorLocation.line,
               (int)pExpectedState->errorLocation.column, (int)actualState.errorLocation.column,
               (int)pExpectedState->errorLocation.depth, (int)actualState.errorLocation.depth,
               (int)pExpectedState->inputEncoding, (int)actualState.inputEncoding
            );
    }
    return isValid;
}

typedef struct tag_ParserSettings
{
    void*         userData;
    JSON_Encoding inputEncoding;
    JSON_Encoding stringEncoding;
    size_t        maxStringLength;
    size_t        maxNumberLength;
    JSON_Boolean  allowBOM;
    JSON_Boolean  allowComments;
    JSON_Boolean  allowSpecialNumbers;
    JSON_Boolean  allowHexNumbers;
    JSON_Boolean  replaceInvalidEncodingSequences;
    JSON_Boolean  trackObjectMembers;
} ParserSettings;

static void InitParserSettings(ParserSettings* pSettings)
{
    pSettings->userData = NULL;
    pSettings->inputEncoding = JSON_UnknownEncoding;
    pSettings->stringEncoding = JSON_UTF8;
    pSettings->maxStringLength = (size_t)-1;
    pSettings->maxNumberLength = (size_t)-1;
    pSettings->allowBOM = JSON_False;
    pSettings->allowComments = JSON_False;
    pSettings->allowSpecialNumbers = JSON_False;
    pSettings->allowHexNumbers = JSON_False;
    pSettings->replaceInvalidEncodingSequences = JSON_False;
    pSettings->trackObjectMembers = JSON_False;
}

static void GetParserSettings(JSON_Parser parser, ParserSettings* pSettings)
{
    pSettings->userData = JSON_Parser_GetUserData(parser);
    pSettings->inputEncoding = JSON_Parser_GetInputEncoding(parser);
    pSettings->stringEncoding = JSON_Parser_GetStringEncoding(parser);
    pSettings->maxStringLength = JSON_Parser_GetMaxStringLength(parser);
    pSettings->maxNumberLength = JSON_Parser_GetMaxNumberLength(parser);
    pSettings->allowBOM = JSON_Parser_GetAllowBOM(parser);
    pSettings->allowComments = JSON_Parser_GetAllowComments(parser);
    pSettings->allowSpecialNumbers = JSON_Parser_GetAllowSpecialNumbers(parser);
    pSettings->allowHexNumbers = JSON_Parser_GetAllowHexNumbers(parser);
    pSettings->replaceInvalidEncodingSequences = JSON_Parser_GetReplaceInvalidEncodingSequences(parser);
    pSettings->trackObjectMembers = JSON_Parser_GetTrackObjectMembers(parser);
}

static int ParserSettingsAreIdentical(const ParserSettings* pSettings1, const ParserSettings* pSettings2)
{
    return (pSettings1->userData == pSettings2->userData &&
            pSettings1->inputEncoding == pSettings2->inputEncoding &&
            pSettings1->stringEncoding == pSettings2->stringEncoding &&
            pSettings1->maxStringLength == pSettings2->maxStringLength &&
            pSettings1->maxNumberLength == pSettings2->maxNumberLength &&
            pSettings1->allowBOM == pSettings2->allowBOM &&
            pSettings1->allowComments == pSettings2->allowComments &&
            pSettings1->allowSpecialNumbers == pSettings2->allowSpecialNumbers &&
            pSettings1->allowHexNumbers == pSettings2->allowHexNumbers &&
            pSettings1->replaceInvalidEncodingSequences == pSettings2->replaceInvalidEncodingSequences &&
            pSettings1->trackObjectMembers == pSettings2->trackObjectMembers);
}

static int CheckParserSettings(JSON_Parser parser, const ParserSettings* pExpectedSettings)
{
    int identical;
    ParserSettings actualSettings;
    GetParserSettings(parser, &actualSettings);
    identical = ParserSettingsAreIdentical(pExpectedSettings, &actualSettings);
    if (!identical)
    {
        printf("FAILURE: parser settings do not match\n"
               "  SETTINGS                                         EXPECTED     ACTUAL\n"
               "  JSON_Parser_GetUserData()                        %8p   %8p\n"
               "  JSON_Parser_GetInputEncoding()                   %8d   %8d\n"
               "  JSON_Parser_GetStringEncoding()                  %8d   %8d\n"
               "  JSON_Parser_GetMaxStringLength()           %8d   %8d\n"
               "  JSON_Parser_GetMaxNumberLength()                 %8d   %8d\n"
               ,
               pExpectedSettings->userData, actualSettings.userData,
               (int)pExpectedSettings->inputEncoding, (int)actualSettings.inputEncoding,
               (int)pExpectedSettings->stringEncoding, (int)actualSettings.stringEncoding,
               (int)pExpectedSettings->maxStringLength, (int)actualSettings.maxStringLength,
               (int)pExpectedSettings->maxNumberLength, (int)actualSettings.maxNumberLength
            );
        printf("  JSON_Parser_GetAllowBOM()                        %8d   %8d\n"
               "  JSON_Parser_GetAllowComments()                   %8d   %8d\n"
               "  JSON_Parser_GetAllowSpecialNumbers()             %8d   %8d\n"
               "  JSON_Parser_GetAllowHexNumbers()                 %8d   %8d\n"
               "  JSON_Parser_GetReplaceInvalidEncodingSequences() %8d   %8d\n"
               "  JSON_Parser_GetTrackObjectMembers()              %8d   %8d\n"
               ,
               (int)pExpectedSettings->allowBOM, (int)actualSettings.allowBOM,
               (int)pExpectedSettings->allowComments, (int)actualSettings.allowComments,
               (int)pExpectedSettings->allowSpecialNumbers, (int)actualSettings.allowSpecialNumbers,
               (int)pExpectedSettings->allowHexNumbers, (int)actualSettings.allowHexNumbers,
               (int)pExpectedSettings->replaceInvalidEncodingSequences, (int)actualSettings.replaceInvalidEncodingSequences,
               (int)pExpectedSettings->trackObjectMembers, (int)actualSettings.trackObjectMembers
            );
    }
    return identical;
}

typedef struct tag_ParserHandlers
{
    JSON_Parser_EncodingDetectedHandler encodingDetectedHandler;
    JSON_Parser_NullHandler             nullHandler;
    JSON_Parser_BooleanHandler          booleanHandler;
    JSON_Parser_StringHandler           stringHandler;
    JSON_Parser_NumberHandler           numberHandler;
    JSON_Parser_SpecialNumberHandler    specialNumberHandler;
    JSON_Parser_StartObjectHandler      startObjectHandler;
    JSON_Parser_EndObjectHandler        endObjectHandler;
    JSON_Parser_ObjectMemberHandler     objectMemberHandler;
    JSON_Parser_StartArrayHandler       startArrayHandler;
    JSON_Parser_EndArrayHandler         endArrayHandler;
    JSON_Parser_ArrayItemHandler        arrayItemHandler;
} ParserHandlers;

static void InitParserHandlers(ParserHandlers* pHandlers)
{
    pHandlers->encodingDetectedHandler = NULL;
    pHandlers->nullHandler = NULL;
    pHandlers->booleanHandler = NULL;
    pHandlers->stringHandler = NULL;
    pHandlers->numberHandler = NULL;
    pHandlers->specialNumberHandler = NULL;
    pHandlers->startObjectHandler = NULL;
    pHandlers->endObjectHandler = NULL;
    pHandlers->objectMemberHandler = NULL;
    pHandlers->startArrayHandler = NULL;
    pHandlers->endArrayHandler = NULL;
    pHandlers->arrayItemHandler = NULL;
}

static void GetParserHandlers(JSON_Parser parser, ParserHandlers* pHandlers)
{
    pHandlers->encodingDetectedHandler = JSON_Parser_GetEncodingDetectedHandler(parser);
    pHandlers->nullHandler = JSON_Parser_GetNullHandler(parser);
    pHandlers->booleanHandler = JSON_Parser_GetBooleanHandler(parser);
    pHandlers->stringHandler = JSON_Parser_GetStringHandler(parser);
    pHandlers->numberHandler = JSON_Parser_GetNumberHandler(parser);
    pHandlers->specialNumberHandler = JSON_Parser_GetSpecialNumberHandler(parser);
    pHandlers->startObjectHandler = JSON_Parser_GetStartObjectHandler(parser);
    pHandlers->endObjectHandler = JSON_Parser_GetEndObjectHandler(parser);
    pHandlers->objectMemberHandler = JSON_Parser_GetObjectMemberHandler(parser);
    pHandlers->startArrayHandler = JSON_Parser_GetStartArrayHandler(parser);
    pHandlers->endArrayHandler = JSON_Parser_GetEndArrayHandler(parser);
    pHandlers->arrayItemHandler = JSON_Parser_GetArrayItemHandler(parser);
}

static int ParserHandlersAreIdentical(const ParserHandlers* pHandlers1, const ParserHandlers* pHandlers2)
{
    return (pHandlers1->encodingDetectedHandler == pHandlers2->encodingDetectedHandler &&
            pHandlers1->nullHandler == pHandlers2->nullHandler &&
            pHandlers1->booleanHandler == pHandlers2->booleanHandler &&
            pHandlers1->stringHandler == pHandlers2->stringHandler &&
            pHandlers1->numberHandler == pHandlers2->numberHandler &&
            pHandlers1->specialNumberHandler == pHandlers2->specialNumberHandler &&
            pHandlers1->startObjectHandler == pHandlers2->startObjectHandler &&
            pHandlers1->endObjectHandler == pHandlers2->endObjectHandler &&
            pHandlers1->objectMemberHandler == pHandlers2->objectMemberHandler &&
            pHandlers1->startArrayHandler == pHandlers2->startArrayHandler &&
            pHandlers1->endArrayHandler == pHandlers2->endArrayHandler &&
            pHandlers1->arrayItemHandler == pHandlers2->arrayItemHandler);
}

#define HANDLER_STRING(p) ((p) ? "non-NULL" : "NULL")

static int CheckParserHandlers(JSON_Parser parser, const ParserHandlers* pExpectedHandlers)
{
    int identical;
    ParserHandlers actualHandlers;
    GetParserHandlers(parser, &actualHandlers);
    identical = ParserHandlersAreIdentical(pExpectedHandlers, &actualHandlers);
    if (!identical)
    {
        printf("FAILURE: parser handlers do not match\n"
               "  HANDLERS                                 EXPECTED     ACTUAL\n"
               "  JSON_Parser_GetEncodingDetectedHandler() %8s   %8s\n"
               "  JSON_Parser_GetNullHandler()             %8s   %8s\n"
               "  JSON_Parser_GetBooleanHandler()          %8s   %8s\n"
               "  JSON_Parser_GetStringHandler()           %8s   %8s\n"
               "  JSON_Parser_GetNumberHandler()           %8s   %8s\n"
               "  JSON_Parser_GetSpecialNumberHandler()    %8s   %8s\n"
               ,
               HANDLER_STRING(pExpectedHandlers->encodingDetectedHandler), HANDLER_STRING(actualHandlers.encodingDetectedHandler),
               HANDLER_STRING(pExpectedHandlers->nullHandler), HANDLER_STRING(actualHandlers.nullHandler),
               HANDLER_STRING(pExpectedHandlers->booleanHandler), HANDLER_STRING(actualHandlers.booleanHandler),
               HANDLER_STRING(pExpectedHandlers->stringHandler), HANDLER_STRING(actualHandlers.stringHandler),
               HANDLER_STRING(pExpectedHandlers->numberHandler), HANDLER_STRING(actualHandlers.numberHandler),
               HANDLER_STRING(pExpectedHandlers->specialNumberHandler), HANDLER_STRING(actualHandlers.specialNumberHandler)
            );
        printf("  JSON_Parser_GetStartObjectHandler()      %8s   %8s\n"
               "  JSON_Parser_GetEndObjectHandler()        %8s   %8s\n"
               "  JSON_Parser_GetObjectMemberHandler()     %8s   %8s\n"
               "  JSON_Parser_GetStartArrayHandler()       %8s   %8s\n"
               "  JSON_Parser_GetEndArrayHandler()         %8s   %8s\n"
               "  JSON_Parser_GetArrayItemHandler()        %8s   %8s\n"
               ,
               HANDLER_STRING(pExpectedHandlers->startObjectHandler), HANDLER_STRING(actualHandlers.startObjectHandler),
               HANDLER_STRING(pExpectedHandlers->endObjectHandler), HANDLER_STRING(actualHandlers.endObjectHandler),
               HANDLER_STRING(pExpectedHandlers->objectMemberHandler), HANDLER_STRING(actualHandlers.objectMemberHandler),
               HANDLER_STRING(pExpectedHandlers->startArrayHandler), HANDLER_STRING(actualHandlers.startArrayHandler),
               HANDLER_STRING(pExpectedHandlers->endArrayHandler), HANDLER_STRING(actualHandlers.endArrayHandler),
               HANDLER_STRING(pExpectedHandlers->arrayItemHandler), HANDLER_STRING(actualHandlers.arrayItemHandler)
            );
    }
    return identical;
}

static int CheckParserHasDefaultValues(JSON_Parser parser)
{
    ParserState state;
    ParserSettings settings;
    ParserHandlers handlers;
    InitParserState(&state);
    InitParserSettings(&settings);
    InitParserHandlers(&handlers);
    return CheckParserState(parser, &state) &&
           CheckParserSettings(parser, &settings) &&
           CheckParserHandlers(parser, &handlers);
}

static int CheckParserCreate(const JSON_MemorySuite* pMemorySuite, JSON_Status expectedStatus, JSON_Parser* pParser)
{
    *pParser = JSON_Parser_Create(pMemorySuite);
    if (expectedStatus == JSON_Success && !*pParser)
    {
        printf("FAILURE: expected JSON_Parser_Create() to return a parser instance\n");
        return 0;
    }
    if (expectedStatus == JSON_Failure && *pParser)
    {
        printf("FAILURE: expected JSON_Parser_Create() to return NULL\n");
        JSON_Parser_Free(*pParser);
        *pParser = NULL;
        return 0;
    }
    return 1;
}

static int CheckParserCreateWithCustomMemorySuite(JSON_ReallocHandler r, JSON_FreeHandler f, JSON_Status expectedStatus, JSON_Parser* pParser)
{
    JSON_MemorySuite memorySuite;
    memorySuite.userData = NULL;
    memorySuite.realloc = r;
    memorySuite.free = f;
    return CheckParserCreate(&memorySuite, expectedStatus, pParser);
}

static int CheckParserReset(JSON_Parser parser, JSON_Status expectedStatus)
{
    if (JSON_Parser_Reset(parser) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_Reset() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserFree(JSON_Parser parser, JSON_Status expectedStatus)
{
    if (JSON_Parser_Free(parser) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_Free() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserGetError(JSON_Parser parser, JSON_Location* pLocation, JSON_Status expectedStatus)
{
    if (JSON_Parser_GetErrorLocation(parser, pLocation) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_GetErrorLocation() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserGetTokenLocation(JSON_Parser parser, JSON_Location* pLocation, JSON_Status expectedStatus)
{
    if (JSON_Parser_GetTokenLocation(parser, pLocation) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_GetTokenLocation() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetUserData(JSON_Parser parser, void* userData, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetUserData(parser, userData) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetUserData() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetInputEncoding(JSON_Parser parser, JSON_Encoding encoding, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetInputEncoding(parser, encoding) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetInputEncoding() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetStringEncoding(JSON_Parser parser, JSON_Encoding encoding, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetStringEncoding(parser, encoding) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetStringEncoding() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetMaxStringLength(JSON_Parser parser, size_t maxLength, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetMaxStringLength(parser, maxLength) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetMaxStringLength() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetMaxNumberLength(JSON_Parser parser, size_t maxLength, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetMaxNumberLength(parser, maxLength) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetMaxNumberLength() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetAllowBOM(JSON_Parser parser, JSON_Boolean allowBOM, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetAllowBOM(parser, allowBOM) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetAllowBOM() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetAllowComments(JSON_Parser parser, JSON_Boolean allowComments, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetAllowComments(parser, allowComments) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetAllowComments() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetAllowSpecialNumbers(JSON_Parser parser, JSON_Boolean allowSpecialNumbers, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetAllowSpecialNumbers(parser, allowSpecialNumbers) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetAllowSpecialNumbers() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetAllowHexNumbers(JSON_Parser parser, JSON_Boolean allowHexNumbers, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetAllowHexNumbers(parser, allowHexNumbers) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetAllowHexNumbers() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetReplaceInvalidEncodingSequences(JSON_Parser parser, JSON_Boolean replaceInvalidEncodingSequences, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetReplaceInvalidEncodingSequences(parser, replaceInvalidEncodingSequences) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetReplaceInvalidEncodingSequences() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetTrackObjectMembers(JSON_Parser parser, JSON_Boolean trackObjectMembers, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetTrackObjectMembers(parser, trackObjectMembers) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetTrackObjectMembers() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetEncodingDetectedHandler(JSON_Parser parser, JSON_Parser_EncodingDetectedHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetEncodingDetectedHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetEncodingDetectedHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetNullHandler(JSON_Parser parser, JSON_Parser_NullHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetNullHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetNullHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetBooleanHandler(JSON_Parser parser, JSON_Parser_BooleanHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetBooleanHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetBooleanHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetStringHandler(JSON_Parser parser, JSON_Parser_StringHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetStringHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetStringHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetNumberHandler(JSON_Parser parser, JSON_Parser_NumberHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetNumberHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetNumberHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetSpecialNumberHandler(JSON_Parser parser, JSON_Parser_SpecialNumberHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetSpecialNumberHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetSpecialNumberHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetStartObjectHandler(JSON_Parser parser, JSON_Parser_StartObjectHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetStartObjectHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetStartObjectHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetEndObjectHandler(JSON_Parser parser, JSON_Parser_EndObjectHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetEndObjectHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetEndObjectHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetObjectMemberHandler(JSON_Parser parser, JSON_Parser_ObjectMemberHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetObjectMemberHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetObjectMemberHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetStartArrayHandler(JSON_Parser parser, JSON_Parser_StartArrayHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetStartArrayHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetStartArrayHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetEndArrayHandler(JSON_Parser parser, JSON_Parser_EndArrayHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetEndArrayHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetEndArrayHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserSetArrayItemHandler(JSON_Parser parser, JSON_Parser_ArrayItemHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Parser_SetArrayItemHandler(parser, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_SetArrayItemHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckParserParse(JSON_Parser parser, const char* pBytes, size_t length, JSON_Boolean isFinal, JSON_Status expectedStatus)
{
    if (JSON_Parser_Parse(parser, pBytes, length, isFinal) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Parser_Parse() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckErrorString(JSON_Error error, const char* pExpectedMessage)
{
    const char* pActualMessage = JSON_ErrorString(error);
    if (strcmp(pExpectedMessage, pActualMessage))
    {
        printf("FAILURE: expected JSON_ErrorString() to return \"%s\" instead of \"%s\"\n", pExpectedMessage, pActualMessage);
        return 0;
    }
    return 1;
}

static void* JSON_CALL ReallocHandler(void* caller, void* ptr, size_t size)
{
    size_t* pBlock = NULL;
    (void)caller; /* unused */
    if ((!ptr && !s_failMalloc) || (ptr && !s_failRealloc))
    {
        size_t newBlockSize = sizeof(size_t) + size;
        size_t oldBlockSize;
        pBlock = (size_t*)ptr;
        if (pBlock)
        {
            pBlock--; /* actual block begins before client's pointer */
            oldBlockSize = *pBlock;
        }
        else
        {
            oldBlockSize = 0;
        }
        pBlock = (size_t*)realloc(pBlock, newBlockSize);
        if (pBlock)
        {
            if (!oldBlockSize)
            {
                s_blocksAllocated++;
            }
            s_bytesAllocated += newBlockSize - oldBlockSize;
            *pBlock = newBlockSize;
            pBlock++; /* return address to memory after block size */
        }
    }
    return pBlock;
}

static void JSON_CALL FreeHandler(void* caller, void* ptr)
{
    (void)caller; /* unused */
    if (ptr)
    {
        size_t* pBlock = (size_t*)ptr;
        pBlock--; /* actual block begins before client's pointer */
        s_blocksAllocated--;
        s_bytesAllocated -= *pBlock;
        free(pBlock);
    }
}

static char s_outputBuffer[4096]; /* big enough for all unit tests */
size_t s_outputLength = 0;

static void OutputFormatted(const char* pFormat, ...)
{
    va_list args;
    int length;
    va_start(args, pFormat);
    length = vsprintf(&s_outputBuffer[s_outputLength], pFormat, args);
    va_end(args);
    s_outputLength += length;
    s_outputBuffer[s_outputLength] = 0;
}

static void OutputCharacter(char c)
{
    s_outputBuffer[s_outputLength] = c;
    s_outputLength++;
    s_outputBuffer[s_outputLength] = 0;
}

static void OutputSeparator()
{
    if (s_outputLength && s_outputBuffer[s_outputLength] != ' ')
    {
        OutputCharacter(' ');
    }
}

static const char s_hexDigits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
static void OutputStringBytes(const char* pBytes, size_t length, JSON_StringAttributes attributes)
{
    size_t i;
    if (attributes != JSON_SimpleString)
    {
        if (attributes & JSON_ContainsNullCharacter)
        {
            OutputCharacter('z');
        }
        if (attributes & JSON_ContainsControlCharacter)
        {
            OutputCharacter('c');
        }
        if (attributes & JSON_ContainsNonASCIICharacter)
        {
            OutputCharacter('a');
        }
        if (attributes & JSON_ContainsNonBMPCharacter)
        {
            OutputCharacter('b');
        }
        if (attributes & JSON_ContainsReplacedCharacter)
        {
            OutputCharacter('r');
        }
        if (length)
        {
            OutputCharacter(' ');
        }
    }
    for (i = 0; i < length; i++)
    {
        unsigned char b = (unsigned char)pBytes[i];
        if (i)
        {
            OutputCharacter(' ');
        }
        OutputCharacter(s_hexDigits[b >> 4]);
        OutputCharacter(s_hexDigits[b & 0xF]);
    }
}

static void OutputNumber(const char* pBytes, JSON_NumberAttributes attributes)
{
    if (attributes != JSON_SimpleNumber)
    {
        if (attributes & JSON_IsNegative)
        {
            OutputCharacter('-');
        }
        if (attributes & JSON_IsHex)
        {
            OutputCharacter('x');
        }
        if (attributes & JSON_ContainsDecimalPoint)
        {
            OutputCharacter('.');
        }
        if (attributes & JSON_ContainsExponent)
        {
            OutputCharacter('e');
        }
        if (attributes & JSON_ContainsNegativeExponent)
        {
            OutputCharacter('-');
        }
        OutputCharacter(' ');
    }
    OutputFormatted("%s", pBytes);
}

static void OutputLocation(const JSON_Location* pLocation)
{
    OutputFormatted("%d,%d,%d,%d", (int)pLocation->byte, (int)pLocation->line, (int)pLocation->column, (int)pLocation->depth);
}

static int CheckOutput(const char* pExpectedOutput)
{
    if (strcmp(pExpectedOutput, s_outputBuffer))
    {
        printf("FAILURE: output does not match expected\n"
               "  EXPECTED %s\n"
               "  ACTUAL   %s\n", pExpectedOutput, s_outputBuffer);
        return 0;
    }
    return 1;
}

static void ResetOutput()
{
    s_outputLength = 0;
    s_outputBuffer[0] = 0;
}

static int TryToMisbehaveInParseHandler(JSON_Parser parser)
{
    if (!CheckParserFree(parser, JSON_Failure) ||
        !CheckParserReset(parser, JSON_Failure) ||
        !CheckParserGetTokenLocation(parser, NULL, JSON_Failure) ||
        !CheckParserSetInputEncoding(parser, JSON_UTF32LE, JSON_Failure) ||
        !CheckParserSetStringEncoding(parser, JSON_UTF32LE, JSON_Failure) ||
        !CheckParserSetAllowBOM(parser, JSON_True, JSON_Failure) ||
        !CheckParserSetAllowComments(parser, JSON_True, JSON_Failure) ||
        !CheckParserSetAllowSpecialNumbers(parser, JSON_True, JSON_Failure) ||
        !CheckParserSetAllowHexNumbers(parser, JSON_True, JSON_Failure) ||
        !CheckParserSetReplaceInvalidEncodingSequences(parser, JSON_True, JSON_Failure) ||
        !CheckParserSetTrackObjectMembers(parser, JSON_True, JSON_Failure) ||
        !CheckParserParse(parser, " ", 1, JSON_False, JSON_Failure))
    {
        return 1;
    }
    return 0;
}

static JSON_Parser_HandlerResult JSON_CALL EncodingDetectedHandler(JSON_Parser parser)
{
    JSON_Location location;
    const char* pszEncoding = "";
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Failure)
    {
        return JSON_Parser_Abort;
    }
    switch (JSON_Parser_GetInputEncoding(parser))
    {
    case JSON_UTF8:
        pszEncoding = "8";
        break;
    case JSON_UTF16LE:
        pszEncoding = "16LE";
        break;
    case JSON_UTF16BE:
        pszEncoding = "16BE";
        break;
    case JSON_UTF32LE:
        pszEncoding = "32LE";
        break;
    case JSON_UTF32BE:
        pszEncoding = "32BE";
        break;
    default:
        break;
    }
    OutputSeparator();
    OutputFormatted("u(%s)", pszEncoding);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL NullHandler(JSON_Parser parser)
{
    JSON_Location location;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    OutputSeparator();
    OutputFormatted("n:");
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL BooleanHandler(JSON_Parser parser, JSON_Boolean value)
{
    JSON_Location location;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    OutputSeparator();
    OutputFormatted("%s:", (value == JSON_True) ? "t" : "f");
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL StringHandler(JSON_Parser parser, const char* pBytes, size_t length, JSON_StringAttributes attributes)
{
    JSON_Location location;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    OutputSeparator();
    OutputFormatted("s(");
    OutputStringBytes(pBytes, length, attributes);
    OutputFormatted("):");
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL NumberHandler(JSON_Parser parser, const char* pValue, size_t length, JSON_NumberAttributes attributes)
{
    JSON_Location location;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (strlen(pValue) != length)
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    OutputSeparator();
    OutputFormatted("#(");
    OutputNumber(pValue, attributes);
    OutputFormatted("):");
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL SpecialNumberHandler(JSON_Parser parser, JSON_SpecialNumber value)
{
    JSON_Location location;
    const char* pValue;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    switch (value)
    {
    case JSON_NaN:
        pValue = "NaN";
        break;
    case JSON_Infinity:
        pValue = "Infinity";
        break;
    case JSON_NegativeInfinity:
        pValue = "-Infinity";
        break;
    default:
        pValue = "UNKNOWN";
        break;
    }
    OutputSeparator();
    OutputFormatted("#(%s):", pValue);
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL StartObjectHandler(JSON_Parser parser)
{
    JSON_Location location;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    OutputSeparator();
    OutputFormatted("{:");
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL EndObjectHandler(JSON_Parser parser)
{
    JSON_Location location;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    OutputSeparator();
    OutputFormatted("}:");
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL ObjectMemberHandler(JSON_Parser parser, const char* pBytes, size_t length, JSON_StringAttributes attributes)
{
    JSON_Location location;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (attributes == JSON_SimpleString && !strcmp(pBytes, "duplicate"))
    {
        return JSON_Parser_TreatAsDuplicateObjectMember;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    OutputSeparator();
    OutputFormatted("m(");
    OutputStringBytes(pBytes, length, attributes);
    OutputFormatted("):");
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL StartArrayHandler(JSON_Parser parser)
{
    JSON_Location location;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    OutputSeparator();
    OutputFormatted("[:");
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL EndArrayHandler(JSON_Parser parser)
{
    JSON_Location location;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    OutputSeparator();
    OutputFormatted("]:");
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult JSON_CALL ArrayItemHandler(JSON_Parser parser)
{
    JSON_Location location;
    if (s_failHandler)
    {
        return JSON_Parser_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInParseHandler(parser))
    {
        return JSON_Parser_Abort;
    }
    if (JSON_Parser_GetTokenLocation(parser, &location) != JSON_Success)
    {
        return JSON_Parser_Abort;
    }
    OutputSeparator();
    OutputFormatted("i:");
    OutputLocation(&location);
    return JSON_Parser_Continue;
}

/* The length and content of this array MUST correspond exactly with the
   JSON_Error_XXX enumeration values defined in jsonsax.h. */
static const char* errorNames[] =
{
    "",
    "OutOfMemory",
    "AbortedByHandler",
    "BOMNotAllowed",
    "InvalidEncodingSequence",
    "UnknownToken",
    "UnexpectedToken",
    "IncompleteToken",
    "ExpectedMoreTokens",
    "UnescapedControlCharacter",
    "InvalidEscapeSequence",
    "UnpairedSurrogateEscapeSequence",
    "TooLongString",
    "InvalidNumber",
    "TooLongNumber",
    "DuplicateObjectMember"
};

typedef enum tag_ParserParam
{
    Standard = 0,

    /* Bottom 4 bits are input encoding. */
    DefaultIn = 0 << 0,
    UTF8In    = 1 << 0,
    UTF16LEIn = 2 << 0,
    UTF16BEIn = 3 << 0,
    UTF32LEIn = 4 << 0,
    UTF32BEIn = 5 << 0,

    /* Next 4 bits are output encoding. */
    DefaultOut = 0 << 4,
    UTF8Out    = 1 << 4,
    UTF16LEOut = 2 << 4,
    UTF16BEOut = 3 << 4,
    UTF32LEOut = 4 << 4,
    UTF32BEOut = 5 << 4,

    /* Next 2 bits are max string length. */
    DefaultMaxStringLength = 0 << 8,
    MaxStringLength0       = 1 << 8,
    MaxStringLength1       = 2 << 8,
    MaxStringLength2       = 3 << 8,

    /* Next 2 bits are max number length. */
    DefaultMaxNumberLength = 0 << 10,
    MaxNumberLength0       = 1 << 10,
    MaxNumberLength1       = 2 << 10,
    MaxNumberLength2       = 3 << 10,

    /* Rest of bits are settings. */
    AllowBOM                        = 1 << 12,
    AllowComments                   = 1 << 13,
    AllowSpecialNumbers             = 1 << 14,
    AllowHexNumbers                 = 1 << 15,
    ReplaceInvalidEncodingSequences = 1 << 16,
    TrackObjectMembers              = 1 << 17
} ParserParam;
typedef unsigned int ParserParams;

typedef struct tag_ParseTest
{
    const char*   pName;
    ParserParams  parserParams;
    const char*   pInput;
    size_t        length;
    JSON_Boolean  isFinal;
    JSON_Encoding inputEncoding;
    const char*   pOutput;
} ParseTest;

static void RunParseTest(const ParseTest* pTest)
{
    JSON_Parser parser = NULL;
    ParserSettings settings;
    ParserState state;
    printf("Test parsing %s ... ", pTest->pName);

    InitParserSettings(&settings);
    if ((pTest->parserParams & 0xF) != DefaultIn)
    {
        settings.inputEncoding = (JSON_Encoding)(pTest->parserParams & 0xF);
    }
    if ((pTest->parserParams & 0xF0) != DefaultOut)
    {
        settings.stringEncoding = (JSON_Encoding)((pTest->parserParams >> 4) & 0xF);
    }
    if ((pTest->parserParams & 0x300) != DefaultMaxStringLength)
    {
        settings.maxStringLength = (size_t)((pTest->parserParams >> 8) & 0x3) - 1;
    }
    if ((pTest->parserParams & 0xC00) != DefaultMaxNumberLength)
    {
        settings.maxNumberLength = (size_t)((pTest->parserParams >> 10) & 0x3) - 1;
    }
    settings.allowBOM = (JSON_Boolean)((pTest->parserParams >> 12) & 0x1);
    settings.allowComments = (JSON_Boolean)((pTest->parserParams >> 13) & 0x1);
    settings.allowSpecialNumbers = (JSON_Boolean)((pTest->parserParams >> 14) & 0x1);
    settings.allowHexNumbers = (JSON_Boolean)((pTest->parserParams >> 15) & 0x1);
    settings.replaceInvalidEncodingSequences = (JSON_Boolean)((pTest->parserParams >> 16) & 0x1);
    settings.trackObjectMembers = (JSON_Boolean)((pTest->parserParams >> 17) & 0x1);

    InitParserState(&state);
    state.inputEncoding = pTest->inputEncoding;
    ResetOutput();

    if (CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &parser) &&
        CheckParserSetEncodingDetectedHandler(parser, &EncodingDetectedHandler, JSON_Success) &&
        CheckParserSetNullHandler(parser, &NullHandler, JSON_Success) &&
        CheckParserSetBooleanHandler(parser, &BooleanHandler, JSON_Success) &&
        CheckParserSetStringHandler(parser, &StringHandler, JSON_Success) &&
        CheckParserSetNumberHandler(parser, &NumberHandler, JSON_Success) &&
        CheckParserSetSpecialNumberHandler(parser, &SpecialNumberHandler, JSON_Success) &&
        CheckParserSetStartObjectHandler(parser, &StartObjectHandler, JSON_Success) &&
        CheckParserSetEndObjectHandler(parser, &EndObjectHandler, JSON_Success) &&
        CheckParserSetObjectMemberHandler(parser, &ObjectMemberHandler, JSON_Success) &&
        CheckParserSetStartArrayHandler(parser, &StartArrayHandler, JSON_Success) &&
        CheckParserSetEndArrayHandler(parser, &EndArrayHandler, JSON_Success) &&
        CheckParserSetArrayItemHandler(parser, &ArrayItemHandler, JSON_Success) &&
        CheckParserSetInputEncoding(parser, settings.inputEncoding, JSON_Success) &&
        CheckParserSetStringEncoding(parser, settings.stringEncoding, JSON_Success) &&
        CheckParserSetMaxStringLength(parser, settings.maxStringLength, JSON_Success) &&
        CheckParserSetMaxNumberLength(parser, settings.maxNumberLength, JSON_Success) &&
        CheckParserSetAllowBOM(parser, settings.allowBOM, JSON_Success) &&
        CheckParserSetAllowComments(parser, settings.allowComments, JSON_Success) &&
        CheckParserSetAllowSpecialNumbers(parser, settings.allowSpecialNumbers, JSON_Success) &&
        CheckParserSetAllowHexNumbers(parser, settings.allowHexNumbers, JSON_Success) &&
        CheckParserSetReplaceInvalidEncodingSequences(parser, settings.replaceInvalidEncodingSequences, JSON_Success) &&
        CheckParserSetTrackObjectMembers(parser, settings.trackObjectMembers, JSON_Success))
    {
        JSON_Parser_Parse(parser, pTest->pInput, pTest->length, pTest->isFinal);
        state.error = JSON_Parser_GetError(parser);
        JSON_Parser_GetErrorLocation(parser, &state.errorLocation);
        if (state.error != JSON_Error_None)
        {
            OutputSeparator();
            OutputFormatted("!(%s):", errorNames[state.error]);
            OutputLocation(&state.errorLocation);
        }
        if (CheckParserState(parser, &state) && CheckOutput(pTest->pOutput))
        {
            printf("OK\n");
        }
        else
        {
            s_failureCount++;
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
    ResetOutput();
}

static void TestParserCreate()
{
    JSON_Parser parser = NULL;
    printf("Test creating parser ... ");
    if (CheckParserCreate(NULL, JSON_Success, &parser) &&
        CheckParserHasDefaultValues(parser))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserCreateWithCustomMemorySuite()
{
    JSON_Parser parser = NULL;
    printf("Test creating parser with custom memory suite ... ");
    if (CheckParserCreateWithCustomMemorySuite(NULL, NULL, JSON_Failure, &parser) &&
        CheckParserCreateWithCustomMemorySuite(&ReallocHandler, NULL, JSON_Failure, &parser) &
        CheckParserCreateWithCustomMemorySuite(NULL, &FreeHandler, JSON_Failure, &parser) &&
        CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &parser) &&
        CheckParserHasDefaultValues(parser))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserCreateMallocFailure()
{
    JSON_Parser parser = NULL;
    printf("Test creating parser malloc failure ... ");
    s_failMalloc = 1;
    if (CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Failure, &parser))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    s_failMalloc = 0;
    JSON_Parser_Free(parser);
}

static void TestParserSetSettings()
{
    JSON_Parser parser = NULL;
    ParserSettings settings;
    printf("Test setting parser settings ... ");
    InitParserSettings(&settings);
    settings.userData = (void*)1;
    settings.inputEncoding = JSON_UTF16LE;
    settings.stringEncoding = JSON_UTF16LE;
    settings.allowBOM = JSON_True;
    settings.allowComments = JSON_True;
    settings.allowSpecialNumbers = JSON_True;
    settings.allowHexNumbers = JSON_True;
    settings.replaceInvalidEncodingSequences = JSON_True;
    settings.trackObjectMembers = JSON_True;
    if (CheckParserCreate(NULL, JSON_Success, &parser) &&
        CheckParserSetUserData(parser, settings.userData, JSON_Success) &&
        CheckParserSetInputEncoding(parser, settings.inputEncoding, JSON_Success) &&
        CheckParserSetStringEncoding(parser, settings.stringEncoding, JSON_Success) &&
        CheckParserSetMaxStringLength(parser, settings.maxStringLength, JSON_Success) &&
        CheckParserSetMaxNumberLength(parser, settings.maxNumberLength, JSON_Success) &&
        CheckParserSetAllowBOM(parser, settings.allowBOM, JSON_Success) &&
        CheckParserSetAllowComments(parser, settings.allowComments, JSON_Success) &&
        CheckParserSetAllowSpecialNumbers(parser, settings.allowSpecialNumbers, JSON_Success) &&
        CheckParserSetAllowHexNumbers(parser, settings.allowHexNumbers, JSON_Success) &&
        CheckParserSetReplaceInvalidEncodingSequences(parser, settings.replaceInvalidEncodingSequences, JSON_Success) &&
        CheckParserSetTrackObjectMembers(parser, settings.trackObjectMembers, JSON_Success) &&
        CheckParserSettings(parser, &settings))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserSetInvalidSettings()
{
    JSON_Parser parser = NULL;
    ParserSettings settings;
    printf("Test setting invalid parser settings ... ");
    InitParserSettings(&settings);
    if (CheckParserCreate(NULL, JSON_Success, &parser) &&
        CheckParserSetInputEncoding(parser, (JSON_Encoding)-1, JSON_Failure) &&
        CheckParserSetInputEncoding(parser, (JSON_Encoding)(JSON_UTF32BE + 1), JSON_Failure) &&
        CheckParserSetStringEncoding(parser, (JSON_Encoding)-1, JSON_Failure) &&
        CheckParserSetStringEncoding(parser, JSON_UnknownEncoding, JSON_Failure) &&
        CheckParserSetStringEncoding(parser, (JSON_Encoding)(JSON_UTF32BE + 1), JSON_Failure) &&
        CheckParserParse(parser, NULL, 1, JSON_False, JSON_Failure) &&
        CheckParserSettings(parser, &settings))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserSetHandlers()
{
    JSON_Parser parser = NULL;
    ParserHandlers handlers;
    printf("Test setting parser handlers ... ");
    InitParserHandlers(&handlers);
    handlers.encodingDetectedHandler = &EncodingDetectedHandler;
    handlers.nullHandler = &NullHandler;
    handlers.booleanHandler = &BooleanHandler;
    handlers.stringHandler = &StringHandler;
    handlers.numberHandler = &NumberHandler;
    handlers.specialNumberHandler = &SpecialNumberHandler;
    handlers.startObjectHandler = &StartObjectHandler;
    handlers.endObjectHandler = &EndObjectHandler;
    handlers.objectMemberHandler = &ObjectMemberHandler;
    handlers.startArrayHandler = &StartArrayHandler;
    handlers.endArrayHandler = &EndArrayHandler;
    handlers.arrayItemHandler = &ArrayItemHandler;
    if (CheckParserCreate(NULL, JSON_Success, &parser) &&
        CheckParserSetEncodingDetectedHandler(parser, handlers.encodingDetectedHandler, JSON_Success) &&
        CheckParserSetNullHandler(parser, handlers.nullHandler, JSON_Success) &&
        CheckParserSetBooleanHandler(parser, handlers.booleanHandler, JSON_Success) &&
        CheckParserSetStringHandler(parser, handlers.stringHandler, JSON_Success) &&
        CheckParserSetNumberHandler(parser, handlers.numberHandler, JSON_Success) &&
        CheckParserSetSpecialNumberHandler(parser, handlers.specialNumberHandler, JSON_Success) &&
        CheckParserSetStartObjectHandler(parser, handlers.startObjectHandler, JSON_Success) &&
        CheckParserSetEndObjectHandler(parser, handlers.endObjectHandler, JSON_Success) &&
        CheckParserSetObjectMemberHandler(parser, handlers.objectMemberHandler, JSON_Success) &&
        CheckParserSetStartArrayHandler(parser, handlers.startArrayHandler, JSON_Success) &&
        CheckParserSetEndArrayHandler(parser, handlers.endArrayHandler, JSON_Success) &&
        CheckParserSetArrayItemHandler(parser, handlers.arrayItemHandler, JSON_Success) &&
        CheckParserHandlers(parser, &handlers))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserReset()
{
    JSON_Parser parser = NULL;
    ParserState state;
    ParserSettings settings;
    ParserHandlers handlers;
    printf("Test resetting parser ... ");
    InitParserState(&state);
    InitParserSettings(&settings);
    InitParserHandlers(&handlers);
    if (CheckParserCreate(NULL, JSON_Success, &parser) &&
        CheckParserSetUserData(parser, (void*)1, JSON_Success) &&
        CheckParserSetInputEncoding(parser, JSON_UTF16LE, JSON_Success) &&
        CheckParserSetStringEncoding(parser, JSON_UTF16LE, JSON_Success) &&
        CheckParserSetMaxStringLength(parser, 32, JSON_Success) &&
        CheckParserSetMaxNumberLength(parser, 32, JSON_Success) &&
        CheckParserSetAllowBOM(parser, JSON_True, JSON_Success) &&
        CheckParserSetAllowComments(parser, JSON_True, JSON_Success) &&
        CheckParserSetAllowSpecialNumbers(parser, JSON_True, JSON_Success) &&
        CheckParserSetAllowHexNumbers(parser, JSON_True, JSON_Success) &&
        CheckParserSetReplaceInvalidEncodingSequences(parser, JSON_True, JSON_Success) &&
        CheckParserSetTrackObjectMembers(parser, JSON_True, JSON_Success) &&
        CheckParserSetEncodingDetectedHandler(parser, &EncodingDetectedHandler, JSON_Success) &&
        CheckParserSetNullHandler(parser, &NullHandler, JSON_Success) &&
        CheckParserSetBooleanHandler(parser, &BooleanHandler, JSON_Success) &&
        CheckParserSetStringHandler(parser, &StringHandler, JSON_Success) &&
        CheckParserSetNumberHandler(parser, &NumberHandler, JSON_Success) &&
        CheckParserSetSpecialNumberHandler(parser, &SpecialNumberHandler, JSON_Success) &&
        CheckParserSetStartObjectHandler(parser, &StartObjectHandler, JSON_Success) &&
        CheckParserSetEndObjectHandler(parser, &EndObjectHandler, JSON_Success) &&
        CheckParserSetObjectMemberHandler(parser, &ObjectMemberHandler, JSON_Success) &&
        CheckParserSetStartArrayHandler(parser, &StartArrayHandler, JSON_Success) &&
        CheckParserSetEndArrayHandler(parser, &EndArrayHandler, JSON_Success) &&
        CheckParserSetArrayItemHandler(parser, &ArrayItemHandler, JSON_Success) &&
        CheckParserParse(parser, "7\x00", 2, JSON_True, JSON_Success) &&
        CheckParserReset(parser, JSON_Success) &&
        CheckParserState(parser, &state) &&
        CheckParserSettings(parser, &settings) &&
        CheckParserHandlers(parser, &handlers))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserMisbehaveInCallbacks()
{
    JSON_Parser parser = NULL;
    printf("Test parser misbehaving in callbacks ... ");
    s_misbehaveInHandler = 1;
    if (CheckParserCreate(NULL, JSON_Success, &parser) &&
        CheckParserSetEncodingDetectedHandler(parser, &EncodingDetectedHandler, JSON_Success) &&
        CheckParserParse(parser, "null", 4, JSON_True, JSON_Success) &&

        CheckParserCreate(NULL, JSON_Success, &parser) &&
        CheckParserSetNullHandler(parser, &NullHandler, JSON_Success) &&
        CheckParserParse(parser, "null", 4, JSON_True, JSON_Success) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetBooleanHandler(parser, &BooleanHandler, JSON_Success) &&
        CheckParserParse(parser, "true", 4, JSON_True, JSON_Success) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetStringHandler(parser, &StringHandler, JSON_Success) &&
        CheckParserParse(parser, "\"\"", 2, JSON_True, JSON_Success) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetNumberHandler(parser, &NumberHandler, JSON_Success) &&
        CheckParserParse(parser, "7", 1, JSON_True, JSON_Success) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetAllowSpecialNumbers(parser, JSON_True, JSON_Success) &&
        CheckParserSetSpecialNumberHandler(parser, &SpecialNumberHandler, JSON_Success) &&
        CheckParserParse(parser, "NaN", 3, JSON_True, JSON_Success) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetStartObjectHandler(parser, &StartObjectHandler, JSON_Success) &&
        CheckParserParse(parser, "{\"x\":0}", 7, JSON_True, JSON_Success) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetEndObjectHandler(parser, &EndObjectHandler, JSON_Success) &&
        CheckParserParse(parser, "{\"x\":0}", 7, JSON_True, JSON_Success) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetObjectMemberHandler(parser, &ObjectMemberHandler, JSON_Success) &&
        CheckParserParse(parser, "{\"x\":0}", 7, JSON_True, JSON_Success) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetStartArrayHandler(parser, &StartArrayHandler, JSON_Success) &&
        CheckParserParse(parser, "[0]", 3, JSON_True, JSON_Success) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetEndArrayHandler(parser, &EndArrayHandler, JSON_Success) &&
        CheckParserParse(parser, "[0]", 3, JSON_True, JSON_Success) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetArrayItemHandler(parser, &ArrayItemHandler, JSON_Success) &&
        CheckParserParse(parser, "[0]", 3, JSON_True, JSON_Success))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    s_misbehaveInHandler = 0;
    JSON_Parser_Free(parser);
}

static void TestParserAbortInCallbacks()
{
    JSON_Parser parser = NULL;
    ParserState state;
    printf("Test parser aborting in callbacks ... ");
    InitParserState(&state);
    state.error = JSON_Error_AbortedByHandler;
    state.errorLocation.byte = 0;
    state.errorLocation.line = 0;
    state.errorLocation.column = 0;
    state.errorLocation.depth = 0;
    state.inputEncoding = JSON_UTF8;
    s_failHandler = 1;
    if (CheckParserCreate(NULL, JSON_Success, &parser) &&
        CheckParserSetEncodingDetectedHandler(parser, &EncodingDetectedHandler, JSON_Success) &&
        CheckParserParse(parser, "    ", 4, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        !!(state.errorLocation.byte = 1) && /* hacky */
        !!(state.errorLocation.column = 1) &&

        CheckParserCreate(NULL, JSON_Success, &parser) &&
        CheckParserSetNullHandler(parser, &NullHandler, JSON_Success) &&
        CheckParserParse(parser, " null", 6, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetBooleanHandler(parser, &BooleanHandler, JSON_Success) &&
        CheckParserParse(parser, " true", 6, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetStringHandler(parser, &StringHandler, JSON_Success) &&
        CheckParserParse(parser, " \"\"", 3, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetNumberHandler(parser, &NumberHandler, JSON_Success) &&
        CheckParserParse(parser, " 7", 2, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetAllowSpecialNumbers(parser, JSON_True, JSON_Success) &&
        CheckParserSetSpecialNumberHandler(parser, &SpecialNumberHandler, JSON_Success) &&
        CheckParserParse(parser, " NaN", 4, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetStartObjectHandler(parser, &StartObjectHandler, JSON_Success) &&
        CheckParserParse(parser, " {}", 3, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetEndObjectHandler(parser, &EndObjectHandler, JSON_Success) &&
        CheckParserParse(parser, "{}", 2, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetStartArrayHandler(parser, &StartArrayHandler, JSON_Success) &&
        CheckParserParse(parser, " []", 3, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetEndArrayHandler(parser, &EndArrayHandler, JSON_Success) &&
        CheckParserParse(parser, "[]", 2, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        !!(state.errorLocation.depth = 1) && /* hacky! */

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetObjectMemberHandler(parser, &ObjectMemberHandler, JSON_Success) &&
        CheckParserParse(parser, "{\"x\":0}", 7, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state) &&

        CheckParserReset(parser, JSON_Success) &&
        CheckParserSetArrayItemHandler(parser, &ArrayItemHandler, JSON_Success) &&
        CheckParserParse(parser, "[0]", 3, JSON_True, JSON_Failure) &&
        CheckParserState(parser, &state))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    s_failHandler = 0;
    JSON_Parser_Free(parser);
}

static void TestParserStringMallocFailure()
{
    int succeeded = 0;
    JSON_Parser parser = NULL;
    ParserState state;
    printf("Test parser string malloc failure ... ");
    InitParserState(&state);
    state.error = JSON_Error_OutOfMemory;
    state.inputEncoding = JSON_UTF8;
    if (CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &parser) &&
        CheckParserParse(parser, "\"", 1, JSON_False, JSON_Success))
    {
        s_failMalloc = 1;
        for (;;)
        {
            if (JSON_Parser_Parse(parser, "a", 1, JSON_False) == JSON_Failure)
            {
                break;
            }
        }
        JSON_Parser_GetErrorLocation(parser, &state.errorLocation);
        if (CheckParserState(parser, &state))
        {
            succeeded = 1;
        }
        s_failMalloc = 0;
    }
    if (succeeded)
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserStringReallocFailure()
{
    int succeeded = 0;
    JSON_Parser parser = NULL;
    ParserState state;
    printf("Test parser string realloc failure ... ");
    InitParserState(&state);
    state.error = JSON_Error_OutOfMemory;
    state.inputEncoding = JSON_UTF8;
    if (CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &parser) &&
        CheckParserParse(parser, "\"", 1, JSON_False, JSON_Success))
    {
        s_failRealloc = 1;
        for (;;)
        {
            if (JSON_Parser_Parse(parser, "a", 1, JSON_False) == JSON_Failure)
            {
                break;
            }
        }
        JSON_Parser_GetErrorLocation(parser, &state.errorLocation);
        if (CheckParserState(parser, &state))
        {
            succeeded = 1;
        }
        s_failRealloc = 0;
    }
    if (succeeded)
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserStackMallocFailure()
{
    int succeeded = 0;
    JSON_Parser parser = NULL;
    ParserState state;
    printf("Test parser stack malloc failure ... ");
    InitParserState(&state);
    state.error = JSON_Error_OutOfMemory;
    state.inputEncoding = JSON_UTF8;
    if (CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &parser))
    {
        s_failMalloc = 1;
        for (;;)
        {
            if (JSON_Parser_Parse(parser, "{\"a\":", 5, JSON_False) == JSON_Failure)
            {
                break;
            }
        }
        JSON_Parser_GetErrorLocation(parser, &state.errorLocation);
        if (CheckParserState(parser, &state))
        {
            succeeded = 1;
        }
        s_failMalloc = 0;
    }
    if (succeeded)
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserStackReallocFailure()
{
    int succeeded = 0;
    JSON_Parser parser = NULL;
    ParserState state;
    printf("Test parser stack realloc failure ... ");
    InitParserState(&state);
    state.error = JSON_Error_OutOfMemory;
    state.inputEncoding = JSON_UTF8;
    if (CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &parser))
    {
        s_failRealloc = 1;
        for (;;)
        {
            if (JSON_Parser_Parse(parser, "{\"a\":", 5, JSON_False) == JSON_Failure)
            {
                break;
            }
        }
        JSON_Parser_GetErrorLocation(parser, &state.errorLocation);
        if (CheckParserState(parser, &state))
        {
            succeeded = 1;
        }
        s_failRealloc = 0;
    }
    if (succeeded)
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserDuplicateMemberTrackingMallocFailure()
{
    int succeeded = 0;
    JSON_Parser parser = NULL;
    ParserState state;
    printf("Test parser duplicate member tracking malloc failure ... ");
    InitParserState(&state);
    state.error = JSON_Error_OutOfMemory;
    state.inputEncoding = JSON_UTF8;
    if (CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &parser) &&
        CheckParserSetTrackObjectMembers(parser, JSON_True, JSON_Success))
    {
        s_failMalloc = 1;
        if (CheckParserParse(parser, "{\"a\":0}", 7, JSON_True, JSON_Failure) &&
            CheckParserState(parser, &state))
        {
            succeeded = 1;
        }
        s_failMalloc = 0;
    }
    if (succeeded)
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserMissing()
{
    ParserState state;
    ParserSettings settings;
    ParserHandlers handlers;
    JSON_Location errorLocation;
    printf("Test NULL parser instance ... ");
    InitParserState(&state);
    InitParserSettings(&settings);
    InitParserHandlers(&handlers);
    if (CheckParserState(NULL, &state) &&
        CheckParserSettings(NULL, &settings) &&
        CheckParserHandlers(NULL, &handlers) &&
        CheckParserFree(NULL, JSON_Failure) &&
        CheckParserReset(NULL, JSON_Failure) &&
        CheckParserSetUserData(NULL, (void*)1, JSON_Failure) &&
        CheckParserGetError(NULL, &errorLocation, JSON_Failure) &&
        CheckParserSetInputEncoding(NULL, JSON_UTF16LE, JSON_Failure) &&
        CheckParserSetStringEncoding(NULL, JSON_UTF16LE, JSON_Failure) &&
        CheckParserSetMaxStringLength(NULL, 128, JSON_Failure) &&
        CheckParserSetMaxNumberLength(NULL, 128, JSON_Failure) &&
        CheckParserSetEncodingDetectedHandler(NULL, &EncodingDetectedHandler, JSON_Failure) &&
        CheckParserSetNullHandler(NULL, &NullHandler, JSON_Failure) &&
        CheckParserSetBooleanHandler(NULL, &BooleanHandler, JSON_Failure) &&
        CheckParserSetStringHandler(NULL, &StringHandler, JSON_Failure) &&
        CheckParserSetNumberHandler(NULL, &NumberHandler, JSON_Failure) &&
        CheckParserSetSpecialNumberHandler(NULL, &SpecialNumberHandler, JSON_Failure) &&
        CheckParserSetStartObjectHandler(NULL, &StartObjectHandler, JSON_Failure) &&
        CheckParserSetEndObjectHandler(NULL, &EndObjectHandler, JSON_Failure) &&
        CheckParserSetObjectMemberHandler(NULL, &ObjectMemberHandler, JSON_Failure) &&
        CheckParserSetStartArrayHandler(NULL, &StartArrayHandler, JSON_Failure) &&
        CheckParserSetEndArrayHandler(NULL, &EndArrayHandler, JSON_Failure) &&
        CheckParserSetArrayItemHandler(NULL, &ArrayItemHandler, JSON_Failure) &&
        CheckParserParse(NULL, "7", 1, JSON_True, JSON_Failure))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
}

static void TestParserGetErrorLocationNullLocation()
{
    JSON_Parser parser = NULL;
    printf("Test parser get error location with NULL location ... ");
    if (CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &parser) &&
        CheckParserParse(parser, "!", 1, JSON_True, JSON_Failure) &&
        CheckParserGetError(parser, NULL, JSON_Failure))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserGetErrorLocationNoError()
{
    JSON_Parser parser = NULL;
    JSON_Location location = { 100, 200, 300, 400 };
    printf("Test parser get error location when no error occurred ... ");
    if (CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &parser) &&
        CheckParserParse(parser, "7", 1, JSON_True, JSON_Success) &&
        CheckParserGetError(parser, &location, JSON_Failure))
    {
        if (location.byte != 100 || location.line != 200 || location.column != 300 || location.depth != 400)
        {
            printf("FAILURE: JSON_Parser_GetErrorLocation() modified the location when it shouldn't have\n");
            s_failureCount++;
        }
        else
        {
            printf("OK\n");
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

static void TestParserGetTokenLocationOutsideHandler()
{
    JSON_Parser parser = NULL;
    JSON_Location location = { 100, 200, 300, 400 };
    printf("Test parser get token location when not in a parse handler  ... ");
    if (CheckParserCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &parser) &&
        CheckParserParse(parser, "7", 1, JSON_True, JSON_Success) &&
        CheckParserGetTokenLocation(parser, &location, JSON_Failure))
    {
        if (location.byte != 100 || location.line != 200 || location.column != 300 || location.depth != 400)
        {
            printf("FAILURE: JSON_Parser_GetTokenLocation() modified the location when it shouldn't have\n");
            s_failureCount++;
        }
        else
        {
            printf("OK\n");
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Parser_Free(parser);
}

#define PARSE_TEST(name, params, input, final, enc, output) { name, params, input, sizeof(input) - 1, final, JSON_##enc, output },

#define FINAL   JSON_True
#define PARTIAL JSON_False

static const ParseTest s_parseTests[] =
{

/* input encoding detection */

PARSE_TEST("infer input encoding from 0 bytes", Standard, "", FINAL, UnknownEncoding, "!(ExpectedMoreTokens):0,0,0,0")
PARSE_TEST("infer input encoding from 1 byte (1)", Standard, "7", FINAL, UTF8, "u(8) #(7):0,0,0,0")
PARSE_TEST("infer input encoding from 1 byte (2)", Standard, " ", FINAL, UTF8, "u(8) !(ExpectedMoreTokens):1,0,1,0")
PARSE_TEST("infer input encoding from 1 byte (3)", Standard, "\xFF", FINAL, UTF8, "u(8) !(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("infer input encoding from 2 bytes (1)", Standard, "{}", FINAL, UTF8, "u(8) {:0,0,0,0 }:1,0,1,0")
PARSE_TEST("infer input encoding from 2 bytes (2)", Standard, "7\x00", FINAL, UTF16LE, "u(16LE) #(7):0,0,0,0")
PARSE_TEST("infer input encoding from 2 bytes (3)", Standard, "\x00" "7", FINAL, UTF16BE, "u(16BE) #(7):0,0,0,0")
PARSE_TEST("infer input encoding from 2 bytes (4)", AllowBOM, "\xFF\xFE", FINAL, UTF16LE, "u(16LE) !(ExpectedMoreTokens):2,0,1,0")
PARSE_TEST("infer input encoding from 2 bytes (5)", AllowBOM, "\xFE\xFF", FINAL, UTF16BE, "u(16BE) !(ExpectedMoreTokens):2,0,1,0")
PARSE_TEST("infer input encoding from 2 bytes (6)", Standard, "\xFF\xFF", FINAL, UTF8, "u(8) !(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("infer input encoding from 3 bytes (1)", Standard, "{ }", FINAL, UTF8, "u(8) {:0,0,0,0 }:2,0,2,0")
PARSE_TEST("infer input encoding from 3 bytes (2)", AllowBOM, "\xEF\xBB\xBF", FINAL, UTF8, "u(8) !(ExpectedMoreTokens):3,0,1,0")
PARSE_TEST("infer input encoding from 3 bytes (3)", AllowBOM, "\xFF\xFF\xFF", FINAL, UTF8, "u(8) !(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("infer input encoding from 3 bytes (4)", Standard, "\xFF\xFF\xFF", FINAL, UTF8, "u(8) !(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("infer input encoding from 4 bytes (1)", Standard, "1234", FINAL, UTF8, "u(8) #(1234):0,0,0,0")
PARSE_TEST("infer input encoding from 4 bytes (2)", Standard, "   7", FINAL, UTF8, "u(8) #(7):3,0,3,0")
PARSE_TEST("infer input encoding from 4 bytes (3)", Standard, "\x00 \x00" "7", FINAL, UTF16BE, "u(16BE) #(7):2,0,1,0")
PARSE_TEST("infer input encoding from 4 bytes (4)", Standard, " \x00" "7\x00", FINAL, UTF16LE, "u(16LE) #(7):2,0,1,0")
PARSE_TEST("infer input encoding from 4 bytes (5)", Standard, "\x00\x00\x00" "7", FINAL, UTF32BE, "u(32BE) #(7):0,0,0,0")
PARSE_TEST("infer input encoding from 4 bytes (6)", Standard, "7\x00\x00\x00", FINAL, UTF32LE, "u(32LE) #(7):0,0,0,0")
PARSE_TEST("no input encoding starts <00 00 00 00>", Standard, "\x00\x00\x00\x00", FINAL, UnknownEncoding, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("no input encoding starts <nz 00 00 nz>", Standard, " \x00\x00 ", FINAL, UnknownEncoding, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 BOM not allowed", Standard, "\xEF\xBB\xBF" "7", PARTIAL, UTF8, "u(8) !(BOMNotAllowed):0,0,0,0")
PARSE_TEST("UTF-16LE BOM not allowed", Standard, "\xFF\xFE" "7\x00", PARTIAL, UTF16LE, "u(16LE) !(BOMNotAllowed):0,0,0,0")
PARSE_TEST("UTF-16BE BOM not allowed", Standard, "\xFE\xFF\x00" "7", PARTIAL, UTF16BE, "u(16BE) !(BOMNotAllowed):0,0,0,0")
PARSE_TEST("UTF-32LE BOM not allowed", Standard, "\xFF\xFE\x00\x00" "7\x00\x00\x00", PARTIAL, UTF32LE, "u(32LE) !(BOMNotAllowed):0,0,0,0")
PARSE_TEST("UTF-32BE BOM not allowed", Standard, "\x00\x00\xFE\xFF\x00\x00\x00" "7", PARTIAL, UTF32BE, "u(32BE) !(BOMNotAllowed):0,0,0,0")
PARSE_TEST("UTF-8 BOM allowed", AllowBOM, "\xEF\xBB\xBF" "7", FINAL, UTF8, "u(8) #(7):3,0,1,0")
PARSE_TEST("UTF-16LE BOM allowed", AllowBOM, "\xFF\xFE" "7\x00", FINAL, UTF16LE, "u(16LE) #(7):2,0,1,0")
PARSE_TEST("UTF-16BE BOM allowed", AllowBOM, "\xFE\xFF\x00" "7", FINAL, UTF16BE, "u(16BE) #(7):2,0,1,0")
PARSE_TEST("UTF-32LE BOM allowed", AllowBOM, "\xFF\xFE\x00\x00" "7\x00\x00\x00", FINAL, UTF32LE, "u(32LE) #(7):4,0,1,0")
PARSE_TEST("UTF-32BE BOM allowed", AllowBOM, "\x00\x00\xFE\xFF\x00\x00\x00" "7", FINAL, UTF32BE, "u(32BE) #(7):4,0,1,0")
PARSE_TEST("UTF-8 BOM allowed but no content", AllowBOM, "\xEF\xBB\xBF", FINAL, UTF8, "u(8) !(ExpectedMoreTokens):3,0,1,0")
PARSE_TEST("UTF-16LE BOM allowed but no content", AllowBOM, "\xFF\xFE", FINAL, UTF16LE, "u(16LE) !(ExpectedMoreTokens):2,0,1,0")
PARSE_TEST("UTF-16BE BOM allowed but no content", AllowBOM, "\xFE\xFF", FINAL, UTF16BE, "u(16BE) !(ExpectedMoreTokens):2,0,1,0")
PARSE_TEST("UTF-32LE BOM allowed but no content", AllowBOM, "\xFF\xFE\x00\x00", FINAL, UTF32LE, "u(32LE) !(ExpectedMoreTokens):4,0,1,0")
PARSE_TEST("UTF-32BE BOM allowed but no content", AllowBOM, "\x00\x00\xFE\xFF", FINAL, UTF32BE, "u(32BE) !(ExpectedMoreTokens):4,0,1,0")

/* invalid input encoding sequences */

PARSE_TEST("UTF-8 truncated sequence (1)", UTF8In, "\xC2", FINAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 truncated sequence (2)", UTF8In, "\xE0", FINAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 truncated sequence (3)", UTF8In, "\xE0\xBF", FINAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 truncated sequence (4)", UTF8In, "\xF0\xBF", FINAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 truncated sequence (5)", UTF8In, "\xF0\xBF\xBF", FINAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 overlong 2-byte sequence not allowed (1)", UTF8In, "\xC0", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 overlong 2-byte sequence not allowed (2)", UTF8In, "\xC1", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 overlong 3-byte sequence not allowed (1)", UTF8In, "\xE0\x80", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 overlong 3-byte sequence not allowed (2)", UTF8In, "\xE0\x9F", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 encoded surrogate not allowed (1)", UTF8In, "\xED\xA0", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 encoded surrogate not allowed (2)", UTF8In, "\xED\xBF", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 overlong 4-byte sequence not allowed (1)", UTF8In, "\xF0\x80", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 overlong 4-byte sequence not allowed (2)", UTF8In, "\xF0\x8F", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 encoded out-of-range codepoint not allowed (1)", UTF8In, "\xF4\x90", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid leading byte not allowed (1)", UTF8In, "\x80", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid leading byte not allowed (2)", UTF8In, "\xBF", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid leading byte not allowed (3)", UTF8In, "\xF5", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid leading byte not allowed (4)", UTF8In, "\xFF", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (1)", UTF8In, "\xC2\x7F", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (2)", UTF8In, "\xC2\xC0", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (3)", UTF8In, "\xE1\x7F", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (4)", UTF8In, "\xE1\xC0", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (5)", UTF8In, "\xE1\xBF\x7F", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (6)", UTF8In, "\xE1\xBF\xC0", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (7)", UTF8In, "\xF1\x7F", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (8)", UTF8In, "\xF1\xC0", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (9)", UTF8In, "\xF1\xBF\x7F", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (10)", UTF8In, "\xF1\xBF\xC0", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (11)", UTF8In, "\xF1\xBF\xBF\x7F", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-8 invalid continuation byte not allowed (12)", UTF8In, "\xF1\xBF\xBF\xC0", PARTIAL, UTF8, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16LE truncated sequence", UTF16LEIn | UTF16LEOut, " ", FINAL, UTF16LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16LE standalone trailing surrogate not allowed (1)", UTF16LEIn | UTF16LEOut, "\x00\xDC", PARTIAL, UTF16LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16LE standalone trailing surrogate not allowed (2)", UTF16LEIn | UTF16LEOut, "\xFF\xDF", PARTIAL, UTF16LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16LE standalone leading surrogate not allowed (1)", UTF16LEIn | UTF16LEOut, "\x00\xD8\x00_", PARTIAL, UTF16LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16LE standalone leading surrogate not allowed (2)", UTF16LEIn | UTF16LEOut, "\xFF\xDB\x00_", PARTIAL, UTF16LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16LE standalone leading surrogate not allowed (3)", UTF16LEIn | UTF16LEOut, "\xFF\xDB_", FINAL, UTF16LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16BE truncated sequence", UTF16BEIn | UTF16BEOut, "\x00", FINAL, UTF16BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16BE standalone trailing surrogate not allowed (1)", UTF16BEIn | UTF16BEOut, "\xDC\x00", PARTIAL, UTF16BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16BE standalone trailing surrogate not allowed (2)", UTF16BEIn | UTF16BEOut, "\xDF\xFF", PARTIAL, UTF16BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16BE standalone leading surrogate not allowed (1)", UTF16BEIn | UTF16BEOut, "\xD8\x00\x00_", PARTIAL, UTF16BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16BE standalone leading surrogate not allowed (2)", UTF16BEIn | UTF16BEOut, "\xDB\xFF\x00_", PARTIAL, UTF16BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-16BE standalone leading surrogate not allowed (3)", UTF16BEIn | UTF16BEOut, "\xDB\xFF", FINAL, UTF16BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32LE truncated sequence (1)", UTF32LEIn | UTF32LEOut, " ", FINAL, UTF32LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32LE truncated sequence (2)", UTF32LEIn | UTF32LEOut, " \x00", FINAL, UTF32LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32LE truncated sequence (3)", UTF32LEIn | UTF32LEOut, " \x00\x00", FINAL, UTF32LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32LE encoded surrogate not allowed (1)", UTF32LEIn | UTF32LEOut, "\x00\xD8\x00\x00", PARTIAL, UTF32LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32LE encoded surrogate not allowed (2)", UTF32LEIn | UTF32LEOut, "\x00\xDF\x00\x00", PARTIAL, UTF32LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32LE encoded out-of-range codepoint not allowed (1)", UTF32LEIn | UTF32LEOut, "\x00\x00\x11\x00", PARTIAL, UTF32LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32LE encoded out-of-range codepoint not allowed (2)", UTF32LEIn | UTF32LEOut, "\xFF\xFF\xFF\xFF", PARTIAL, UTF32LE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32BE truncated sequence (1)", UTF32BEIn | UTF32BEOut, "\x00", FINAL, UTF32BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32BE truncated sequence (2)", UTF32BEIn | UTF32BEOut, "\x00\x00", FINAL, UTF32BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32BE truncated sequence (3)", UTF32BEIn | UTF32BEOut, "\x00\x00\x00", FINAL, UTF32BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32BE encoded surrogate not allowed (1)", UTF32BEIn | UTF32BEOut, "\x00\x00\xD8\x00", PARTIAL, UTF32BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32BE encoded surrogate not allowed (2)", UTF32BEIn | UTF32BEOut, "\x00\x00\xDF\xFF", PARTIAL, UTF32BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32BE encoded out-of-range codepoint not allowed (1)", UTF32BEIn | UTF32BEOut, "\x00\x11\x00\x00", PARTIAL, UTF32BE, "!(InvalidEncodingSequence):0,0,0,0")
PARSE_TEST("UTF-32BE encoded out-of-range codepoint not allowed (2)", UTF32BEIn | UTF32BEOut, "\xFF\xFF\xFF\xFF", PARTIAL, UTF32BE, "!(InvalidEncodingSequence):0,0,0,0")

/* replace invalid input encoding sequences */

PARSE_TEST("replace UTF-8 truncated 2-byte sequence (1)", ReplaceInvalidEncodingSequences, "\"abc\xC2\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 truncated 2-byte sequence (2)", ReplaceInvalidEncodingSequences, "\"abc\xC2\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):6,0,6,0")
PARSE_TEST("replace UTF-8 truncated 3-byte sequence (1)", ReplaceInvalidEncodingSequences, "\"abc\xE0\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 truncated 3-byte sequence (2)", ReplaceInvalidEncodingSequences, "\"abc\xE0\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):6,0,6,0")
PARSE_TEST("replace UTF-8 truncated 3-byte sequence (3)", ReplaceInvalidEncodingSequences, "\"abc\xE0\xBF\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 truncated 3-byte sequence (4)", ReplaceInvalidEncodingSequences, "\"abc\xE0\xBF\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):7,0,6,0")
PARSE_TEST("replace UTF-8 truncated 4-byte sequence (1)", ReplaceInvalidEncodingSequences, "\"abc\xF0\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 truncated 4-byte sequence (2)", ReplaceInvalidEncodingSequences, "\"abc\xF0\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):6,0,6,0")
PARSE_TEST("replace UTF-8 truncated 4-byte sequence (3)", ReplaceInvalidEncodingSequences, "\"abc\xF0\xBF\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 truncated 4-byte sequence (4)", ReplaceInvalidEncodingSequences, "\"abc\xF0\xBF\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):7,0,6,0")
PARSE_TEST("replace UTF-8 truncated 4-byte sequence (5)", ReplaceInvalidEncodingSequences, "\"abc\xF0\xBF\xBF\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 truncated 4-byte sequence (6)", ReplaceInvalidEncodingSequences, "\"abc\xF0\xBF\xBF\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):8,0,6,0")
PARSE_TEST("replace UTF-8 overlong 2-byte sequence (1)", ReplaceInvalidEncodingSequences, "\"abc\xC0\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 overlong 2-byte sequence (2)", ReplaceInvalidEncodingSequences, "\"abc\xC0\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):6,0,6,0")
PARSE_TEST("replace UTF-8 overlong 2-byte sequence (3)", ReplaceInvalidEncodingSequences, "\"abc\xC1\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 overlong 2-byte sequence (4)", ReplaceInvalidEncodingSequences, "\"abc\xC1\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):6,0,6,0")
PARSE_TEST("replace UTF-8 overlong 3-byte sequence (1)", ReplaceInvalidEncodingSequences, "\"abc\xE0\x80\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 overlong 3-byte sequence (2)", ReplaceInvalidEncodingSequences, "\"abc\xE0\x80\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 overlong 3-byte sequence (3)", ReplaceInvalidEncodingSequences, "\"abc\xE0\x9F\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 overlong 3-byte sequence (4)", ReplaceInvalidEncodingSequences, "\"abc\xE0\x9F\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 encoded surrogate (1)", ReplaceInvalidEncodingSequences, "\"abc\xED\xA0\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 encoded surrogate (2)", ReplaceInvalidEncodingSequences, "\"abc\xED\xA0\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 encoded surrogate (3)", ReplaceInvalidEncodingSequences, "\"abc\xED\xBF\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 encoded surrogate (4)", ReplaceInvalidEncodingSequences, "\"abc\xED\xBF\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 overlong 4-byte sequence (1)", ReplaceInvalidEncodingSequences, "\"abc\xF0\x80\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 overlong 4-byte sequence (2)", ReplaceInvalidEncodingSequences, "\"abc\xF0\x80\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 overlong 4-byte sequence (3)", ReplaceInvalidEncodingSequences, "\"abc\xF0\x8F\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 overlong 4-byte sequence (4)", ReplaceInvalidEncodingSequences, "\"abc\xF0\x8F\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 encoded out-of-range codepoint (1)", ReplaceInvalidEncodingSequences, "\"abc\xF4\x90\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 encoded out-of-range codepoint (2)", ReplaceInvalidEncodingSequences, "\"abc\xF4\x90\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 invalid leading byte (1)", ReplaceInvalidEncodingSequences, "\"abc\x80\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid leading byte (2)", ReplaceInvalidEncodingSequences, "\"abc\x80\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):6,0,6,0")
PARSE_TEST("replace UTF-8 invalid leading byte (3)", ReplaceInvalidEncodingSequences, "\"abc\xBF\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid leading byte (4)", ReplaceInvalidEncodingSequences, "\"abc\xBF\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):6,0,6,0")
PARSE_TEST("replace UTF-8 invalid leading byte (5)", ReplaceInvalidEncodingSequences, "\"abc\xF5\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid leading byte (6)", ReplaceInvalidEncodingSequences, "\"abc\xF5\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):6,0,6,0")
PARSE_TEST("replace UTF-8 invalid leading byte (7)", ReplaceInvalidEncodingSequences, "\"abc\xFF\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid leading byte (8)", ReplaceInvalidEncodingSequences, "\"abc\xFF\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD):0,0,0,0 !(UnknownToken):6,0,6,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (1)", ReplaceInvalidEncodingSequences, "\"abc\xC2\x7F\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (2)", ReplaceInvalidEncodingSequences, "\"abc\xC2\x7F\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (3)", ReplaceInvalidEncodingSequences, "\"abc\xC2\xC0\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (4)", ReplaceInvalidEncodingSequences, "\"abc\xC2\xC0\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (5)", ReplaceInvalidEncodingSequences, "\"abc\xE1\x7F\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (6)", ReplaceInvalidEncodingSequences, "\"abc\xE1\x7F\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (7)", ReplaceInvalidEncodingSequences, "\"abc\xE1\xC0\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (8)", ReplaceInvalidEncodingSequences, "\"abc\xE1\xC0\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (9)", ReplaceInvalidEncodingSequences, "\"abc\xE1\xBF\x7F\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (10)", ReplaceInvalidEncodingSequences, "\"abc\xE1\xBF\x7F\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0 !(UnknownToken):8,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (11)", ReplaceInvalidEncodingSequences, "\"abc\xE1\xBF\xC0\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (12)", ReplaceInvalidEncodingSequences, "\"abc\xE1\xBF\xC0\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):8,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (13)", ReplaceInvalidEncodingSequences, "\"abc\xF1\x7F\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (14)", ReplaceInvalidEncodingSequences, "\"abc\xF1\x7F\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (15)", ReplaceInvalidEncodingSequences, "\"abc\xF1\xC0\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (16)", ReplaceInvalidEncodingSequences, "\"abc\xF1\xC0\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):7,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (17)", ReplaceInvalidEncodingSequences, "\"abc\xF1\xBF\x7F\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (18)", ReplaceInvalidEncodingSequences, "\"abc\xF1\xBF\x7F\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0 !(UnknownToken):8,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (19)", ReplaceInvalidEncodingSequences, "\"abc\xF1\xBF\xC0\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (20)", ReplaceInvalidEncodingSequences, "\"abc\xF1\xBF\xC0\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):8,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (21)", ReplaceInvalidEncodingSequences, "\"abc\xF1\xBF\xBF\x7F\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (22)", ReplaceInvalidEncodingSequences, "\"abc\xF1\xBF\xBF\x7F\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD 7F):0,0,0,0 !(UnknownToken):9,0,7,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (23)", ReplaceInvalidEncodingSequences, "\"abc\xF1\xBF\xBF\xC0\"", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-8 invalid continuation byte (24)", ReplaceInvalidEncodingSequences, "\"abc\xF1\xBF\xBF\xC0\"!", FINAL, UTF8, "u(8) s(ar 61 62 63 EF BF BD EF BF BD):0,0,0,0 !(UnknownToken):9,0,7,0")
PARSE_TEST("Unicode 5.2.0 replacement example (1)", ReplaceInvalidEncodingSequences, "   \"\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64\"", FINAL, UTF8, "u(8) s(ar 61 EF BF BD EF BF BD EF BF BD 62 EF BF BD 63 EF BF BD EF BF BD 64):3,0,3,0")
PARSE_TEST("Unicode 5.2.0 replacement example (2)", ReplaceInvalidEncodingSequences, "   \"\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64\"!", FINAL, UTF8, "u(8) s(ar 61 EF BF BD EF BF BD EF BF BD 62 EF BF BD 63 EF BF BD EF BF BD 64):3,0,3,0 !(UnknownToken):18,0,15,0")
PARSE_TEST("replace UTF-16LE standalone trailing surrogate (1)", ReplaceInvalidEncodingSequences, "\"\x00" "_\x00" "\x00\xDC" "\"\x00", FINAL, UTF16LE, "u(16LE) s(ar 5F EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-16LE standalone trailing surrogate (2)", ReplaceInvalidEncodingSequences, "\"\x00" "_\x00" "\x00\xDC" "\"\x00" "!\x00", FINAL, UTF16LE, "u(16LE) s(ar 5F EF BF BD):0,0,0,0 !(UnknownToken):8,0,4,0")
PARSE_TEST("replace UTF-16LE standalone trailing surrogate (3)", ReplaceInvalidEncodingSequences, "\"\x00" "_\x00" "\xFF\xDF" "\"\x00", FINAL, UTF16LE, "u(16LE) s(ar 5F EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-16LE standalone trailing surrogate (4)", ReplaceInvalidEncodingSequences, "\"\x00" "_\x00" "\xFF\xDF" "\"\x00" "!\x00", FINAL, UTF16LE, "u(16LE) s(ar 5F EF BF BD):0,0,0,0 !(UnknownToken):8,0,4,0")
PARSE_TEST("replace UTF-16LE standalone leading surrogate (1)", ReplaceInvalidEncodingSequences,  "\"\x00" "_\x00" "\x00\xD8" "_\x00" "\"\x00", FINAL, UTF16LE, "u(16LE) s(ar 5F EF BF BD 5F):0,0,0,0")
PARSE_TEST("replace UTF-16LE standalone leading surrogate (2)", ReplaceInvalidEncodingSequences,  "\"\x00" "_\x00" "\x00\xD8" "_\x00" "\"\x00" "!\x00", FINAL, UTF16LE, "u(16LE) s(ar 5F EF BF BD 5F):0,0,0,0 !(UnknownToken):10,0,5,0")
PARSE_TEST("replace UTF-16LE standalone leading surrogate (3)", ReplaceInvalidEncodingSequences,  "\"\x00" "_\x00" "\xFF\xDB" "_\x00" "\"\x00", FINAL, UTF16LE, "u(16LE) s(ar 5F EF BF BD 5F):0,0,0,0")
PARSE_TEST("replace UTF-16LE standalone leading surrogate (4)", ReplaceInvalidEncodingSequences,  "\"\x00" "_\x00" "\xFF\xDB" "_\x00" "\"\x00" "!\x00", FINAL, UTF16LE, "u(16LE) s(ar 5F EF BF BD 5F):0,0,0,0 !(UnknownToken):10,0,5,0")
PARSE_TEST("replace UTF-16BE standalone trailing surrogate (1)", ReplaceInvalidEncodingSequences, "\x00\"" "\x00_" "\xDC\x00" "\x00\"", FINAL, UTF16BE, "u(16BE) s(ar 5F EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-16BE standalone trailing surrogate (2)", ReplaceInvalidEncodingSequences, "\x00\"" "\x00_" "\xDC\x00" "\x00\"" "\x00!", FINAL, UTF16BE, "u(16BE) s(ar 5F EF BF BD):0,0,0,0 !(UnknownToken):8,0,4,0")
PARSE_TEST("replace UTF-16BE standalone trailing surrogate (3)", ReplaceInvalidEncodingSequences, "\x00\"" "\x00_" "\xDF\xFF" "\x00\"", FINAL, UTF16BE, "u(16BE) s(ar 5F EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-16BE standalone trailing surrogate (4)", ReplaceInvalidEncodingSequences, "\x00\"" "\x00_" "\xDF\xFF" "\x00\"" "\x00!", FINAL, UTF16BE, "u(16BE) s(ar 5F EF BF BD):0,0,0,0 !(UnknownToken):8,0,4,0")
PARSE_TEST("replace UTF-16BE standalone leading surrogate (1)", ReplaceInvalidEncodingSequences,  "\x00\"" "\x00_" "\xD8\x00" "\x00_" "\x00\"", FINAL, UTF16BE, "u(16BE) s(ar 5F EF BF BD 5F):0,0,0,0")
PARSE_TEST("replace UTF-16BE standalone leading surrogate (2)", ReplaceInvalidEncodingSequences,  "\x00\"" "\x00_" "\xD8\x00" "\x00_" "\x00\"" "!\x00", FINAL, UTF16BE, "u(16BE) s(ar 5F EF BF BD 5F):0,0,0,0 !(UnknownToken):10,0,5,0")
PARSE_TEST("replace UTF-16BE standalone leading surrogate (3)", ReplaceInvalidEncodingSequences,  "\x00\"" "\x00_" "\xDB\xFF" "\x00_" "\x00\"", FINAL, UTF16BE, "u(16BE) s(ar 5F EF BF BD 5F):0,0,0,0")
PARSE_TEST("replace UTF-16BE standalone leading surrogate (4)", ReplaceInvalidEncodingSequences,  "\x00\"" "\x00_" "\xDB\xFF" "\x00_" "\x00\"" "!\x00", FINAL, UTF16BE, "u(16BE) s(ar 5F EF BF BD 5F):0,0,0,0 !(UnknownToken):10,0,5,0")
PARSE_TEST("replace UTF-32LE encoded surrogate (1)", ReplaceInvalidEncodingSequences, "\"\x00\x00\x00" "\x00\xD8\x00\x00" "\"\x00\x00\x00", FINAL, UTF32LE, "u(32LE) s(ar EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-32LE encoded surrogate (2)", ReplaceInvalidEncodingSequences, "\"\x00\x00\x00" "\x00\xD8\x00\x00" "\"\x00\x00\x00" "!\x00\x00\x00", FINAL, UTF32LE, "u(32LE) s(ar EF BF BD):0,0,0,0 !(UnknownToken):12,0,3,0")
PARSE_TEST("replace UTF-32LE encoded surrogate (3)", ReplaceInvalidEncodingSequences, "\"\x00\x00\x00" "\xFF\xDF\x00\x00" "\"\x00\x00\x00", FINAL, UTF32LE, "u(32LE) s(ar EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-32LE encoded surrogate (4)", ReplaceInvalidEncodingSequences, "\"\x00\x00\x00" "\xFF\xDF\x00\x00" "\"\x00\x00\x00" "!\x00\x00\x00", FINAL, UTF32LE, "u(32LE) s(ar EF BF BD):0,0,0,0 !(UnknownToken):12,0,3,0")
PARSE_TEST("replace UTF-32LE encoded out-of-range codepoint (1)", ReplaceInvalidEncodingSequences, "\"\x00\x00\x00" "\x00\x00\x11\x00" "\"\x00\x00\x00", FINAL, UTF32LE, "u(32LE) s(ar EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-32LE encoded out-of-range codepoint (2)", ReplaceInvalidEncodingSequences, "\"\x00\x00\x00" "\x00\x00\x11\x00" "\"\x00\x00\x00" "!\x00\x00\x00", FINAL, UTF32LE, "u(32LE) s(ar EF BF BD):0,0,0,0 !(UnknownToken):12,0,3,0")
PARSE_TEST("replace UTF-32LE encoded out-of-range codepoint (3)", ReplaceInvalidEncodingSequences, "\"\x00\x00\x00" "\x00\x00\x00\x01" "\"\x00\x00\x00", FINAL, UTF32LE, "u(32LE) s(ar EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-32LE encoded out-of-range codepoint (4)", ReplaceInvalidEncodingSequences, "\"\x00\x00\x00" "\x00\x00\x00\x01" "\"\x00\x00\x00" "!\x00\x00\x00", FINAL, UTF32LE, "u(32LE) s(ar EF BF BD):0,0,0,0 !(UnknownToken):12,0,3,0")
PARSE_TEST("replace UTF-32BE encoded surrogate (1)", ReplaceInvalidEncodingSequences, "\x00\x00\x00\"" "\x00\x00\xD8\x00" "\x00\x00\x00\"", FINAL, UTF32BE, "u(32BE) s(ar EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-32BE encoded surrogate (2)", ReplaceInvalidEncodingSequences, "\x00\x00\x00\"" "\x00\x00\xD8\x00" "\x00\x00\x00\"" "\x00\x00\x00!", FINAL, UTF32BE, "u(32BE) s(ar EF BF BD):0,0,0,0 !(UnknownToken):12,0,3,0")
PARSE_TEST("replace UTF-32BE encoded surrogate (3)", ReplaceInvalidEncodingSequences, "\x00\x00\x00\"" "\x00\x00\xDF\xFF" "\x00\x00\x00\"", FINAL, UTF32BE, "u(32BE) s(ar EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-32BE encoded surrogate (4)", ReplaceInvalidEncodingSequences, "\x00\x00\x00\"" "\x00\x00\xDF\xFF" "\x00\x00\x00\"" "\x00\x00\x00!", FINAL, UTF32BE, "u(32BE) s(ar EF BF BD):0,0,0,0 !(UnknownToken):12,0,3,0")
PARSE_TEST("replace UTF-32BE encoded out-of-range codepoint (1)", ReplaceInvalidEncodingSequences, "\x00\x00\x00\"" "\x00\x11\x00\x00" "\x00\x00\x00\"", FINAL, UTF32BE, "u(32BE) s(ar EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-32BE encoded out-of-range codepoint (2)", ReplaceInvalidEncodingSequences, "\x00\x00\x00\"" "\x00\x11\x00\x00" "\x00\x00\x00\"" "\x00\x00\x00!", FINAL, UTF32BE, "u(32BE) s(ar EF BF BD):0,0,0,0 !(UnknownToken):12,0,3,0")
PARSE_TEST("replace UTF-32BE encoded out-of-range codepoint (3)", ReplaceInvalidEncodingSequences, "\x00\x00\x00\"" "\x01\x00\x00\x00" "\x00\x00\x00\"", FINAL, UTF32BE, "u(32BE) s(ar EF BF BD):0,0,0,0")
PARSE_TEST("replace UTF-32BE encoded out-of-range codepoint (4)", ReplaceInvalidEncodingSequences, "\x00\x00\x00\"" "\x01\x00\x00\x00" "\x00\x00\x00\"" "\x00\x00\x00!", FINAL, UTF32BE, "u(32BE) s(ar EF BF BD):0,0,0,0 !(UnknownToken):12,0,3,0")

/* general */

PARSE_TEST("no input bytes (partial)", Standard, "", PARTIAL, UnknownEncoding, "")
PARSE_TEST("no input bytes", Standard, "", FINAL, UnknownEncoding, "!(ExpectedMoreTokens):0,0,0,0")
PARSE_TEST("all whitespace (partial) (1)", Standard, " ", PARTIAL, UnknownEncoding, "")
PARSE_TEST("all whitespace (partial) (2)", Standard, "\t", PARTIAL, UnknownEncoding, "")
PARSE_TEST("all whitespace (partial) (3)", Standard, "\r\n", PARTIAL, UnknownEncoding, "")
PARSE_TEST("all whitespace (partial) (4)", Standard, "\r\n\n\r ", PARTIAL, UTF8, "u(8)")
PARSE_TEST("all whitespace (1)", Standard, " ", FINAL, UTF8, "u(8) !(ExpectedMoreTokens):1,0,1,0")
PARSE_TEST("all whitespace (2)", Standard, "\t", FINAL, UTF8, "u(8) !(ExpectedMoreTokens):1,0,1,0")
PARSE_TEST("all whitespace (3)", Standard, "\r\n", FINAL, UTF8, "u(8) !(ExpectedMoreTokens):2,1,0,0")
PARSE_TEST("all whitespace (4)", Standard, "\r\n\n\r ", FINAL, UTF8, "u(8) !(ExpectedMoreTokens):5,3,1,0")
PARSE_TEST("trailing garbage (1)", Standard, "7 !", FINAL, UTF8, "u(8) #(7):0,0,0,0 !(UnknownToken):2,0,2,0")
PARSE_TEST("trailing garbage (2)", Standard, "7 {", FINAL, UTF8, "u(8) #(7):0,0,0,0 !(UnexpectedToken):2,0,2,0")
PARSE_TEST("trailing garbage (3)", Standard, "7 \xC0", FINAL, UTF8, "u(8) #(7):0,0,0,0 !(InvalidEncodingSequence):2,0,2,0")
PARSE_TEST("trailing garbage (4)", Standard, "7 \xC2", FINAL, UTF8, "u(8) #(7):0,0,0,0 !(InvalidEncodingSequence):2,0,2,0")
PARSE_TEST("trailing garbage (5)", Standard, "7 [", FINAL, UTF8, "u(8) #(7):0,0,0,0 !(UnexpectedToken):2,0,2,0")
PARSE_TEST("trailing garbage (6)", Standard, "7 ,", FINAL, UTF8, "u(8) #(7):0,0,0,0 !(UnexpectedToken):2,0,2,0")
PARSE_TEST("trailing garbage (7)", Standard, "7 8", FINAL, UTF8, "u(8) #(7):0,0,0,0 !(UnexpectedToken):2,0,2,0")
PARSE_TEST("trailing garbage (8)", Standard, "7 \"", FINAL, UTF8, "u(8) #(7):0,0,0,0 !(IncompleteToken):2,0,2,0")

/* null */

PARSE_TEST("null (1)", Standard, "null", FINAL, UTF8, "u(8) n:0,0,0,0")
PARSE_TEST("null (2)", Standard, " null ", FINAL, UTF8, "u(8) n:1,0,1,0")
PARSE_TEST("n is not a literal", Standard, "n ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("nu is not a literal", Standard, "nu ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("nul is not a literal", Standard, "nul ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("nullx is not a literal", Standard, "nullx", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("null0 is not a literal", Standard, "null0", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("null_ is not a literal", Standard, "null_", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("nullX is not a literal", Standard, "nullX", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("NULL is not a literal", Standard, "NULL", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("null truncated after n", Standard, "n", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("null truncated after nu", Standard, "nu", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("null truncated after nul", Standard, "nul", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")

/* true */

PARSE_TEST("true (1)", Standard, "true", FINAL, UTF8, "u(8) t:0,0,0,0")
PARSE_TEST("true (2)", Standard, " true ", FINAL, UTF8, "u(8) t:1,0,1,0")
PARSE_TEST("t is not a literal", Standard, "t ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("tr is not a literal", Standard, "tr ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("tru is not a literal", Standard, "tru ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("trux is not a literal", Standard, "trux", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("true0 is not a literal", Standard, "true0", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("true_ is not a literal", Standard, "true__", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("trueX is not a literal", Standard, "trueX", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("TRUE is not a literal", Standard, "TRUE", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("true truncated after t", Standard, "t", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("true truncated after tr", Standard, "tr", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("true truncated after tru", Standard, "tru", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")

/* false */

PARSE_TEST("false (1)", Standard, "false", FINAL, UTF8, "u(8) f:0,0,0,0")
PARSE_TEST("false (2)", Standard, " false ", FINAL, UTF8, "u(8) f:1,0,1,0")
PARSE_TEST("f is not a literal", Standard, "f ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("fa is not a literal", Standard, "fa ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("fal is not a literal", Standard, "fal ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("falx is not a literal", Standard, "falx", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("fals is not a literal", Standard, "fals", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("false0 is not a literal", Standard, "false0", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("false_ is not a literal", Standard, "false_", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("falseX is not a literal", Standard, "falseX", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("FALSE is not a literal", Standard, "FALSE", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("false truncated after f", Standard, "f", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("false truncated after fa", Standard, "fa", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("false truncated after fal", Standard, "fal", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("false truncated after fals", Standard, "fals", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")

/* NaN */

PARSE_TEST("NaN (1)", AllowSpecialNumbers, "NaN", FINAL, UTF8, "u(8) #(NaN):0,0,0,0")
PARSE_TEST("NaN (2)", AllowSpecialNumbers, " NaN ", FINAL, UTF8, "u(8) #(NaN):1,0,1,0")
PARSE_TEST("N is not a literal", AllowSpecialNumbers, "N ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Na is not a literal", AllowSpecialNumbers, "Na ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Nax is not a literal", AllowSpecialNumbers, "Nax", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Na0 is not a literal", AllowSpecialNumbers, "NaN0", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("NaN_ is not a literal", AllowSpecialNumbers, "NaN_", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("NaNX is not a literal", AllowSpecialNumbers, "NaNX", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("NAN is not a literal", AllowSpecialNumbers, "NAN", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("NaN truncated after N", AllowSpecialNumbers, "N", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("NaN truncated after Na", AllowSpecialNumbers, "Na", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("NaN not allowed", Standard, "NaN", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")

/* Infinity */

PARSE_TEST("Infinity (1)", AllowSpecialNumbers, "Infinity", FINAL, UTF8, "u(8) #(Infinity):0,0,0,0")
PARSE_TEST("Infinity (2)", AllowSpecialNumbers, " Infinity ", FINAL, UTF8, "u(8) #(Infinity):1,0,1,0")
PARSE_TEST("I is not a literal", AllowSpecialNumbers, "I ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("In is not a literal", AllowSpecialNumbers, "In ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Inf is not a literal", AllowSpecialNumbers, "Inf ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infi is not a literal", AllowSpecialNumbers, "Infi ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infin is not a literal", AllowSpecialNumbers, "Infin ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infini is not a literal", AllowSpecialNumbers, "Infini ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinit is not a literal", AllowSpecialNumbers, "Infinit ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinitx is not a literal", AllowSpecialNumbers, "Infinitx", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinit0 is not a literal", AllowSpecialNumbers, "Infinit0", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinity_ is not a literal", AllowSpecialNumbers, "Infinity_", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("InfinityX is not a literal", AllowSpecialNumbers, "InfinityX", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("INF is not a literal", AllowSpecialNumbers, "INF", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("INFINITY is not a literal", AllowSpecialNumbers, "INFINITY", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinity truncated after I", AllowSpecialNumbers, "I", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinity truncated after In", AllowSpecialNumbers, "In", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinity truncated after Inf", AllowSpecialNumbers, "Inf", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinity truncated after Infi", AllowSpecialNumbers, "Infi", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinity truncated after Infin", AllowSpecialNumbers, "Infin", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinity truncated after Infini", AllowSpecialNumbers, "Infini", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinity truncated after Infinit", AllowSpecialNumbers, "Infinit", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("Infinity not allowed", Standard, "Infinity", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")

/* -Infinity */

PARSE_TEST("-Infinity (1)", AllowSpecialNumbers, "-Infinity", FINAL, UTF8, "u(8) #(-Infinity):0,0,0,0")
PARSE_TEST("-Infinity (2)", AllowSpecialNumbers, " -Infinity ", FINAL, UTF8, "u(8) #(-Infinity):1,0,1,0")
PARSE_TEST("-I is not a number", AllowSpecialNumbers, "-I ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-In is not a number", AllowSpecialNumbers, "-In ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Inf is not a number", AllowSpecialNumbers, "-Inf ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infi is not a number", AllowSpecialNumbers, "-Infi ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infin is not a number", AllowSpecialNumbers, "-Infin ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infini is not a number", AllowSpecialNumbers, "-Infini ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinit is not a number", AllowSpecialNumbers, "-Infinit ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinitx is not a number", AllowSpecialNumbers, "-Infinitx", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinit0 is not a number", AllowSpecialNumbers, "-Infinit0", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinity_ is not a number", AllowSpecialNumbers, "-Infinity_", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-InfinityX is not a number", AllowSpecialNumbers, "-InfinityX", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-INF is not a number", AllowSpecialNumbers, "-INF", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-INFINITY is not a number", AllowSpecialNumbers, "-INFINITY", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinity truncated after I", AllowSpecialNumbers, "-I", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinity truncated after In", AllowSpecialNumbers, "-In", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinity truncated after Inf", AllowSpecialNumbers, "-Inf", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinity truncated after Infi", AllowSpecialNumbers, "-Infi", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinity truncated after Infin", AllowSpecialNumbers, "-Infin", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinity truncated after Infini", AllowSpecialNumbers, "-Infini", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinity truncated after Infinit", AllowSpecialNumbers, "-Infinit", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("-Infinity not allowed", Standard, "-Infinity", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")

/* numbers */

PARSE_TEST("0 (1)", Standard, "0", FINAL, UTF8, "u(8) #(0):0,0,0,0")
PARSE_TEST("0 (2)", Standard, " 0 ", FINAL, UTF8, "u(8) #(0):1,0,1,0")
PARSE_TEST("-0 (1)", Standard, "-0", FINAL, UTF8, "u(8) #(- -0):0,0,0,0")
PARSE_TEST("-0 (2)", Standard, " -0 ", FINAL, UTF8, "u(8) #(- -0):1,0,1,0")
PARSE_TEST("7 (1)", Standard, "7", FINAL, UTF8, "u(8) #(7):0,0,0,0")
PARSE_TEST("7 (2)", Standard, " 7 ", FINAL, UTF8, "u(8) #(7):1,0,1,0")
PARSE_TEST("-7 (1)", Standard, "-7", FINAL, UTF8, "u(8) #(- -7):0,0,0,0")
PARSE_TEST("-7 (2)", Standard, " -7 ", FINAL, UTF8, "u(8) #(- -7):1,0,1,0")
PARSE_TEST("1234567890 (1)", Standard, "1234567890", FINAL, UTF8, "u(8) #(1234567890):0,0,0,0")
PARSE_TEST("1234567890 (2)", Standard, " 1234567890 ", FINAL, UTF8, "u(8) #(1234567890):1,0,1,0")
PARSE_TEST("-1234567890 (1)", Standard, "-1234567890", FINAL, UTF8, "u(8) #(- -1234567890):0,0,0,0")
PARSE_TEST("-1234567890 (2)", Standard, " -1234567890 ", FINAL, UTF8, "u(8) #(- -1234567890):1,0,1,0")
PARSE_TEST("0e1 (1)", Standard, "0e1", FINAL, UTF8, "u(8) #(e 0e1):0,0,0,0")
PARSE_TEST("0e1 (2)", Standard, " 0e1 ", FINAL, UTF8, "u(8) #(e 0e1):1,0,1,0")
PARSE_TEST("1e2 (1)", Standard, "1e2", FINAL, UTF8, "u(8) #(e 1e2):0,0,0,0")
PARSE_TEST("1e2 (2)", Standard, " 1e2 ", FINAL, UTF8, "u(8) #(e 1e2):1,0,1,0")
PARSE_TEST("0e+1 (1)", Standard, "0e+1", FINAL, UTF8, "u(8) #(e 0e+1):0,0,0,0")
PARSE_TEST("0e+1 (2)", Standard, " 0e+1 ", FINAL, UTF8, "u(8) #(e 0e+1):1,0,1,0")
PARSE_TEST("1e+2 (1)", Standard, "1e+2", FINAL, UTF8, "u(8) #(e 1e+2):0,0,0,0")
PARSE_TEST("1e+2 (2)", Standard, " 1e+2 ", FINAL, UTF8, "u(8) #(e 1e+2):1,0,1,0")
PARSE_TEST("0e-1 (1)", Standard, "0e-1", FINAL, UTF8, "u(8) #(e- 0e-1):0,0,0,0")
PARSE_TEST("0e-1 (2)", Standard, " 0e-1 ", FINAL, UTF8, "u(8) #(e- 0e-1):1,0,1,0")
PARSE_TEST("1e-2 (1)", Standard, "1e-2", FINAL, UTF8, "u(8) #(e- 1e-2):0,0,0,0")
PARSE_TEST("1e-2 (2)", Standard, " 1e-2 ", FINAL, UTF8, "u(8) #(e- 1e-2):1,0,1,0")
PARSE_TEST("1234567890E0987654321 (1)", Standard, "1234567890E0987654321", FINAL, UTF8, "u(8) #(e 1234567890E0987654321):0,0,0,0")
PARSE_TEST("1234567890E0987654321 (2)", Standard, " 1234567890E0987654321 ", FINAL, UTF8, "u(8) #(e 1234567890E0987654321):1,0,1,0")
PARSE_TEST("0.0 (1)", Standard, "0.0", FINAL, UTF8, "u(8) #(. 0.0):0,0,0,0")
PARSE_TEST("0.0 (2)", Standard, " 0.0 ", FINAL, UTF8, "u(8) #(. 0.0):1,0,1,0")
PARSE_TEST("0.12 (1)", Standard, "0.12", FINAL, UTF8, "u(8) #(. 0.12):0,0,0,0")
PARSE_TEST("0.12 (2)", Standard, " 0.12 ", FINAL, UTF8, "u(8) #(. 0.12):1,0,1,0")
PARSE_TEST("1.2 (1)", Standard, "1.2", FINAL, UTF8, "u(8) #(. 1.2):0,0,0,0")
PARSE_TEST("1.2 (2)", Standard, " 1.2 ", FINAL, UTF8, "u(8) #(. 1.2):1,0,1,0")
PARSE_TEST("1.23 (1)", Standard, "1.23", FINAL, UTF8, "u(8) #(. 1.23):0,0,0,0")
PARSE_TEST("1.23 (2)", Standard, " 1.23 ", FINAL, UTF8, "u(8) #(. 1.23):1,0,1,0")
PARSE_TEST("1.23e456 (1)", Standard, "1.23e456", FINAL, UTF8, "u(8) #(.e 1.23e456):0,0,0,0")
PARSE_TEST("1.23e456 (2)", Standard, " 1.23e456 ", FINAL, UTF8, "u(8) #(.e 1.23e456):1,0,1,0")
PARSE_TEST("1.23e+456 (1)", Standard, "1.23e+456", FINAL, UTF8, "u(8) #(.e 1.23e+456):0,0,0,0")
PARSE_TEST("1.23e+456 (2)", Standard, " 1.23e+456 ", FINAL, UTF8, "u(8) #(.e 1.23e+456):1,0,1,0")
PARSE_TEST("1.23e-456 (1)", Standard, "1.23e-456", FINAL, UTF8, "u(8) #(.e- 1.23e-456):0,0,0,0")
PARSE_TEST("1.23e-456 (2)", Standard, " 1.23e-456 ", FINAL, UTF8, "u(8) #(.e- 1.23e-456):1,0,1,0")
PARSE_TEST("max length number (1)", MaxNumberLength1, "1", FINAL, UTF8, "u(8) #(1):0,0,0,0")
PARSE_TEST("max length number (2)", MaxNumberLength2, "-1", FINAL, UTF8, "u(8) #(- -1):0,0,0,0")
PARSE_TEST("number encoded in UTF-16LE (1)", UTF16LEIn | UTF16LEOut, "0\x00", FINAL, UTF16LE, "#(0):0,0,0,0")
PARSE_TEST("number encoded in UTF-16LE (2)", UTF16LEIn | UTF16LEOut, "-\x00" "1\x00" ".\x00" "2\x00" "3\x00" "e\x00" "-\x00" "4\x00" "5\x00" "6\x00", FINAL, UTF16LE, "#(-.e- -1.23e-456):0,0,0,0")
PARSE_TEST("number encoded in UTF-16BE (1)", UTF16BEIn | UTF16BEOut, "\x00" "0", FINAL, UTF16BE, "#(0):0,0,0,0")
PARSE_TEST("number encoded in UTF-16BE (2)", UTF16BEIn | UTF16BEOut, "\x00" "-\x00" "1\x00" ".\x00" "2\x00" "3\x00" "e\x00" "-\x00" "4\x00" "5\x00" "6", FINAL, UTF16BE, "#(-.e- -1.23e-456):0,0,0,0")
PARSE_TEST("number encoded in UTF-32LE (1)", UTF32LEIn | UTF32LEOut, "0\x00\x00\x00", FINAL, UTF32LE, "#(0):0,0,0,0")
PARSE_TEST("number encoded in UTF-32LE (2)", UTF32LEIn | UTF32LEOut, "-\x00\x00\x00" "1\x00\x00\x00" ".\x00\x00\x00" "2\x00\x00\x00" "3\x00\x00\x00" "e\x00\x00\x00" "-\x00\x00\x00" "4\x00\x00\x00" "5\x00\x00\x00" "6\x00\x00\x00", FINAL, UTF32LE, "#(-.e- -1.23e-456):0,0,0,0")
PARSE_TEST("number encoded in UTF-32BE (1)", UTF32BEIn | UTF32BEOut, "\x00\x00\x00" "0", FINAL, UTF32BE, "#(0):0,0,0,0")
PARSE_TEST("number encoded in UTF-32BE (2)", UTF32BEIn | UTF32BEOut, "\x00\x00\x00" "-\x00\x00\x00" "1\x00\x00\x00" ".\x00\x00\x00" "2\x00\x00\x00" "3\x00\x00\x00" "e\x00\x00\x00" "-\x00\x00\x00" "4\x00\x00\x00" "5\x00\x00\x00" "6", FINAL, UTF32BE, "#(-.e- -1.23e-456):0,0,0,0")
PARSE_TEST("number cannot have leading + sign", Standard, "+7", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("number cannot have digits after leading 0 (1)", Standard, "00", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("number cannot have digits after leading 0 (2)", Standard, "01", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("number cannot have digits after leading 0 (3)", Standard, "-00", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("number cannot have digits after leading 0 (4)", Standard, "-01", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("number requires digit after - sign", Standard, "-x", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("number truncated after - sign", Standard, "-", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("number requires digit after decimal point", Standard, "7.x", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("number truncated after decimal point", Standard, "7.", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("number requires digit or sign after e", Standard, "7ex", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("number truncated after e", Standard, "7e", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("number requires digit or sign after E", Standard, "7Ex", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("number truncated after E", Standard, "7E", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("number requires digit after exponent + sign", Standard, "7e+x", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("number truncated after exponent + sign", Standard, "7e+", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("number requires digit after exponent - sign", Standard, "7e-x", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("number truncated after exponent - sign", Standard, "7e-", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("very long number", Standard, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", FINAL, UTF8,
           "u(8) #(123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890):0,0,0,0")
PARSE_TEST("too long number (1)", MaxNumberLength0, "1", FINAL, UTF8, "u(8) !(TooLongNumber):0,0,0,0")
PARSE_TEST("too long number (2)", MaxNumberLength1, "-1", FINAL, UTF8, "u(8) !(TooLongNumber):0,0,0,0")
PARSE_TEST("too long number (3)", MaxNumberLength2, "1.0", FINAL, UTF8, "u(8) !(TooLongNumber):0,0,0,0")

/* hex numbers */

PARSE_TEST("hex number not allowed (1)", Standard, "0x0", FINAL, UTF8, "u(8) #(0):0,0,0,0 !(UnknownToken):1,0,1,0")
PARSE_TEST("hex number not allowed (2)", Standard, "0X1", FINAL, UTF8, "u(8) #(0):0,0,0,0 !(UnknownToken):1,0,1,0")
PARSE_TEST("hex number not allowed (3)", Standard, "-0X1", FINAL, UTF8, "u(8) #(- -0):0,0,0,0 !(UnknownToken):2,0,2,0")
PARSE_TEST("negative hex number not allowed", AllowHexNumbers, "-0X1", FINAL, UTF8, "u(8) #(- -0):0,0,0,0 !(UnknownToken):2,0,2,0")
PARSE_TEST("hex number (1)", AllowHexNumbers, "0x0", FINAL, UTF8, "u(8) #(x 0x0):0,0,0,0")
PARSE_TEST("hex number (2)", AllowHexNumbers, "0x1", FINAL, UTF8, "u(8) #(x 0x1):0,0,0,0")
PARSE_TEST("hex number (3)", AllowHexNumbers, "0x0000", FINAL, UTF8, "u(8) #(x 0x0000):0,0,0,0")
PARSE_TEST("hex number (4)", AllowHexNumbers, "0x123456789abcdefABCDEF", FINAL, UTF8, "u(8) #(x 0x123456789abcdefABCDEF):0,0,0,0")
PARSE_TEST("maximum length hex number", AllowHexNumbers, "0x123456789a123456789a123456789a123456789a123456789a123456789a0", FINAL, UTF8, "u(8) #(x 0x123456789a123456789a123456789a123456789a123456789a123456789a0):0,0,0,0")
PARSE_TEST("hex number truncated after x", AllowHexNumbers, "0x", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("hex number requires  digit after x", AllowHexNumbers, "0xx", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("hex number truncated after X", AllowHexNumbers, "0X", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("hex number requires  digit after X", AllowHexNumbers, "0Xx", FINAL, UTF8, "u(8) !(InvalidNumber):0,0,0,0")
PARSE_TEST("too long hex number (1)", AllowHexNumbers | MaxNumberLength0, "0x0", FINAL, UTF8, "u(8) !(TooLongNumber):0,0,0,0")
PARSE_TEST("too long hex number (2)", AllowHexNumbers | MaxNumberLength1, "0x0", FINAL, UTF8, "u(8) !(TooLongNumber):0,0,0,0")
PARSE_TEST("too long hex number (3)", AllowHexNumbers | MaxNumberLength2, "0x0", FINAL, UTF8, "u(8) !(TooLongNumber):0,0,0,0")

/* strings */

PARSE_TEST("empty string", Standard, "\"\"", FINAL, UTF8, "u(8) s():0,0,0,0")
PARSE_TEST("UTF-8 -> UTF-8",    UTF8In | UTF8Out,    "\"" "\x61\xC2\xA9\xE4\xB8\x81\xF0\x9F\x80\x84" "\"", FINAL, UTF8, "s(ab 61 C2 A9 E4 B8 81 F0 9F 80 84):0,0,0,0")
PARSE_TEST("UTF-8 -> UTF-16LE", UTF8In | UTF16LEOut, "\"" "\x61\xC2\xA9\xE4\xB8\x81\xF0\x9F\x80\x84" "\"", FINAL, UTF8, "s(ab 61 00 A9 00 01 4E 3C D8 04 DC):0,0,0,0")
PARSE_TEST("UTF-8 -> UTF-16BE", UTF8In | UTF16BEOut, "\"" "\x61\xC2\xA9\xE4\xB8\x81\xF0\x9F\x80\x84" "\"", FINAL, UTF8, "s(ab 00 61 00 A9 4E 01 D8 3C DC 04):0,0,0,0")
PARSE_TEST("UTF-8 -> UTF-32LE", UTF8In | UTF32LEOut, "\"" "\x61\xC2\xA9\xE4\xB8\x81\xF0\x9F\x80\x84" "\"", FINAL, UTF8, "s(ab 61 00 00 00 A9 00 00 00 01 4E 00 00 04 F0 01 00):0,0,0,0")
PARSE_TEST("UTF-8 -> UTF-32BE", UTF8In | UTF32BEOut, "\"" "\x61\xC2\xA9\xE4\xB8\x81\xF0\x9F\x80\x84" "\"", FINAL, UTF8, "s(ab 00 00 00 61 00 00 00 A9 00 00 4E 01 00 01 F0 04):0,0,0,0")
PARSE_TEST("UTF-16LE -> UTF-8",    UTF16LEIn | UTF8Out,    "\"\x00" "\x61\x00\xA9\x00\x01\x4E\x3C\xD8\x04\xDC" "\"\x00", FINAL, UTF16LE, "s(ab 61 C2 A9 E4 B8 81 F0 9F 80 84):0,0,0,0")
PARSE_TEST("UTF-16LE -> UTF-16LE", UTF16LEIn | UTF16LEOut, "\"\x00" "\x61\x00\xA9\x00\x01\x4E\x3C\xD8\x04\xDC" "\"\x00", FINAL, UTF16LE, "s(ab 61 00 A9 00 01 4E 3C D8 04 DC):0,0,0,0")
PARSE_TEST("UTF-16LE -> UTF-16BE", UTF16LEIn | UTF16BEOut, "\"\x00" "\x61\x00\xA9\x00\x01\x4E\x3C\xD8\x04\xDC" "\"\x00", FINAL, UTF16LE, "s(ab 00 61 00 A9 4E 01 D8 3C DC 04):0,0,0,0")
PARSE_TEST("UTF-16LE -> UTF-32LE", UTF16LEIn | UTF32LEOut, "\"\x00" "\x61\x00\xA9\x00\x01\x4E\x3C\xD8\x04\xDC" "\"\x00", FINAL, UTF16LE, "s(ab 61 00 00 00 A9 00 00 00 01 4E 00 00 04 F0 01 00):0,0,0,0")
PARSE_TEST("UTF-16LE -> UTF-32BE", UTF16LEIn | UTF32BEOut, "\"\x00" "\x61\x00\xA9\x00\x01\x4E\x3C\xD8\x04\xDC" "\"\x00", FINAL, UTF16LE, "s(ab 00 00 00 61 00 00 00 A9 00 00 4E 01 00 01 F0 04):0,0,0,0")
PARSE_TEST("UTF-16BE -> UTF-8",    UTF16BEIn | UTF8Out,    "\x00\"" "\x00\x61\x00\xA9\x4E\x01\xD8\x3C\xDC\x04" "\x00\"", FINAL, UTF16BE, "s(ab 61 C2 A9 E4 B8 81 F0 9F 80 84):0,0,0,0")
PARSE_TEST("UTF-16BE -> UTF-16LE", UTF16BEIn | UTF16LEOut, "\x00\"" "\x00\x61\x00\xA9\x4E\x01\xD8\x3C\xDC\x04" "\x00\"", FINAL, UTF16BE, "s(ab 61 00 A9 00 01 4E 3C D8 04 DC):0,0,0,0")
PARSE_TEST("UTF-16BE -> UTF-16BE", UTF16BEIn | UTF16BEOut, "\x00\"" "\x00\x61\x00\xA9\x4E\x01\xD8\x3C\xDC\x04" "\x00\"", FINAL, UTF16BE, "s(ab 00 61 00 A9 4E 01 D8 3C DC 04):0,0,0,0")
PARSE_TEST("UTF-16BE -> UTF-32LE", UTF16BEIn | UTF32LEOut, "\x00\"" "\x00\x61\x00\xA9\x4E\x01\xD8\x3C\xDC\x04" "\x00\"", FINAL, UTF16BE, "s(ab 61 00 00 00 A9 00 00 00 01 4E 00 00 04 F0 01 00):0,0,0,0")
PARSE_TEST("UTF-16BE -> UTF-32BE", UTF16BEIn | UTF32BEOut, "\x00\"" "\x00\x61\x00\xA9\x4E\x01\xD8\x3C\xDC\x04" "\x00\"", FINAL, UTF16BE, "s(ab 00 00 00 61 00 00 00 A9 00 00 4E 01 00 01 F0 04):0,0,0,0")
PARSE_TEST("UTF-32LE -> UTF-8",    UTF32LEIn | UTF8Out,    "\"\x00\x00\x00" "\x61\x00\x00\x00\xA9\x00\x00\x00\x01\x4E\x00\x00\x04\xF0\x01\x00" "\"\x00\x00\x00", FINAL, UTF32LE, "s(ab 61 C2 A9 E4 B8 81 F0 9F 80 84):0,0,0,0")
PARSE_TEST("UTF-32LE -> UTF-16LE", UTF32LEIn | UTF16LEOut, "\"\x00\x00\x00" "\x61\x00\x00\x00\xA9\x00\x00\x00\x01\x4E\x00\x00\x04\xF0\x01\x00" "\"\x00\x00\x00", FINAL, UTF32LE, "s(ab 61 00 A9 00 01 4E 3C D8 04 DC):0,0,0,0")
PARSE_TEST("UTF-32LE -> UTF-16BE", UTF32LEIn | UTF16BEOut, "\"\x00\x00\x00" "\x61\x00\x00\x00\xA9\x00\x00\x00\x01\x4E\x00\x00\x04\xF0\x01\x00" "\"\x00\x00\x00", FINAL, UTF32LE, "s(ab 00 61 00 A9 4E 01 D8 3C DC 04):0,0,0,0")
PARSE_TEST("UTF-32LE -> UTF-32LE", UTF32LEIn | UTF32LEOut, "\"\x00\x00\x00" "\x61\x00\x00\x00\xA9\x00\x00\x00\x01\x4E\x00\x00\x04\xF0\x01\x00" "\"\x00\x00\x00", FINAL, UTF32LE, "s(ab 61 00 00 00 A9 00 00 00 01 4E 00 00 04 F0 01 00):0,0,0,0")
PARSE_TEST("UTF-32LE -> UTF-32BE", UTF32LEIn | UTF32BEOut, "\"\x00\x00\x00" "\x61\x00\x00\x00\xA9\x00\x00\x00\x01\x4E\x00\x00\x04\xF0\x01\x00" "\"\x00\x00\x00", FINAL, UTF32LE, "s(ab 00 00 00 61 00 00 00 A9 00 00 4E 01 00 01 F0 04):0,0,0,0")
PARSE_TEST("UTF-32BE -> UTF-8",    UTF32BEIn | UTF8Out,    "\x00\x00\x00\"" "\x00\x00\x00\x61\x00\x00\x00\xA9\x00\x00\x4E\x01\x00\x01\xF0\x04" "\x00\x00\x00\"", FINAL, UTF32BE, "s(ab 61 C2 A9 E4 B8 81 F0 9F 80 84):0,0,0,0")
PARSE_TEST("UTF-32BE -> UTF-16LE", UTF32BEIn | UTF16LEOut, "\x00\x00\x00\"" "\x00\x00\x00\x61\x00\x00\x00\xA9\x00\x00\x4E\x01\x00\x01\xF0\x04" "\x00\x00\x00\"", FINAL, UTF32BE, "s(ab 61 00 A9 00 01 4E 3C D8 04 DC):0,0,0,0")
PARSE_TEST("UTF-32BE -> UTF-16BE", UTF32BEIn | UTF16BEOut, "\x00\x00\x00\"" "\x00\x00\x00\x61\x00\x00\x00\xA9\x00\x00\x4E\x01\x00\x01\xF0\x04" "\x00\x00\x00\"", FINAL, UTF32BE, "s(ab 00 61 00 A9 4E 01 D8 3C DC 04):0,0,0,0")
PARSE_TEST("UTF-32BE -> UTF-32LE", UTF32BEIn | UTF32LEOut, "\x00\x00\x00\"" "\x00\x00\x00\x61\x00\x00\x00\xA9\x00\x00\x4E\x01\x00\x01\xF0\x04" "\x00\x00\x00\"", FINAL, UTF32BE, "s(ab 61 00 00 00 A9 00 00 00 01 4E 00 00 04 F0 01 00):0,0,0,0")
PARSE_TEST("UTF-32BE -> UTF-32BE", UTF32BEIn | UTF32BEOut, "\x00\x00\x00\"" "\x00\x00\x00\x61\x00\x00\x00\xA9\x00\x00\x4E\x01\x00\x01\xF0\x04" "\x00\x00\x00\"", FINAL, UTF32BE, "s(ab 00 00 00 61 00 00 00 A9 00 00 4E 01 00 01 F0 04):0,0,0,0")
PARSE_TEST("all whitespace string", Standard, "\" \\r\\n\\t \"", FINAL, UTF8, "u(8) s(c 20 0D 0A 09 20):0,0,0,0")
PARSE_TEST("ASCII string", Standard, "\"abc DEF 123\"", FINAL, UTF8, "u(8) s(61 62 63 20 44 45 46 20 31 32 33):0,0,0,0")
PARSE_TEST("simple string escape sequences", Standard, "\"\\\"\\\\/\\t\\n\\r\\f\\b\"", FINAL, UTF8, "u(8) s(c 22 5C 2F 09 0A 0D 0C 08):0,0,0,0")
PARSE_TEST("string hex escape sequences", Standard, "\"\\u0000\\u0020\\u0aF9\\ufFfF\\uD834\\udd1e\"", FINAL, UTF8, "u(8) s(zcab 00 20 E0 AB B9 EF BF BF F0 9D 84 9E):0,0,0,0")
PARSE_TEST("string escaped control characters", Standard, "\""
                   "\\u0000\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007\\u0008\\u0009\\u000A\\u000B\\u000C\\u000D\\u000E\\u000F"
                   "\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017\\u0018\\u0019\\u001A\\u001B\\u001C\\u001D\\u001E\\u001F"
                   "\"", FINAL, UTF8, "u(8) s(zc 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F):0,0,0,0")
PARSE_TEST("non-control ASCII string", Standard, "\""
                   " !\\u0022#$%&'()+*,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\u005C]^_`abcdefghijklmnopqrstuvwxyz{|}~\\u007F"
                   "\"", FINAL, UTF8, "u(8) s(20 21 22 23 24 25 26 27 28 29 2B 2A 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A 3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A 4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A 5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A 7B 7C 7D 7E 7F):0,0,0,0")
PARSE_TEST("long string", Standard, "\""
                   "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
                   "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
                   "\"", FINAL, UTF8, "u(8) s(30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46):0,0,0,0")
PARSE_TEST("unterminated string (1)", Standard, "\"", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("unterminated string (2)", Standard, "\"abc", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string cannot contain unescaped control character (1)", Standard, "\"abc\x00\"", FINAL, UTF8, "u(8) !(UnescapedControlCharacter):4,0,4,0")
PARSE_TEST("string cannot contain unescaped control character (2)", Standard, "\"abc\x09\"", FINAL, UTF8, "u(8) !(UnescapedControlCharacter):4,0,4,0")
PARSE_TEST("string cannot contain unescaped control character (3)", Standard, "\"abc\x0A\"", FINAL, UTF8, "u(8) !(UnescapedControlCharacter):4,0,4,0")
PARSE_TEST("string cannot contain unescaped control character (4)", Standard, "\"abc\x0D\"", FINAL, UTF8, "u(8) !(UnescapedControlCharacter):4,0,4,0")
PARSE_TEST("string cannot contain unescaped control character (5)", Standard, "\"abc\x1F\"", FINAL, UTF8, "u(8) !(UnescapedControlCharacter):4,0,4,0")
PARSE_TEST("string cannot contain invalid escape sequence (1)", Standard, "\"\\v\"", FINAL, UTF8, "u(8) !(InvalidEscapeSequence):1,0,1,0")
PARSE_TEST("string cannot contain invalid escape sequence (2)", Standard, "\"\\x0020\"", FINAL, UTF8, "u(8) !(InvalidEscapeSequence):1,0,1,0")
PARSE_TEST("string cannot contain invalid escape sequence (3)", Standard, "\"\\ \"", FINAL, UTF8, "u(8) !(InvalidEscapeSequence):1,0,1,0")
PARSE_TEST("string truncated after \\", Standard, "\"\\", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string truncated after \\u", Standard, "\"\\u", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string truncated after \\ux", Standard, "\"\\u0", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string truncated after \\uxx", Standard, "\"\\u01", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string truncated after \\uxxx", Standard, "\"\\u01a", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string requires hex digit after \\u", Standard, "\"\\ux\"", FINAL, UTF8, "u(8) !(InvalidEscapeSequence):1,0,1,0")
PARSE_TEST("string requires hex digit after \\ux", Standard, "\"\\u0x\"", FINAL, UTF8, "u(8) !(InvalidEscapeSequence):1,0,1,0")
PARSE_TEST("string requires hex digit after \\uxx", Standard, "\"\\u01x\"", FINAL, UTF8, "u(8) !(InvalidEscapeSequence):1,0,1,0")
PARSE_TEST("string requires hex digit after \\uxxx", Standard, "\"\\u01ax\"", FINAL, UTF8, "u(8) !(InvalidEscapeSequence):1,0,1,0")
PARSE_TEST("string truncated after escaped leading surrogate", Standard, "\"\\uD800", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string requires escaped surrogates to appear in valid pairs (1)", Standard, "\"\\uD834\"", FINAL, UTF8, "u(8) !(UnpairedSurrogateEscapeSequence):1,0,1,0")
PARSE_TEST("string requires escaped surrogates to appear in valid pairs (2)", Standard, "\"\\uD834x\"", FINAL, UTF8, "u(8) !(UnpairedSurrogateEscapeSequence):1,0,1,0")
PARSE_TEST("string requires escaped surrogates to appear in valid pairs (3)", Standard, "\"\\uD834\\n\"", FINAL, UTF8, "u(8) !(UnpairedSurrogateEscapeSequence):1,0,1,0")
PARSE_TEST("string requires escaped surrogates to appear in valid pairs (4)", Standard, "\"\\uD834\\u0020\"", FINAL, UTF8, "u(8) !(UnpairedSurrogateEscapeSequence):1,0,1,0")
PARSE_TEST("string requires escaped surrogates to appear in valid pairs (5)", Standard, "\"\\uD834\\uD834\"", FINAL, UTF8, "u(8) !(UnpairedSurrogateEscapeSequence):1,0,1,0")
PARSE_TEST("string requires escaped surrogates to appear in valid pairs (6)", Standard, "\"\\uDC00\"", FINAL, UTF8, "u(8) !(UnpairedSurrogateEscapeSequence):1,0,1,0")
PARSE_TEST("string truncated after \\ of trailing surrogate escape sequence", Standard, "\"\\uD834\\", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string truncated after \\u of trailing surrogate escape sequence", Standard, "\"\\uD834\\u", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string truncated after \\ux of trailing surrogate escape sequence", Standard, "\"\\uD834\\uD", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string truncated after \\uxx of trailing surrogate escape sequence", Standard, "\"\\uD834\\uDD", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("string truncated after \\uxxx of trailing surrogate escape sequence", Standard, "\"\\uD834\\uDD1", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("max length 0 string (1)", MaxStringLength0, "\"\"", FINAL, UTF8, "u(8) s():0,0,0,0")
PARSE_TEST("max length 0 string (2)", MaxStringLength0, "{\"\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 m():1,0,1,1 #(0):4,0,4,1 }:5,0,5,0")
PARSE_TEST("max length 0 string (3)", MaxStringLength0, "\"a\"", FINAL, UTF8, "u(8) !(TooLongString):0,0,0,0")
PARSE_TEST("max length 0 string (4)", MaxStringLength0, "{\"a\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 !(TooLongString):1,0,1,1")
PARSE_TEST("max length 1 string (1)", MaxStringLength1, "\"a\"", FINAL, UTF8, "u(8) s(61):0,0,0,0")
PARSE_TEST("max length 1 string (2)", MaxStringLength1, "{\"a\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):5,0,5,1 }:6,0,6,0")
PARSE_TEST("max length 1 string (3)", MaxStringLength1, "\"ab\"", FINAL, UTF8, "u(8) !(TooLongString):0,0,0,0")
PARSE_TEST("max length 1 string (4)", MaxStringLength1, "{\"ab\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 !(TooLongString):1,0,1,1")
PARSE_TEST("max length 1 string (5)", MaxStringLength1, "\"\xE0\xAB\xB9\"", FINAL, UTF8, "u(8) !(TooLongString):0,0,0,0")
PARSE_TEST("max length 2 string (1)", MaxStringLength2, "\"ab\"", FINAL, UTF8, "u(8) s(61 62):0,0,0,0")
PARSE_TEST("max length 2 string (2)", MaxStringLength2, "{\"ab\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61 62):1,0,1,1 #(0):6,0,6,1 }:7,0,7,0")
PARSE_TEST("max length 2 string (3)", MaxStringLength2, "\"abc\"", FINAL, UTF8, "u(8) !(TooLongString):0,0,0,0")
PARSE_TEST("max length 2 string (4)", MaxStringLength2, "{\"abc\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 !(TooLongString):1,0,1,1")
PARSE_TEST("max length 2 string (5)", MaxStringLength2, "\"\xE0\xAB\xB9\"", FINAL, UTF8, "u(8) !(TooLongString):0,0,0,0")

/* objects */

PARSE_TEST("start object", UTF8In, "{", PARTIAL, UTF8, "{:0,0,0,0")
PARSE_TEST("empty object", Standard, "{}", FINAL, UTF8, "u(8) {:0,0,0,0 }:1,0,1,0")
PARSE_TEST("single-member object", Standard, "{ \"pi\" : 3.14159 }", FINAL, UTF8, "u(8) {:0,0,0,0 m(70 69):2,0,2,1 #(. 3.14159):9,0,9,1 }:17,0,17,0")
PARSE_TEST("multi-member object", Standard, "{ \"pi\" : 3.14159, \"e\" : 2.71828 }", FINAL, UTF8, "u(8) {:0,0,0,0 m(70 69):2,0,2,1 #(. 3.14159):9,0,9,1 m(65):18,0,18,1 #(. 2.71828):24,0,24,1 }:32,0,32,0")
PARSE_TEST("all types of object member values", AllowSpecialNumbers | AllowHexNumbers, "{ \"a\" : null, \"b\" : true, \"c\" : \"foo\", \"d\" : 17, \"e\" : NaN, \"f\": 0xbeef, \"g\" : {}, \"h\" : {}, \"i\" : [] }", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):2,0,2,1 n:8,0,8,1 m(62):14,0,14,1 t:20,0,20,1 m(63):26,0,26,1 s(66 6F 6F):32,0,32,1 m(64):39,0,39,1 #(17):45,0,45,1 m(65):49,0,49,1 #(NaN):55,0,55,1 m(66):60,0,60,1 #(x 0xbeef):65,0,65,1 m(67):73,0,73,1 {:79,0,79,1 }:80,0,80,1 m(68):83,0,83,1 {:89,0,89,1 }:90,0,90,1 m(69):93,0,93,1 [:99,0,99,1 ]:100,0,100,1 }:102,0,102,0")
PARSE_TEST("nested objects", Standard, "{\"a\":{\"b\":{\"c\":{\"d\":{\"e\":{}}}}}}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 {:5,0,5,1 m(62):6,0,6,2 {:10,0,10,2 m(63):11,0,11,3 {:15,0,15,3 m(64):16,0,16,4 {:20,0,20,4 m(65):21,0,21,5 {:25,0,25,5 }:26,0,26,5 }:27,0,27,4 }:28,0,28,3 }:29,0,29,2 }:30,0,30,1 }:31,0,31,0")
PARSE_TEST("object members with similar names", Standard, "{\"\":null,\"\\u0000\":0,\"x\":1,\"X\":2,\"x2\":3,\"x\\u0000\":4,\"x\\u0000y\":5}", FINAL, UTF8, "u(8) {:0,0,0,0 m():1,0,1,1 n:4,0,4,1 m(zc 00):9,0,9,1 #(0):18,0,18,1 m(78):20,0,20,1 #(1):24,0,24,1 m(58):26,0,26,1 #(2):30,0,30,1 m(78 32):32,0,32,1 #(3):37,0,37,1 m(zc 78 00):39,0,39,1 #(4):49,0,49,1 m(zc 78 00 79):51,0,51,1 #(5):62,0,62,1 }:63,0,63,0")
PARSE_TEST("different objects with members with same names", Standard, "{\"foo\":{\"foo\":{\"foo\":3}}}", FINAL, UTF8, "u(8) {:0,0,0,0 m(66 6F 6F):1,0,1,1 {:7,0,7,1 m(66 6F 6F):8,0,8,2 {:14,0,14,2 m(66 6F 6F):15,0,15,3 #(3):21,0,21,3 }:22,0,22,2 }:23,0,23,1 }:24,0,24,0")
PARSE_TEST("object truncated after left curly brace", Standard, "{", FINAL, UTF8, "u(8) {:0,0,0,0 !(ExpectedMoreTokens):1,0,1,1")
PARSE_TEST("object truncated after member name (1)", Standard, "{\"x\"", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 !(ExpectedMoreTokens):4,0,4,1")
PARSE_TEST("object truncated after member name (2)", Standard, "{\"x\":1,\"y\"", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 !(ExpectedMoreTokens):10,0,10,1")
PARSE_TEST("object truncated after colon (1)", Standard, "{\"x\":", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 !(ExpectedMoreTokens):5,0,5,1")
PARSE_TEST("object truncated after colon (2)", Standard, "{\"x\":1,\"y\":", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 !(ExpectedMoreTokens):11,0,11,1")
PARSE_TEST("object truncated after member value (1)", Standard, "{\"x\":1", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 !(ExpectedMoreTokens):6,0,6,1")
PARSE_TEST("object truncated after member value (2)", Standard, "{\"x\":1,\"y\":2", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 #(2):11,0,11,1 !(ExpectedMoreTokens):12,0,12,1")
PARSE_TEST("object truncated after comma (1)", Standard, "{\"x\":1,", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 !(ExpectedMoreTokens):7,0,7,1")
PARSE_TEST("object truncated after comma (2)", Standard, "{\"x\":1,\"y\":2,", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 #(2):11,0,11,1 !(ExpectedMoreTokens):13,0,13,1")
PARSE_TEST("object requires string member names (1)", Standard, "{null:1}", FINAL, UTF8, "u(8) {:0,0,0,0 !(UnexpectedToken):1,0,1,1")
PARSE_TEST("object requires string member names (2)", Standard, "{true:1}", FINAL, UTF8, "u(8) {:0,0,0,0 !(UnexpectedToken):1,0,1,1")
PARSE_TEST("object requires string member names (3)", Standard, "{false:1}", FINAL, UTF8, "u(8) {:0,0,0,0 !(UnexpectedToken):1,0,1,1")
PARSE_TEST("object requires string member names (4)", Standard, "{7:1}", FINAL, UTF8, "u(8) {:0,0,0,0 !(UnexpectedToken):1,0,1,1")
PARSE_TEST("object requires string member names (5)", Standard, "{[]:1}", FINAL, UTF8, "u(8) {:0,0,0,0 !(UnexpectedToken):1,0,1,1")
PARSE_TEST("object requires string member names (6)", Standard, "{{}:1}", FINAL, UTF8, "u(8) {:0,0,0,0 !(UnexpectedToken):1,0,1,1")
PARSE_TEST("object member requires value (1)", Standard, "{\"x\"}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 !(UnexpectedToken):4,0,4,1")
PARSE_TEST("object member requires value (2)", Standard, "{\"x\":}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 !(UnexpectedToken):5,0,5,1")
PARSE_TEST("object member missing (1)", Standard, "{,\"y\":2}", FINAL, UTF8, "u(8) {:0,0,0,0 !(UnexpectedToken):1,0,1,1")
PARSE_TEST("object member missing (2)", Standard, "{\"x\":1,,\"y\":2}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 !(UnexpectedToken):7,0,7,1")
PARSE_TEST("object member missing (3)", Standard, "{\"x\":1,}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 !(UnexpectedToken):7,0,7,1")
PARSE_TEST("object members require comma separator", Standard, "{\"x\":1 \"y\":2}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 !(UnexpectedToken):7,0,7,1")
PARSE_TEST("object members must be unique (1)", TrackObjectMembers, "{\"x\":1,\"x\":2}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 !(DuplicateObjectMember):7,0,7,1")
PARSE_TEST("object members must be unique (2)", TrackObjectMembers, "{\"x\":1,\"y\":2,\"x\":3}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 #(2):11,0,11,1 !(DuplicateObjectMember):13,0,13,1")
PARSE_TEST("object members must be unique (3)", TrackObjectMembers, "{\"x\":1,\"y\":{\"TRUE\":true,\"FALSE\":false},\"x\":3}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 {:11,0,11,1 m(54 52 55 45):12,0,12,2 t:19,0,19,2 m(46 41 4C 53 45):24,0,24,2 f:32,0,32,2 }:37,0,37,1 !(DuplicateObjectMember):39,0,39,1")
PARSE_TEST("object members must be unique (4)", TrackObjectMembers, "{\"x\":1,\"y\":{\"TRUE\":true,\"TRUE\":true},\"z\":3}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 {:11,0,11,1 m(54 52 55 45):12,0,12,2 t:19,0,19,2 !(DuplicateObjectMember):24,0,24,2")
PARSE_TEST("object members must be unique (5)", TrackObjectMembers, "{\"x\":1,\"y\":2,\"y\":3}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 #(2):11,0,11,1 !(DuplicateObjectMember):13,0,13,1")
PARSE_TEST("allow duplicate object members (1)", Standard, "{\"x\":1,\"x\":2}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(78):7,0,7,1 #(2):11,0,11,1 }:12,0,12,0")
PARSE_TEST("allow duplicate object members (2)", Standard, "{\"x\":1,\"y\":2,\"x\":3}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 #(2):11,0,11,1 m(78):13,0,13,1 #(3):17,0,17,1 }:18,0,18,0")
PARSE_TEST("allow duplicate object members (3)", Standard, "{\"x\":1,\"y\":{\"TRUE\":true,\"FALSE\":false},\"x\":3}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 {:11,0,11,1 m(54 52 55 45):12,0,12,2 t:19,0,19,2 m(46 41 4C 53 45):24,0,24,2 f:32,0,32,2 }:37,0,37,1 m(78):39,0,39,1 #(3):43,0,43,1 }:44,0,44,0")
PARSE_TEST("allow duplicate object members (4)", Standard, "{\"x\":1,\"y\":{\"TRUE\":true,\"TRUE\":true},\"z\":3}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 {:11,0,11,1 m(54 52 55 45):12,0,12,2 t:19,0,19,2 m(54 52 55 45):24,0,24,2 t:31,0,31,2 }:35,0,35,1 m(7A):37,0,37,1 #(3):41,0,41,1 }:42,0,42,0")
PARSE_TEST("allow duplicate object members (5)", Standard, "{\"x\":1,\"y\":2,\"y\":3}", FINAL, UTF8, "u(8) {:0,0,0,0 m(78):1,0,1,1 #(1):5,0,5,1 m(79):7,0,7,1 #(2):11,0,11,1 m(79):13,0,13,1 #(3):17,0,17,1 }:18,0,18,0")
PARSE_TEST("detect duplicate object member in callback", Standard, "{\"duplicate\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 !(DuplicateObjectMember):1,0,1,1")
PARSE_TEST("empty string object member name (1)", Standard, "{\"\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 m():1,0,1,1 #(0):4,0,4,1 }:5,0,5,0")
PARSE_TEST("empty string object member name (2)", TrackObjectMembers, "{\"\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 m():1,0,1,1 #(0):4,0,4,1 }:5,0,5,0")
PARSE_TEST("empty string object member name (3)", TrackObjectMembers, "{\"\":0,\"x\":1}", FINAL, UTF8, "u(8) {:0,0,0,0 m():1,0,1,1 #(0):4,0,4,1 m(78):6,0,6,1 #(1):10,0,10,1 }:11,0,11,0")
PARSE_TEST("empty string object member name (4)", TrackObjectMembers, "{\"\":0,\"\":1}", FINAL, UTF8, "u(8) {:0,0,0,0 m():1,0,1,1 #(0):4,0,4,1 !(DuplicateObjectMember):6,0,6,1")

/* arrays */

PARSE_TEST("start array", UTF8In, "[", PARTIAL, UTF8, "[:0,0,0,0")
PARSE_TEST("empty array", Standard, "[]", FINAL, UTF8, "u(8) [:0,0,0,0 ]:1,0,1,0")
PARSE_TEST("single-item array", Standard, "[ 3.14159 ]", FINAL, UTF8, "u(8) [:0,0,0,0 i:2,0,2,1 #(. 3.14159):2,0,2,1 ]:10,0,10,0")
PARSE_TEST("multi-item array", Standard, "[ 3.14159, 2.71828 ]", FINAL, UTF8, "u(8) [:0,0,0,0 i:2,0,2,1 #(. 3.14159):2,0,2,1 i:11,0,11,1 #(. 2.71828):11,0,11,1 ]:19,0,19,0")
PARSE_TEST("all types of array items", AllowSpecialNumbers | AllowHexNumbers, "[ null, true, \"foo\", 17, NaN, 0xbeef, {}, [] ]", FINAL, UTF8, "u(8) [:0,0,0,0 i:2,0,2,1 n:2,0,2,1 i:8,0,8,1 t:8,0,8,1 i:14,0,14,1 s(66 6F 6F):14,0,14,1 i:21,0,21,1 #(17):21,0,21,1 i:25,0,25,1 #(NaN):25,0,25,1 i:30,0,30,1 #(x 0xbeef):30,0,30,1 i:38,0,38,1 {:38,0,38,1 }:39,0,39,1 i:42,0,42,1 [:42,0,42,1 ]:43,0,43,1 ]:45,0,45,0")
PARSE_TEST("nested arrays", Standard, "[[],[[],[[],[[],[[],[]]]]]]", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 [:1,0,1,1 ]:2,0,2,1 i:4,0,4,1 [:4,0,4,1 i:5,0,5,2 [:5,0,5,2 ]:6,0,6,2 i:8,0,8,2 [:8,0,8,2 i:9,0,9,3 [:9,0,9,3 ]:10,0,10,3 i:12,0,12,3 [:12,0,12,3 i:13,0,13,4 [:13,0,13,4 ]:14,0,14,4 i:16,0,16,4 [:16,0,16,4 i:17,0,17,5 [:17,0,17,5 ]:18,0,18,5 i:20,0,20,5 [:20,0,20,5 ]:21,0,21,5 ]:22,0,22,4 ]:23,0,23,3 ]:24,0,24,2 ]:25,0,25,1 ]:26,0,26,0")
PARSE_TEST("array truncated after left square brace", Standard, "[", FINAL, UTF8, "u(8) [:0,0,0,0 !(ExpectedMoreTokens):1,0,1,1")
PARSE_TEST("array truncated after item value (1)", Standard, "[1", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(1):1,0,1,1 !(ExpectedMoreTokens):2,0,2,1")
PARSE_TEST("array truncated after item value (2)", Standard, "[1,2", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(1):1,0,1,1 i:3,0,3,1 #(2):3,0,3,1 !(ExpectedMoreTokens):4,0,4,1")
PARSE_TEST("array truncated after comma (1)", Standard, "[1,", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(1):1,0,1,1 !(ExpectedMoreTokens):3,0,3,1")
PARSE_TEST("array truncated after comma (2)", Standard, "[1,2,", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(1):1,0,1,1 i:3,0,3,1 #(2):3,0,3,1 !(ExpectedMoreTokens):5,0,5,1")
PARSE_TEST("array item missing (1)", Standard, "[,2]", FINAL, UTF8, "u(8) [:0,0,0,0 !(UnexpectedToken):1,0,1,1")
PARSE_TEST("array item missing (2)", Standard, "[1,,2]", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(1):1,0,1,1 !(UnexpectedToken):3,0,3,1")
PARSE_TEST("array item missing (3)", Standard, "[1,]", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(1):1,0,1,1 !(UnexpectedToken):3,0,3,1")
PARSE_TEST("array items require comma separator", Standard, "[1 2]", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(1):1,0,1,1 !(UnexpectedToken):3,0,3,1")

/* comments */

PARSE_TEST("single-line comment not allowed (1)", Standard, "0 // comment", FINAL, UTF8, "u(8) #(0):0,0,0,0 !(UnknownToken):2,0,2,0")
PARSE_TEST("single-line comment not allowed (2)", Standard, "// comment\r\n0", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("multi-line comment not allowed (1)", Standard, "0 /* comment */", FINAL, UTF8, "u(8) #(0):0,0,0,0 !(UnknownToken):2,0,2,0")
PARSE_TEST("multi-line comment not allowed (2)", Standard, "/* comment */0", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("multi-line comment not allowed (3)", Standard, "/* comment \r\n * / * /*/0", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("multi-line comment not allowed (4)", Standard, "/* comment \r\n * / * /*/\r\n0", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("single-line comment (1)", AllowComments, "0 //", FINAL, UTF8, "u(8) #(0):0,0,0,0")
PARSE_TEST("single-line comment (2)", AllowComments, "0 // comment", FINAL, UTF8, "u(8) #(0):0,0,0,0")
PARSE_TEST("single-line comment (3)", AllowComments, "// comment\r\n0", FINAL, UTF8, "u(8) #(0):12,1,0,0")
PARSE_TEST("single-line comment with extra slashes", AllowComments, "0 ////////////", FINAL, UTF8, "u(8) #(0):0,0,0,0")
PARSE_TEST("single-line comment in object (1)", AllowComments, "{// comment\n\"a\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):12,1,0,1 #(0):16,1,4,1 }:17,1,5,0")
PARSE_TEST("single-line comment in object (2)", AllowComments, "{\"a\"// comment\n:0}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):16,1,1,1 }:17,1,2,0")
PARSE_TEST("single-line comment in object (3)", AllowComments, "{\"a\":// comment\n0}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):16,1,0,1 }:17,1,1,0")
PARSE_TEST("single-line comment in object (4)", AllowComments, "{\"a\":0// comment\n}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):5,0,5,1 }:17,1,0,0")
PARSE_TEST("single-line comment in object (5)", AllowComments, "{\"a\":0// comment\n,\"b\":1}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):5,0,5,1 m(62):18,1,1,1 #(1):22,1,5,1 }:23,1,6,0")
PARSE_TEST("single-line comment in object (6)", AllowComments, "{\"a\":0,// comment\n\"b\":1}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):5,0,5,1 m(62):18,1,0,1 #(1):22,1,4,1 }:23,1,5,0")
PARSE_TEST("single-line comment in array (1)", AllowComments, "[// comment\n0]", FINAL, UTF8, "u(8) [:0,0,0,0 i:12,1,0,1 #(0):12,1,0,1 ]:13,1,1,0")
PARSE_TEST("single-line comment in array (2)", AllowComments, "[0// comment\n]", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(0):1,0,1,1 ]:13,1,0,0")
PARSE_TEST("single-line comment in array (3)", AllowComments, "[0// comment\n,1]", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(0):1,0,1,1 i:14,1,1,1 #(1):14,1,1,1 ]:15,1,2,0")
PARSE_TEST("single-line comment in array (4)", AllowComments, "[0,// comment\n1]", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(0):1,0,1,1 i:14,1,0,1 #(1):14,1,0,1 ]:15,1,1,0")
PARSE_TEST("multi-line comment (1)", AllowComments, "0 /**/", FINAL, UTF8, "u(8) #(0):0,0,0,0")
PARSE_TEST("multi-line comment (2)", AllowComments, "0 /* comment */", FINAL, UTF8, "u(8) #(0):0,0,0,0")
PARSE_TEST("multi-line comment (3)", AllowComments, "/* comment */0", FINAL, UTF8, "u(8) #(0):13,0,13,0")
PARSE_TEST("multi-line comment (4)", AllowComments, "/* comment \r\n * / * /*/0", FINAL, UTF8, "u(8) #(0):23,1,10,0")
PARSE_TEST("multi-line comment (5)", AllowComments, "/* comment \r\n * / * /*/\r\n0", FINAL, UTF8, "u(8) #(0):25,2,0,0")
PARSE_TEST("multi-line comment with extra stars", AllowComments, "0 /************/", FINAL, UTF8, "u(8) #(0):0,0,0,0")
PARSE_TEST("multi-line comment in object (1)", AllowComments, "{/* comment */\"a\":0}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):14,0,14,1 #(0):18,0,18,1 }:19,0,19,0")
PARSE_TEST("multi-line comment in object (2)", AllowComments, "{\"a\"/* comment */:0}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):18,0,18,1 }:19,0,19,0")
PARSE_TEST("multi-line comment in object (3)", AllowComments, "{\"a\":/* comment */0}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):18,0,18,1 }:19,0,19,0")
PARSE_TEST("multi-line comment in object (4)", AllowComments, "{\"a\":0/* comment */}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):5,0,5,1 }:19,0,19,0")
PARSE_TEST("multi-line comment in object (5)", AllowComments, "{\"a\":0/* comment */,\"b\":1}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):5,0,5,1 m(62):20,0,20,1 #(1):24,0,24,1 }:25,0,25,0")
PARSE_TEST("multi-line comment in object (6)", AllowComments, "{\"a\":0,/* comment */\"b\":1}", FINAL, UTF8, "u(8) {:0,0,0,0 m(61):1,0,1,1 #(0):5,0,5,1 m(62):20,0,20,1 #(1):24,0,24,1 }:25,0,25,0")
PARSE_TEST("multi-line comment in array (1)", AllowComments, "[/* comment */0]", FINAL, UTF8, "u(8) [:0,0,0,0 i:14,0,14,1 #(0):14,0,14,1 ]:15,0,15,0")
PARSE_TEST("multi-line comment in array (2)", AllowComments, "[0/* comment */]", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(0):1,0,1,1 ]:15,0,15,0")
PARSE_TEST("multi-line comment in array (3)", AllowComments, "[0/* comment */,1]", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(0):1,0,1,1 i:16,0,16,1 #(1):16,0,16,1 ]:17,0,17,0")
PARSE_TEST("multi-line comment in array (4)", AllowComments, "[0,/* comment */1]", FINAL, UTF8, "u(8) [:0,0,0,0 i:1,0,1,1 #(0):1,0,1,1 i:16,0,16,1 #(1):16,0,16,1 ]:17,0,17,0")
PARSE_TEST("unclosed multi-line comment (1)", AllowComments, "/*", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("unclosed multi-line comment (2)", AllowComments, "/* comment", FINAL, UTF8, "u(8) !(IncompleteToken):0,0,0,0")
PARSE_TEST("just a comment (1)", AllowComments, "//", FINAL, UTF8, "u(8) !(ExpectedMoreTokens):2,0,2,0")
PARSE_TEST("just a comment (2)", AllowComments, "/**/", FINAL, UTF8, "u(8) !(ExpectedMoreTokens):4,0,4,0")
PARSE_TEST("comment between tokens (1)", AllowComments, "[//\n]", FINAL, UTF8, "u(8) [:0,0,0,0 ]:4,1,0,0")
PARSE_TEST("comment between tokens (2)", AllowComments, "[/**/]", FINAL, UTF8, "u(8) [:0,0,0,0 ]:5,0,5,0")
PARSE_TEST("lone forward slash (1)", AllowComments, "/", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("lone forward slash (2)", AllowComments, "/ ", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")

/* random tokens */

PARSE_TEST("random ]", Standard, "]", FINAL, UTF8, "u(8) !(UnexpectedToken):0,0,0,0")
PARSE_TEST("random }", Standard, "}", FINAL, UTF8, "u(8) !(UnexpectedToken):0,0,0,0")
PARSE_TEST("random :", Standard, ":", FINAL, UTF8, "u(8) !(UnexpectedToken):0,0,0,0")
PARSE_TEST("random ,", Standard, ",", FINAL, UTF8, "u(8) !(UnexpectedToken):0,0,0,0")
PARSE_TEST("single-quoted strings not allowed", Standard, "'abc'", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("random \\", Standard, "\\n", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")
PARSE_TEST("random /", Standard, "/", FINAL, UTF8, "u(8) !(UnknownToken):0,0,0,0")

/* multi-line input */

PARSE_TEST("multi-line input", Standard, "[\r 1,\n  2,\r\n\r\n   3]", FINAL, UTF8, "u(8) [:0,0,0,0 i:3,1,1,1 #(1):3,1,1,1 i:8,2,2,1 #(2):8,2,2,1 i:17,4,3,1 #(3):17,4,3,1 ]:18,4,4,0")
PARSE_TEST("multi-line input error (1)", Standard, "[\r1", FINAL, UTF8, "u(8) [:0,0,0,0 i:2,1,0,1 #(1):2,1,0,1 !(ExpectedMoreTokens):3,1,1,1")
PARSE_TEST("multi-line input error (2)", Standard, "[\n1", FINAL, UTF8, "u(8) [:0,0,0,0 i:2,1,0,1 #(1):2,1,0,1 !(ExpectedMoreTokens):3,1,1,1")
PARSE_TEST("multi-line input error (3)", Standard, "[\r\n1", FINAL, UTF8, "u(8) [:0,0,0,0 i:3,1,0,1 #(1):3,1,0,1 !(ExpectedMoreTokens):4,1,1,1")
PARSE_TEST("multi-line input error (4)", Standard, "[\r1,\n2\r\n", FINAL, UTF8, "u(8) [:0,0,0,0 i:2,1,0,1 #(1):2,1,0,1 i:5,2,0,1 #(2):5,2,0,1 !(ExpectedMoreTokens):8,3,0,1")
PARSE_TEST("multi-line input error (5)", Standard, "[\r\"x\n", FINAL, UTF8, "u(8) [:0,0,0,0 !(UnescapedControlCharacter):4,1,2,1")
PARSE_TEST("multi-line input error (6)", Standard, "[\n\"x\n", FINAL, UTF8, "u(8) [:0,0,0,0 !(UnescapedControlCharacter):4,1,2,1")
PARSE_TEST("multi-line input error (7)", Standard, "[\r\n\"x\r\n", FINAL, UTF8, "u(8) [:0,0,0,0 !(UnescapedControlCharacter):5,1,2,1")

};

static void TestParserParse()
{
    size_t i;
    for  (i = 0; i < sizeof(s_parseTests)/sizeof(s_parseTests[0]); i++)
    {
        RunParseTest(&s_parseTests[i]);
    }
}

typedef struct tag_WriterState
{
    JSON_Error error;
} WriterState;

static void InitWriterState(WriterState* pState)
{
    pState->error = JSON_Error_None;
}

static void GetWriterState(JSON_Writer writer, WriterState* pState)
{
    pState->error = JSON_Writer_GetError(writer);
}

static int WriterStatesAreIdentical(const WriterState* pState1, const WriterState* pState2)
{
    return (pState1->error == pState2->error);
}

static int CheckWriterState(JSON_Writer writer, const WriterState* pExpectedState)
{
    int isValid;
    WriterState actualState;
    GetWriterState(writer, &actualState);
    isValid = WriterStatesAreIdentical(pExpectedState, &actualState);
    if (!isValid)
    {
        printf("FAILURE: writer state does not match\n"
               "  STATE                                 EXPECTED     ACTUAL\n"
               "  JSON_Writer_GetError()                %8d   %8d\n"
               ,
               (int)pExpectedState->error, (int)actualState.error
            );
    }
    return isValid;
}

typedef struct tag_WriterSettings
{
    void*         userData;
    JSON_Encoding outputEncoding;
    JSON_Boolean  useCRLF;
    JSON_Boolean  replaceInvalidEncodingSequences;
} WriterSettings;

static void InitWriterSettings(WriterSettings* pSettings)
{
    pSettings->userData = NULL;
    pSettings->outputEncoding = JSON_UTF8;
    pSettings->useCRLF = JSON_False;
    pSettings->replaceInvalidEncodingSequences = JSON_False;
}

static void GetWriterSettings(JSON_Writer writer, WriterSettings* pSettings)
{
    pSettings->userData = JSON_Writer_GetUserData(writer);
    pSettings->outputEncoding = JSON_Writer_GetOutputEncoding(writer);
    pSettings->useCRLF = JSON_Writer_GetUseCRLF(writer);
    pSettings->replaceInvalidEncodingSequences = JSON_Writer_GetReplaceInvalidEncodingSequences(writer);
}

static int WriterSettingsAreIdentical(const WriterSettings* pSettings1, const WriterSettings* pSettings2)
{
    return (pSettings1->userData == pSettings2->userData &&
            pSettings1->outputEncoding == pSettings2->outputEncoding &&
            pSettings1->useCRLF == pSettings2->useCRLF &&
            pSettings1->replaceInvalidEncodingSequences == pSettings2->replaceInvalidEncodingSequences);
}

static int CheckWriterSettings(JSON_Writer writer, const WriterSettings* pExpectedSettings)
{
    int identical;
    WriterSettings actualSettings;
    GetWriterSettings(writer, &actualSettings);
    identical = WriterSettingsAreIdentical(pExpectedSettings, &actualSettings);
    if (!identical)
    {
        printf("FAILURE: writer settings do not match\n"
               "  SETTINGS                                         EXPECTED     ACTUAL\n"
               "  JSON_Writer_GetUserData()                        %8p   %8p\n"
               "  JSON_Writer_GetOutputEncoding()                  %8d   %8d\n"
               "  JSON_Writer_GetUseCRLF()                         %8d   %8d\n"
               "  JSON_Writer_GetReplaceInvalidEncodingSequences() %8d   %8d\n"
               ,
               pExpectedSettings->userData, actualSettings.userData,
               (int)pExpectedSettings->outputEncoding, (int)actualSettings.outputEncoding,
               (int)pExpectedSettings->useCRLF, (int)actualSettings.useCRLF,
               (int)pExpectedSettings->replaceInvalidEncodingSequences, (int)actualSettings.replaceInvalidEncodingSequences
            );
    }
    return identical;
}

typedef struct tag_WriterHandlers
{
    JSON_Writer_OutputHandler outputHandler;
} WriterHandlers;

static void InitWriterHandlers(WriterHandlers* pHandlers)
{
    pHandlers->outputHandler = NULL;
}

static void GetWriterHandlers(JSON_Writer writer, WriterHandlers* pHandlers)
{
    pHandlers->outputHandler = JSON_Writer_GetOutputHandler(writer);
}

static int WriterHandlersAreIdentical(const WriterHandlers* pHandlers1, const WriterHandlers* pHandlers2)
{
    return (pHandlers1->outputHandler == pHandlers2->outputHandler);
}

static int CheckWriterHandlers(JSON_Writer writer, const WriterHandlers* pExpectedHandlers)
{
    int identical;
    WriterHandlers actualHandlers;
    GetWriterHandlers(writer, &actualHandlers);
    identical = WriterHandlersAreIdentical(pExpectedHandlers, &actualHandlers);
    if (!identical)
    {
        printf("FAILURE: writer handlers do not match\n"
               "  HANDLERS                             EXPECTED     ACTUAL\n"
               "  JSON_Writer_GetOutputHandler()       %8s   %8s\n"
               ,
               HANDLER_STRING(pExpectedHandlers->outputHandler), HANDLER_STRING(actualHandlers.outputHandler)
            );
    }
    return identical;
}

static int CheckWriterHasDefaultValues(JSON_Writer writer)
{
    WriterState state;
    WriterSettings settings;
    WriterHandlers handlers;
    InitWriterState(&state);
    InitWriterSettings(&settings);
    InitWriterHandlers(&handlers);
    return CheckWriterState(writer, &state) &&
           CheckWriterSettings(writer, &settings) &&
           CheckWriterHandlers(writer, &handlers);
}

static int CheckWriterCreate(const JSON_MemorySuite* pMemorySuite, JSON_Status expectedStatus, JSON_Writer* pWriter)
{
    *pWriter = JSON_Writer_Create(pMemorySuite);
    if (expectedStatus == JSON_Success && !*pWriter)
    {
        printf("FAILURE: expected JSON_Writer_Create() to return a writer instance\n");
        return 0;
    }
    if (expectedStatus == JSON_Failure && *pWriter)
    {
        printf("FAILURE: expected JSON_Writer_Create() to return NULL\n");
        JSON_Writer_Free(*pWriter);
        *pWriter = NULL;
        return 0;
    }
    return 1;
}

static int CheckWriterCreateWithCustomMemorySuite(JSON_ReallocHandler r, JSON_FreeHandler f, JSON_Status expectedStatus, JSON_Writer* pWriter)
{
    JSON_MemorySuite memorySuite;
    memorySuite.userData = NULL;
    memorySuite.realloc = r;
    memorySuite.free = f;
    return CheckWriterCreate(&memorySuite, expectedStatus, pWriter);
}

static int CheckWriterReset(JSON_Writer writer, JSON_Status expectedStatus)
{
    if (JSON_Writer_Reset(writer) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_Reset() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterFree(JSON_Writer writer, JSON_Status expectedStatus)
{
    if (JSON_Writer_Free(writer) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_Free() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterSetUserData(JSON_Writer writer, void* userData, JSON_Status expectedStatus)
{
    if (JSON_Writer_SetUserData(writer, userData) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_SetUserData() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterSetOutputEncoding(JSON_Writer writer, JSON_Encoding encoding, JSON_Status expectedStatus)
{
    if (JSON_Writer_SetOutputEncoding(writer, encoding) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_SetOutputEncoding() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterSetUseCRLF(JSON_Writer writer, JSON_Boolean useCRLF, JSON_Status expectedStatus)
{
    if (JSON_Writer_SetUseCRLF(writer, useCRLF) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_SetUseCRLF() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterSetReplaceInvalidEncodingSequences(JSON_Writer writer, JSON_Boolean replaceInvalidEncodingSequences, JSON_Status expectedStatus)
{
    if (JSON_Writer_SetReplaceInvalidEncodingSequences(writer, replaceInvalidEncodingSequences) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_SetReplaceInvalidEncodingSequences() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterSetOutputHandler(JSON_Writer writer, JSON_Writer_OutputHandler handler, JSON_Status expectedStatus)
{
    if (JSON_Writer_SetOutputHandler(writer, handler) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_SetOutputHandler() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteNull(JSON_Writer writer, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteNull(writer) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteNull() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteBoolean(JSON_Writer writer, JSON_Boolean value, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteBoolean(writer, value) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteBoolean() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteString(JSON_Writer writer, const char* pBytes, size_t length, JSON_Encoding encoding, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteString(writer, pBytes, length, encoding) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteString() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteNumber(JSON_Writer writer, const char* pValue, size_t length, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteNumber(writer, pValue, length) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteNumber() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteSpecialNumber(JSON_Writer writer, JSON_SpecialNumber value, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteSpecialNumber(writer, value) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteSpecialNumber() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteStartObject(JSON_Writer writer, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteStartObject(writer) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteStartObject() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteEndObject(JSON_Writer writer, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteEndObject(writer) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteEndObject() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteStartArray(JSON_Writer writer, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteStartArray(writer) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteStartArray() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteEndArray(JSON_Writer writer, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteEndArray(writer) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteEndArray() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteColon(JSON_Writer writer, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteColon(writer) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteColon() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteComma(JSON_Writer writer, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteComma(writer) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteComma() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteSpace(JSON_Writer writer, size_t numberOfSpaces, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteSpace(writer, numberOfSpaces) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteSpace() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int CheckWriterWriteNewLine(JSON_Writer writer, JSON_Status expectedStatus)
{
    if (JSON_Writer_WriteNewLine(writer) != expectedStatus)
    {
        printf("FAILURE: expected JSON_Writer_WriteNewLine() to return %s\n", (expectedStatus == JSON_Success) ? "JSON_Success" : "JSON_Failure");
        return 0;
    }
    return 1;
}

static int TryToMisbehaveInWriteHandler(JSON_Writer writer)
{
    if (!CheckWriterFree(writer, JSON_Failure) ||
        !CheckWriterReset(writer, JSON_Failure) ||
        !CheckWriterSetOutputEncoding(writer, JSON_UTF32LE, JSON_Failure) ||
        !CheckWriterSetUseCRLF(writer, JSON_True, JSON_Failure) ||
        !CheckWriterSetReplaceInvalidEncodingSequences(writer, JSON_True, JSON_Failure) ||
        !CheckWriterWriteNull(writer, JSON_Failure) ||
        !CheckWriterWriteBoolean(writer, JSON_True, JSON_Failure) ||
        !CheckWriterWriteString(writer, "abc", 3, JSON_UTF8, JSON_Failure) ||
        !CheckWriterWriteNumber(writer, "0", 1, JSON_Failure) ||
        !CheckWriterWriteSpecialNumber(writer, JSON_NaN, JSON_Failure) ||
        !CheckWriterWriteStartObject(writer, JSON_Failure) ||
        !CheckWriterWriteEndObject(writer, JSON_Failure) ||
        !CheckWriterWriteStartArray(writer, JSON_Failure) ||
        !CheckWriterWriteEndArray(writer, JSON_Failure) ||
        !CheckWriterWriteColon(writer, JSON_Failure) ||
        !CheckWriterWriteComma(writer, JSON_Failure) ||
        !CheckWriterWriteSpace(writer, 3, JSON_Failure) ||
        !CheckWriterWriteNewLine(writer, JSON_Failure))
    {
        return 1;
    }
    return 0;
}

static JSON_Writer_HandlerResult JSON_CALL OutputHandler(JSON_Writer writer, const char* pBytes, size_t length)
{
    if (s_failHandler)
    {
        return JSON_Writer_Abort;
    }
    if (s_misbehaveInHandler && TryToMisbehaveInWriteHandler(writer))
    {
        return JSON_Writer_Abort;
    }
    OutputSeparator();
    OutputStringBytes(pBytes, length, JSON_SimpleString);
    return JSON_Writer_Continue;
}

static void TestWriterCreate()
{
    JSON_Writer writer = NULL;
    printf("Test creating writer ... ");
    if (CheckWriterCreate(NULL, JSON_Success, &writer) &&
        CheckWriterHasDefaultValues(writer))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
}

static void TestWriterCreateWithCustomMemorySuite()
{
    JSON_Writer writer = NULL;
    printf("Test creating writer with custom memory suite ... ");
    if (CheckWriterCreateWithCustomMemorySuite(NULL, NULL, JSON_Failure, &writer) &&
        CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, NULL, JSON_Failure, &writer) &&
        CheckWriterCreateWithCustomMemorySuite(NULL, &FreeHandler, JSON_Failure, &writer) &&
        CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterHasDefaultValues(writer))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
}

static void TestWriterCreateMallocFailure()
{
    JSON_Writer writer = NULL;
    printf("Test creating writer malloc failure ... ");
    s_failMalloc = 1;
    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Failure, &writer))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    s_failMalloc = 0;
    JSON_Writer_Free(writer);
}

static void TestWriterMissing()
{
    WriterState state;
    WriterSettings settings;
    WriterHandlers handlers;
    printf("Test NULL writer instance ... ");
    InitWriterState(&state);
    InitWriterSettings(&settings);
    InitWriterHandlers(&handlers);
    if (CheckWriterState(NULL, &state) &&
        CheckWriterSettings(NULL, &settings) &&
        CheckWriterHandlers(NULL, &handlers) &&
        CheckWriterFree(NULL, JSON_Failure) &&
        CheckWriterReset(NULL, JSON_Failure) &&
        CheckWriterSetUserData(NULL, (void*)1, JSON_Failure) &&
        CheckWriterSetOutputEncoding(NULL, JSON_UTF16LE, JSON_Failure) &&
        CheckWriterSetOutputHandler(NULL, &OutputHandler, JSON_Failure) &&
        CheckWriterWriteNull(NULL, JSON_Failure) &&
        CheckWriterWriteBoolean(NULL, JSON_True, JSON_Failure) &&
        CheckWriterWriteString(NULL, "abc", 3, JSON_UTF8, JSON_Failure) &&
        CheckWriterWriteNumber(NULL, "0", 1, JSON_Failure) &&
        CheckWriterWriteSpecialNumber(NULL, JSON_NaN, JSON_Failure) &&
        CheckWriterWriteStartObject(NULL, JSON_Failure) &&
        CheckWriterWriteEndObject(NULL, JSON_Failure) &&
        CheckWriterWriteStartArray(NULL, JSON_Failure) &&
        CheckWriterWriteEndArray(NULL, JSON_Failure) &&
        CheckWriterWriteColon(NULL, JSON_Failure) &&
        CheckWriterWriteComma(NULL, JSON_Failure) &&
        CheckWriterWriteSpace(NULL, 3, JSON_Failure) &&
        CheckWriterWriteNewLine(NULL, JSON_Failure))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
}

static void TestWriterSetSettings()
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    printf("Test setting writer settings ... ");
    InitWriterSettings(&settings);
    settings.userData = (void*)1;
    settings.outputEncoding = JSON_UTF16LE;
    settings.replaceInvalidEncodingSequences = JSON_True;
    if (CheckWriterCreate(NULL, JSON_Success, &writer) &&
        CheckWriterSetUserData(writer, settings.userData, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, settings.outputEncoding, JSON_Success) &&
        CheckWriterSetUseCRLF(writer, settings.useCRLF, JSON_Success) &&
        CheckWriterSetReplaceInvalidEncodingSequences(writer, settings.replaceInvalidEncodingSequences, JSON_Success) &&
        CheckWriterSettings(writer, &settings))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
}

static void TestWriterSetInvalidSettings()
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    printf("Test setting invalid writer settings ... ");
    InitWriterSettings(&settings);
    if (CheckWriterCreate(NULL, JSON_Success, &writer) &&
        CheckWriterSetOutputEncoding(writer, (JSON_Encoding)-1, JSON_Failure) &&
        CheckWriterSetOutputEncoding(writer, JSON_UnknownEncoding, JSON_Failure) &&
        CheckWriterSetOutputEncoding(writer, (JSON_Encoding)(JSON_UTF32BE + 1), JSON_Failure) &&
        CheckWriterWriteString(writer, NULL, 1, JSON_UTF8, JSON_Failure) &&
        CheckWriterWriteString(writer, "a", 1, JSON_UnknownEncoding, JSON_Failure) &&
        CheckWriterWriteString(writer, "a", 1, (JSON_Encoding)(JSON_UTF32BE + 1), JSON_Failure) &&
        CheckWriterWriteNumber(writer, NULL, 1, JSON_Failure) &&
        CheckWriterSettings(writer, &settings))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
}

static void TestWriterSetHandlers()
{
    JSON_Writer writer = NULL;
    WriterHandlers handlers;
    printf("Test setting writer handlers ... ");
    InitWriterHandlers(&handlers);
    handlers.outputHandler = &OutputHandler;
    if (CheckWriterCreate(NULL, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, handlers.outputHandler, JSON_Success) &&
        CheckWriterHandlers(writer, &handlers))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
}

static void TestWriterReset()
{
    JSON_Writer writer = NULL;
    WriterState state;
    WriterSettings settings;
    WriterHandlers handlers;
    printf("Test resetting writer ... ");
    InitWriterState(&state);
    InitWriterSettings(&settings);
    InitWriterHandlers(&handlers);
    if (CheckWriterCreate(NULL, JSON_Success, &writer) &&
        CheckWriterSetUserData(writer, (void*)1, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, JSON_UTF16LE, JSON_Success) &&
        CheckWriterSetUseCRLF(writer, JSON_True, JSON_Success) &&
        CheckWriterSetReplaceInvalidEncodingSequences(writer, JSON_True, JSON_Success) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterWriteNull(writer, JSON_Success) &&
        CheckWriterReset(writer, JSON_Success) &&
        CheckWriterState(writer, &state) &&
        CheckWriterSettings(writer, &settings) &&
        CheckWriterHandlers(writer, &handlers))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
}

static void TestWriterMisbehaveInCallbacks()
{
    JSON_Writer writer = NULL;
    printf("Test writer misbehaving in callbacks ... ");
    s_misbehaveInHandler = 1;
    if (CheckWriterCreate(NULL, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterWriteNull(writer, JSON_Success))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    s_misbehaveInHandler = 0;
    JSON_Writer_Free(writer);
}

static void TestWriterAbortInCallbacks()
{
    JSON_Writer writer = NULL;
    WriterState state;
    printf("Test writer aborting in callbacks ... ");
    InitWriterState(&state);
    state.error = JSON_Error_AbortedByHandler;
    s_failHandler = 1;
    if (CheckWriterCreate(NULL, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterWriteNull(writer, JSON_Failure) &&
        CheckWriterState(writer, &state))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    s_failHandler = 0;
    JSON_Writer_Free(writer);
}

static void TestWriterStackMallocFailure()
{
    int succeeded = 0;
    JSON_Writer writer = NULL;
    WriterState state;
    printf("Test writer stack malloc failure ... ");
    InitWriterState(&state);
    state.error = JSON_Error_OutOfMemory;
    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer))
    {
        s_failMalloc = 1;
        for (;;)
        {
            if (JSON_Writer_WriteStartArray(writer) == JSON_Failure)
            {
                break;
            }
        }
        if (CheckWriterState(writer, &state))
        {
            succeeded = 1;
        }
        s_failMalloc = 0;
    }
    if (succeeded)
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
}

static void TestWriterStackReallocFailure()
{
    int succeeded = 0;
    JSON_Writer writer = NULL;
    WriterState state;
    printf("Test writer stack realloc failure ... ");
    InitWriterState(&state);
    state.error = JSON_Error_OutOfMemory;
    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer))
    {
        s_failRealloc = 1;
        for (;;)
        {
            if (JSON_Writer_WriteStartArray(writer) == JSON_Failure)
            {
                break;
            }
        }
        if (CheckWriterState(writer, &state))
        {
            succeeded = 1;
        }
        s_failRealloc = 0;
    }
    if (succeeded)
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
}

typedef struct tag_WriteTest
{
    const char*   pName;
    JSON_Encoding inputEncoding;
    JSON_Encoding outputEncoding;
    JSON_Boolean  replaceInvalidEncodingSequences;
    const char*   pInput;
    size_t        length; /* overloaded for boolean, specialnumber */
    const char*   pOutput;
} WriteTest;

#define REPLACE JSON_True
#define NO_REPLACE JSON_False

#define WRITE_NULL_TEST(name, out_enc, output) { name, JSON_UnknownEncoding, JSON_##out_enc, NO_REPLACE, NULL, 0, output },

static void RunWriteNullTest(const WriteTest* pTest)
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    WriterState state;
    printf("Test writing null %s ... ", pTest->pName);

    InitWriterSettings(&settings);
    settings.outputEncoding = pTest->outputEncoding;

    InitWriterState(&state);
    ResetOutput();

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, settings.outputEncoding, JSON_Success))
    {
        if (JSON_Writer_WriteNull(writer) != JSON_Success)
        {
            state.error = JSON_Writer_GetError(writer);
            if (state.error != JSON_Error_None)
            {
                OutputSeparator();
                OutputFormatted("!(%s)", errorNames[state.error]);
            }
        }
        if (CheckWriterState(writer, &state) && CheckOutput(pTest->pOutput))
        {
            printf("OK\n");
        }
        else
        {
            s_failureCount++;
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
    ResetOutput();
}

static const WriteTest s_writeNullTests[] =
{

WRITE_NULL_TEST("-> UTF-8",    UTF8,    "6E 75 6C 6C")
WRITE_NULL_TEST("-> UTF-16LE", UTF16LE, "6E 00 75 00 6C 00 6C 00")
WRITE_NULL_TEST("-> UTF-16BE", UTF16BE, "00 6E 00 75 00 6C 00 6C")
WRITE_NULL_TEST("-> UTF-32LE", UTF32LE, "6E 00 00 00 75 00 00 00 6C 00 00 00 6C 00 00 00")
WRITE_NULL_TEST("-> UTF-32BE", UTF32BE, "00 00 00 6E 00 00 00 75 00 00 00 6C 00 00 00 6C")

};

static void TestWriterWriteNull()
{
    size_t i;
    for  (i = 0; i < sizeof(s_writeNullTests)/sizeof(s_writeNullTests[0]); i++)
    {
        RunWriteNullTest(&s_writeNullTests[i]);
    }
}

#define WRITE_BOOLEAN_TEST(name, out_enc, input, output) { name, JSON_UnknownEncoding, JSON_##out_enc, NO_REPLACE, NULL, (size_t)input, output },

static void RunWriteBooleanTest(const WriteTest* pTest)
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    WriterState state;
    printf("Test writing boolean %s ... ", pTest->pName);

    InitWriterSettings(&settings);
    settings.outputEncoding = pTest->outputEncoding;

    InitWriterState(&state);
    ResetOutput();

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, settings.outputEncoding, JSON_Success))
    {
        if (JSON_Writer_WriteBoolean(writer, (JSON_Boolean)pTest->length) != JSON_Success)
        {
            state.error = JSON_Writer_GetError(writer);
            if (state.error != JSON_Error_None)
            {
                OutputSeparator();
                OutputFormatted("!(%s)", errorNames[state.error]);
            }
        }
        if (CheckWriterState(writer, &state) && CheckOutput(pTest->pOutput))
        {
            printf("OK\n");
        }
        else
        {
            s_failureCount++;
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
    ResetOutput();
}

static const WriteTest s_writeBooleanTests[] =
{

WRITE_BOOLEAN_TEST("true -> UTF-8",     UTF8,    JSON_True, "74 72 75 65")
WRITE_BOOLEAN_TEST("true -> UTF-16LE",  UTF16LE, JSON_True, "74 00 72 00 75 00 65 00")
WRITE_BOOLEAN_TEST("true -> UTF-16BE",  UTF16BE, JSON_True, "00 74 00 72 00 75 00 65")
WRITE_BOOLEAN_TEST("true -> UTF-32LE",  UTF32LE, JSON_True, "74 00 00 00 72 00 00 00 75 00 00 00 65 00 00 00")
WRITE_BOOLEAN_TEST("true -> UTF-32BE",  UTF32BE, JSON_True, "00 00 00 74 00 00 00 72 00 00 00 75 00 00 00 65")
WRITE_BOOLEAN_TEST("false -> UTF-8",    UTF8,    JSON_False, "66 61 6C 73 65")
WRITE_BOOLEAN_TEST("false -> UTF-16LE", UTF16LE, JSON_False, "66 00 61 00 6C 00 73 00 65 00")
WRITE_BOOLEAN_TEST("false -> UTF-16BE", UTF16BE, JSON_False, "00 66 00 61 00 6C 00 73 00 65")
WRITE_BOOLEAN_TEST("false -> UTF-32LE", UTF32LE, JSON_False, "66 00 00 00 61 00 00 00 6C 00 00 00 73 00 00 00 65 00 00 00")
WRITE_BOOLEAN_TEST("false -> UTF-32BE", UTF32BE, JSON_False, "00 00 00 66 00 00 00 61 00 00 00 6C 00 00 00 73 00 00 00 65")

};

static void TestWriterWriteBoolean()
{
    size_t i;
    for  (i = 0; i < sizeof(s_writeBooleanTests)/sizeof(s_writeBooleanTests[0]); i++)
    {
        RunWriteBooleanTest(&s_writeBooleanTests[i]);
    }
}

static void RunWriteStringTest(const WriteTest* pTest)
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    WriterState state;
    printf("Test writing string %s ... ", pTest->pName);

    InitWriterSettings(&settings);
    settings.outputEncoding = pTest->outputEncoding;
    settings.replaceInvalidEncodingSequences = pTest->replaceInvalidEncodingSequences;

    InitWriterState(&state);
    ResetOutput();

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, settings.outputEncoding, JSON_Success) &&
        CheckWriterSetReplaceInvalidEncodingSequences(writer, settings.replaceInvalidEncodingSequences, JSON_Success))
    {
        if (JSON_Writer_WriteString(writer, pTest->pInput, pTest->length, pTest->inputEncoding) != JSON_Success)
        {
            state.error = JSON_Writer_GetError(writer);
            if (state.error != JSON_Error_None)
            {
                OutputSeparator();
                OutputFormatted("!(%s)", errorNames[state.error]);
            }
        }
        if (CheckWriterState(writer, &state) && CheckOutput(pTest->pOutput))
        {
            printf("OK\n");
        }
        else
        {
            s_failureCount++;
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
    ResetOutput();
}

#define WRITE_STRING_TEST(name, in_enc, out_enc, replace, input, output) { name, JSON_##in_enc, JSON_##out_enc, replace, input, sizeof(input) - 1, output },

static const WriteTest s_writeStringTests[] =
{

WRITE_STRING_TEST("empty string UTF-8 -> UTF-8",       UTF8,    UTF8,    NO_REPLACE, "", "22 22")
WRITE_STRING_TEST("empty string UTF-8 -> UTF-16LE",    UTF8,    UTF16LE, NO_REPLACE, "", "22 00 22 00")
WRITE_STRING_TEST("empty string UTF-8 -> UTF-16BE",    UTF8,    UTF16BE, NO_REPLACE, "", "00 22 00 22")
WRITE_STRING_TEST("empty string UTF-8 -> UTF-32LE",    UTF8,    UTF32LE, NO_REPLACE, "", "22 00 00 00 22 00 00 00")
WRITE_STRING_TEST("empty string UTF-8 -> UTF-32BE",    UTF8,    UTF32BE, NO_REPLACE, "", "00 00 00 22 00 00 00 22")
WRITE_STRING_TEST("empty string UTF-16LE -> UTF-8",    UTF16LE, UTF8,    NO_REPLACE, "", "22 22")
WRITE_STRING_TEST("empty string UTF-16LE -> UTF-16LE", UTF16LE, UTF16LE, NO_REPLACE, "", "22 00 22 00")
WRITE_STRING_TEST("empty string UTF-16LE -> UTF-16BE", UTF16LE, UTF16BE, NO_REPLACE, "", "00 22 00 22")
WRITE_STRING_TEST("empty string UTF-16LE -> UTF-32LE", UTF16LE, UTF32LE, NO_REPLACE, "", "22 00 00 00 22 00 00 00")
WRITE_STRING_TEST("empty string UTF-16LE -> UTF-32BE", UTF16LE, UTF32BE, NO_REPLACE, "", "00 00 00 22 00 00 00 22")
WRITE_STRING_TEST("empty string UTF-16BE -> UTF-8",    UTF16BE, UTF8,    NO_REPLACE, "", "22 22")
WRITE_STRING_TEST("empty string UTF-16BE -> UTF-16LE", UTF16BE, UTF16LE, NO_REPLACE, "", "22 00 22 00")
WRITE_STRING_TEST("empty string UTF-16BE -> UTF-16BE", UTF16BE, UTF16BE, NO_REPLACE, "", "00 22 00 22")
WRITE_STRING_TEST("empty string UTF-16BE -> UTF-32LE", UTF16BE, UTF32LE, NO_REPLACE, "", "22 00 00 00 22 00 00 00")
WRITE_STRING_TEST("empty string UTF-16BE -> UTF-32BE", UTF16BE, UTF32BE, NO_REPLACE, "", "00 00 00 22 00 00 00 22")
WRITE_STRING_TEST("empty string UTF-32LE -> UTF-8",    UTF32LE, UTF8,    NO_REPLACE, "", "22 22")
WRITE_STRING_TEST("empty string UTF-32LE -> UTF-16LE", UTF32LE, UTF16LE, NO_REPLACE, "", "22 00 22 00")
WRITE_STRING_TEST("empty string UTF-32LE -> UTF-16BE", UTF32LE, UTF16BE, NO_REPLACE, "", "00 22 00 22")
WRITE_STRING_TEST("empty string UTF-32LE -> UTF-32LE", UTF32LE, UTF32LE, NO_REPLACE, "", "22 00 00 00 22 00 00 00")
WRITE_STRING_TEST("empty string UTF-32LE -> UTF-32BE", UTF32LE, UTF32BE, NO_REPLACE, "", "00 00 00 22 00 00 00 22")
WRITE_STRING_TEST("empty string UTF-32BE -> UTF-8",    UTF32BE, UTF8,    NO_REPLACE, "", "22 22")
WRITE_STRING_TEST("empty string UTF-32BE -> UTF-16LE", UTF32BE, UTF16LE, NO_REPLACE, "", "22 00 22 00")
WRITE_STRING_TEST("empty string UTF-32BE -> UTF-16BE", UTF32BE, UTF16BE, NO_REPLACE, "", "00 22 00 22")
WRITE_STRING_TEST("empty string UTF-32BE -> UTF-32LE", UTF32BE, UTF32LE, NO_REPLACE, "", "22 00 00 00 22 00 00 00")
WRITE_STRING_TEST("empty string UTF-32BE -> UTF-32BE", UTF32BE, UTF32BE, NO_REPLACE, "", "00 00 00 22 00 00 00 22")

WRITE_STRING_TEST("UTF-8 -> UTF-8",       UTF8,    UTF8,    NO_REPLACE, "\x61\xC2\xA9\xE4\xB8\x81\xF0\x9F\x80\x84", "22 61 C2 A9 E4 B8 81 F0 9F 80 84 22")
WRITE_STRING_TEST("UTF-8 -> UTF-16LE",    UTF8,    UTF16LE, NO_REPLACE, "\x61\xC2\xA9\xE4\xB8\x81\xF0\x9F\x80\x84", "22 00 61 00 A9 00 01 4E 3C D8 04 DC 22 00")
WRITE_STRING_TEST("UTF-8 -> UTF-16BE",    UTF8,    UTF16BE, NO_REPLACE, "\x61\xC2\xA9\xE4\xB8\x81\xF0\x9F\x80\x84", "00 22 00 61 00 A9 4E 01 D8 3C DC 04 00 22")
WRITE_STRING_TEST("UTF-8 -> UTF-32LE",    UTF8,    UTF32LE, NO_REPLACE, "\x61\xC2\xA9\xE4\xB8\x81\xF0\x9F\x80\x84", "22 00 00 00 61 00 00 00 A9 00 00 00 01 4E 00 00 04 F0 01 00 22 00 00 00")
WRITE_STRING_TEST("UTF-8 -> UTF-32BE",    UTF8,    UTF32BE, NO_REPLACE, "\x61\xC2\xA9\xE4\xB8\x81\xF0\x9F\x80\x84", "00 00 00 22 00 00 00 61 00 00 00 A9 00 00 4E 01 00 01 F0 04 00 00 00 22")
WRITE_STRING_TEST("UTF-16LE -> UTF-8",    UTF16LE, UTF8,    NO_REPLACE, "\x61\x00\xA9\x00\x01\x4E\x3C\xD8\x04\xDC", "22 61 C2 A9 E4 B8 81 F0 9F 80 84 22")
WRITE_STRING_TEST("UTF-16LE -> UTF-16LE", UTF16LE, UTF16LE, NO_REPLACE, "\x61\x00\xA9\x00\x01\x4E\x3C\xD8\x04\xDC", "22 00 61 00 A9 00 01 4E 3C D8 04 DC 22 00")
WRITE_STRING_TEST("UTF-16LE -> UTF-16BE", UTF16LE, UTF16BE, NO_REPLACE, "\x61\x00\xA9\x00\x01\x4E\x3C\xD8\x04\xDC", "00 22 00 61 00 A9 4E 01 D8 3C DC 04 00 22")
WRITE_STRING_TEST("UTF-16LE -> UTF-32LE", UTF16LE, UTF32LE, NO_REPLACE, "\x61\x00\xA9\x00\x01\x4E\x3C\xD8\x04\xDC", "22 00 00 00 61 00 00 00 A9 00 00 00 01 4E 00 00 04 F0 01 00 22 00 00 00")
WRITE_STRING_TEST("UTF-16LE -> UTF-32BE", UTF16LE, UTF32BE, NO_REPLACE, "\x61\x00\xA9\x00\x01\x4E\x3C\xD8\x04\xDC", "00 00 00 22 00 00 00 61 00 00 00 A9 00 00 4E 01 00 01 F0 04 00 00 00 22")
WRITE_STRING_TEST("UTF-16BE -> UTF-8",    UTF16BE, UTF8,    NO_REPLACE, "\x00\x61\x00\xA9\x4E\x01\xD8\x3C\xDC\x04", "22 61 C2 A9 E4 B8 81 F0 9F 80 84 22")
WRITE_STRING_TEST("UTF-16BE -> UTF-16LE", UTF16BE, UTF16LE, NO_REPLACE, "\x00\x61\x00\xA9\x4E\x01\xD8\x3C\xDC\x04", "22 00 61 00 A9 00 01 4E 3C D8 04 DC 22 00")
WRITE_STRING_TEST("UTF-16BE -> UTF-16BE", UTF16BE, UTF16BE, NO_REPLACE, "\x00\x61\x00\xA9\x4E\x01\xD8\x3C\xDC\x04", "00 22 00 61 00 A9 4E 01 D8 3C DC 04 00 22")
WRITE_STRING_TEST("UTF-16BE -> UTF-32LE", UTF16BE, UTF32LE, NO_REPLACE, "\x00\x61\x00\xA9\x4E\x01\xD8\x3C\xDC\x04", "22 00 00 00 61 00 00 00 A9 00 00 00 01 4E 00 00 04 F0 01 00 22 00 00 00")
WRITE_STRING_TEST("UTF-16BE -> UTF-32BE", UTF16BE, UTF32BE, NO_REPLACE, "\x00\x61\x00\xA9\x4E\x01\xD8\x3C\xDC\x04", "00 00 00 22 00 00 00 61 00 00 00 A9 00 00 4E 01 00 01 F0 04 00 00 00 22")
WRITE_STRING_TEST("UTF-32LE -> UTF-8",    UTF32LE, UTF8,    NO_REPLACE, "\x61\x00\x00\x00\xA9\x00\x00\x00\x01\x4E\x00\x00\x04\xF0\x01\x00", "22 61 C2 A9 E4 B8 81 F0 9F 80 84 22")
WRITE_STRING_TEST("UTF-32LE -> UTF-16LE", UTF32LE, UTF16LE, NO_REPLACE, "\x61\x00\x00\x00\xA9\x00\x00\x00\x01\x4E\x00\x00\x04\xF0\x01\x00", "22 00 61 00 A9 00 01 4E 3C D8 04 DC 22 00")
WRITE_STRING_TEST("UTF-32LE -> UTF-16BE", UTF32LE, UTF16BE, NO_REPLACE, "\x61\x00\x00\x00\xA9\x00\x00\x00\x01\x4E\x00\x00\x04\xF0\x01\x00", "00 22 00 61 00 A9 4E 01 D8 3C DC 04 00 22")
WRITE_STRING_TEST("UTF-32LE -> UTF-32LE", UTF32LE, UTF32LE, NO_REPLACE, "\x61\x00\x00\x00\xA9\x00\x00\x00\x01\x4E\x00\x00\x04\xF0\x01\x00", "22 00 00 00 61 00 00 00 A9 00 00 00 01 4E 00 00 04 F0 01 00 22 00 00 00")
WRITE_STRING_TEST("UTF-32LE -> UTF-32BE", UTF32LE, UTF32BE, NO_REPLACE, "\x61\x00\x00\x00\xA9\x00\x00\x00\x01\x4E\x00\x00\x04\xF0\x01\x00", "00 00 00 22 00 00 00 61 00 00 00 A9 00 00 4E 01 00 01 F0 04 00 00 00 22")
WRITE_STRING_TEST("UTF-32BE -> UTF-8",    UTF32BE, UTF8,    NO_REPLACE, "\x00\x00\x00\x61\x00\x00\x00\xA9\x00\x00\x4E\x01\x00\x01\xF0\x04", "22 61 C2 A9 E4 B8 81 F0 9F 80 84 22")
WRITE_STRING_TEST("UTF-32BE -> UTF-16LE", UTF32BE, UTF16LE, NO_REPLACE, "\x00\x00\x00\x61\x00\x00\x00\xA9\x00\x00\x4E\x01\x00\x01\xF0\x04", "22 00 61 00 A9 00 01 4E 3C D8 04 DC 22 00")
WRITE_STRING_TEST("UTF-32BE -> UTF-16BE", UTF32BE, UTF16BE, NO_REPLACE, "\x00\x00\x00\x61\x00\x00\x00\xA9\x00\x00\x4E\x01\x00\x01\xF0\x04", "00 22 00 61 00 A9 4E 01 D8 3C DC 04 00 22")
WRITE_STRING_TEST("UTF-32BE -> UTF-32LE", UTF32BE, UTF32LE, NO_REPLACE, "\x00\x00\x00\x61\x00\x00\x00\xA9\x00\x00\x4E\x01\x00\x01\xF0\x04", "22 00 00 00 61 00 00 00 A9 00 00 00 01 4E 00 00 04 F0 01 00 22 00 00 00")
WRITE_STRING_TEST("UTF-32BE -> UTF-32BE", UTF32BE, UTF32BE, NO_REPLACE, "\x00\x00\x00\x61\x00\x00\x00\xA9\x00\x00\x4E\x01\x00\x01\xF0\x04", "00 00 00 22 00 00 00 61 00 00 00 A9 00 00 4E 01 00 01 F0 04 00 00 00 22")

/* escape sequences */

WRITE_STRING_TEST("simple escape sequences -> UTF-8",    UTF8, UTF8,    NO_REPLACE, "\\" "\"" "/" "\t" "\n" "\r" "\f" "\b", "22 5C 5C 5C 22 5C 2F 5C 74 5C 6E 5C 72 5C 66 5C 62 22")
WRITE_STRING_TEST("simple escape sequences -> UTF-16LE", UTF8, UTF16LE, NO_REPLACE, "\\" "\"" "/" "\t" "\n" "\r" "\f" "\b", "22 00 5C 00 5C 00 5C 00 22 00 5C 00 2F 00 5C 00 74 00 5C 00 6E 00 5C 00 72 00 5C 00 66 00 5C 00 62 00 22 00")
WRITE_STRING_TEST("simple escape sequences -> UTF-16BE", UTF8, UTF16BE, NO_REPLACE, "\\" "\"" "/" "\t" "\n" "\r" "\f" "\b", "00 22 00 5C 00 5C 00 5C 00 22 00 5C 00 2F 00 5C 00 74 00 5C 00 6E 00 5C 00 72 00 5C 00 66 00 5C 00 62 00 22")
WRITE_STRING_TEST("simple escape sequences -> UTF-32LE", UTF8, UTF32LE, NO_REPLACE, "\\" "\"" "/" "\t" "\n" "\r" "\f" "\b", "22 00 00 00 5C 00 00 00 5C 00 00 00 5C 00 00 00 22 00 00 00 5C 00 00 00 2F 00 00 00 5C 00 00 00 74 00 00 00 5C 00 00 00 6E 00 00 00 5C 00 00 00 72 00 00 00 5C 00 00 00 66 00 00 00 5C 00 00 00 62 00 00 00 22 00 00 00")
WRITE_STRING_TEST("simple escape sequences -> UTF-32BE", UTF8, UTF32BE, NO_REPLACE, "\\" "\"" "/" "\t" "\n" "\r" "\f" "\b", "00 00 00 22 00 00 00 5C 00 00 00 5C 00 00 00 5C 00 00 00 22 00 00 00 5C 00 00 00 2F 00 00 00 5C 00 00 00 74 00 00 00 5C 00 00 00 6E 00 00 00 5C 00 00 00 72 00 00 00 5C 00 00 00 66 00 00 00 5C 00 00 00 62 00 00 00 22")

WRITE_STRING_TEST("unprintable ASCII characters hex escape sequences -> UTF-8",    UTF8, UTF8,    NO_REPLACE, "\x00\x1F\x7F", "22 5C 75 30 30 30 30 5C 75 30 30 31 46 5C 75 30 30 37 46 22")
WRITE_STRING_TEST("unprintable ASCII characters hex escape sequences -> UTF-16LE", UTF8, UTF16LE, NO_REPLACE, "\x00\x1F\x7F", "22 00 5C 00 75 00 30 00 30 00 30 00 30 00 5C 00 75 00 30 00 30 00 31 00 46 00 5C 00 75 00 30 00 30 00 37 00 46 00 22 00")
WRITE_STRING_TEST("unprintable ASCII characters hex escape sequences -> UTF-16BE", UTF8, UTF16BE, NO_REPLACE, "\x00\x1F\x7F", "00 22 00 5C 00 75 00 30 00 30 00 30 00 30 00 5C 00 75 00 30 00 30 00 31 00 46 00 5C 00 75 00 30 00 30 00 37 00 46 00 22")
WRITE_STRING_TEST("unprintable ASCII characters hex escape sequences -> UTF-32LE", UTF8, UTF32LE, NO_REPLACE, "\x00\x1F\x7F", "22 00 00 00 5C 00 00 00 75 00 00 00 30 00 00 00 30 00 00 00 30 00 00 00 30 00 00 00 5C 00 00 00 75 00 00 00 30 00 00 00 30 00 00 00 31 00 00 00 46 00 00 00 5C 00 00 00 75 00 00 00 30 00 00 00 30 00 00 00 37 00 00 00 46 00 00 00 22 00 00 00")
WRITE_STRING_TEST("unprintable ASCII characters hex escape sequences -> UTF-32BE", UTF8, UTF32BE, NO_REPLACE, "\x00\x1F\x7F", "00 00 00 22 00 00 00 5C 00 00 00 75 00 00 00 30 00 00 00 30 00 00 00 30 00 00 00 30 00 00 00 5C 00 00 00 75 00 00 00 30 00 00 00 30 00 00 00 31 00 00 00 46 00 00 00 5C 00 00 00 75 00 00 00 30 00 00 00 30 00 00 00 37 00 00 00 46 00 00 00 22")

WRITE_STRING_TEST("BMP noncharacter hex escape sequences -> UTF-8",    UTF16BE, UTF8,    NO_REPLACE, "\x00\xFE\x00\xFF\xFF\xFE\xFF\xFF", "22 5C 75 30 30 46 45 5C 75 30 30 46 46 5C 75 46 46 46 45 5C 75 46 46 46 46 22")
WRITE_STRING_TEST("BMP noncharacter hex escape sequences -> UTF-16LE", UTF16BE, UTF16LE, NO_REPLACE, "\x00\xFE\x00\xFF\xFF\xFE\xFF\xFF", "22 00 5C 00 75 00 30 00 30 00 46 00 45 00 5C 00 75 00 30 00 30 00 46 00 46 00 5C 00 75 00 46 00 46 00 46 00 45 00 5C 00 75 00 46 00 46 00 46 00 46 00 22 00")
WRITE_STRING_TEST("BMP noncharacter hex escape sequences -> UTF-16BE", UTF16BE, UTF16BE, NO_REPLACE, "\x00\xFE\x00\xFF\xFF\xFE\xFF\xFF", "00 22 00 5C 00 75 00 30 00 30 00 46 00 45 00 5C 00 75 00 30 00 30 00 46 00 46 00 5C 00 75 00 46 00 46 00 46 00 45 00 5C 00 75 00 46 00 46 00 46 00 46 00 22")
WRITE_STRING_TEST("BMP noncharacter hex escape sequences -> UTF-32LE", UTF16BE, UTF32LE, NO_REPLACE, "\x00\xFE\x00\xFF\xFF\xFE\xFF\xFF", "22 00 00 00 5C 00 00 00 75 00 00 00 30 00 00 00 30 00 00 00 46 00 00 00 45 00 00 00 5C 00 00 00 75 00 00 00 30 00 00 00 30 00 00 00 46 00 00 00 46 00 00 00 5C 00 00 00 75 00 00 00 46 00 00 00 46 00 00 00 46 00 00 00 45 00 00 00 5C 00 00 00 75 00 00 00 46 00 00 00 46 00 00 00 46 00 00 00 46 00 00 00 22 00 00 00")
WRITE_STRING_TEST("BMP noncharacter hex escape sequences -> UTF-32BE", UTF16BE, UTF32BE, NO_REPLACE, "\x00\xFE\x00\xFF\xFF\xFE\xFF\xFF", "00 00 00 22 00 00 00 5C 00 00 00 75 00 00 00 30 00 00 00 30 00 00 00 46 00 00 00 45 00 00 00 5C 00 00 00 75 00 00 00 30 00 00 00 30 00 00 00 46 00 00 00 46 00 00 00 5C 00 00 00 75 00 00 00 46 00 00 00 46 00 00 00 46 00 00 00 45 00 00 00 5C 00 00 00 75 00 00 00 46 00 00 00 46 00 00 00 46 00 00 00 46 00 00 00 22")

WRITE_STRING_TEST("more BMP noncharacter hex escape sequences -> UTF-8",    UTF16BE, UTF8,    NO_REPLACE, "\xFD\xD0\xFD\xEF", "22 5C 75 46 44 44 30 5C 75 46 44 45 46 22")
WRITE_STRING_TEST("more BMP noncharacter hex escape sequences -> UTF-16LE", UTF16BE, UTF16LE, NO_REPLACE, "\xFD\xD0\xFD\xEF", "22 00 5C 00 75 00 46 00 44 00 44 00 30 00 5C 00 75 00 46 00 44 00 45 00 46 00 22 00")
WRITE_STRING_TEST("more BMP noncharacter hex escape sequences -> UTF-16BE", UTF16BE, UTF16BE, NO_REPLACE, "\xFD\xD0\xFD\xEF", "00 22 00 5C 00 75 00 46 00 44 00 44 00 30 00 5C 00 75 00 46 00 44 00 45 00 46 00 22")
WRITE_STRING_TEST("more BMP noncharacter hex escape sequences -> UTF-32LE", UTF16BE, UTF32LE, NO_REPLACE, "\xFD\xD0\xFD\xEF", "22 00 00 00 5C 00 00 00 75 00 00 00 46 00 00 00 44 00 00 00 44 00 00 00 30 00 00 00 5C 00 00 00 75 00 00 00 46 00 00 00 44 00 00 00 45 00 00 00 46 00 00 00 22 00 00 00")
WRITE_STRING_TEST("more BMP noncharacter hex escape sequences -> UTF-32BE", UTF16BE, UTF32BE, NO_REPLACE, "\xFD\xD0\xFD\xEF", "00 00 00 22 00 00 00 5C 00 00 00 75 00 00 00 46 00 00 00 44 00 00 00 44 00 00 00 30 00 00 00 5C 00 00 00 75 00 00 00 46 00 00 00 44 00 00 00 45 00 00 00 46 00 00 00 22")

WRITE_STRING_TEST("Javascript compatibility hex escape sequences -> UTF-8",    UTF16BE, UTF8,    NO_REPLACE, "\x20\x28\x20\x29", "22 5C 75 32 30 32 38 5C 75 32 30 32 39 22")
WRITE_STRING_TEST("Javascript compatibility hex escape sequences -> UTF-16LE", UTF16BE, UTF16LE, NO_REPLACE, "\x20\x28\x20\x29", "22 00 5C 00 75 00 32 00 30 00 32 00 38 00 5C 00 75 00 32 00 30 00 32 00 39 00 22 00")
WRITE_STRING_TEST("Javascript compatibility hex escape sequences -> UTF-16BE", UTF16BE, UTF16BE, NO_REPLACE, "\x20\x28\x20\x29", "00 22 00 5C 00 75 00 32 00 30 00 32 00 38 00 5C 00 75 00 32 00 30 00 32 00 39 00 22")
WRITE_STRING_TEST("Javascript compatibility hex escape sequences -> UTF-32LE", UTF16BE, UTF32LE, NO_REPLACE, "\x20\x28\x20\x29", "22 00 00 00 5C 00 00 00 75 00 00 00 32 00 00 00 30 00 00 00 32 00 00 00 38 00 00 00 5C 00 00 00 75 00 00 00 32 00 00 00 30 00 00 00 32 00 00 00 39 00 00 00 22 00 00 00")
WRITE_STRING_TEST("Javascript compatibility hex escape sequences -> UTF-32BE", UTF16BE, UTF32BE, NO_REPLACE, "\x20\x28\x20\x29", "00 00 00 22 00 00 00 5C 00 00 00 75 00 00 00 32 00 00 00 30 00 00 00 32 00 00 00 38 00 00 00 5C 00 00 00 75 00 00 00 32 00 00 00 30 00 00 00 32 00 00 00 39 00 00 00 22")

WRITE_STRING_TEST("non-BMP noncharacter hex escape sequences -> UTF-8",    UTF16BE, UTF8,    NO_REPLACE, "\xD8\x34\xDD\xFE\xD8\x34\xDD\xFF", "22 5C 75 44 38 33 34 5C 75 44 44 46 45 5C 75 44 38 33 34 5C 75 44 44 46 46 22")
WRITE_STRING_TEST("non-BMP noncharacter hex escape sequences -> UTF-16LE", UTF16BE, UTF16LE, NO_REPLACE, "\xD8\x34\xDD\xFE\xD8\x34\xDD\xFF", "22 00 5C 00 75 00 44 00 38 00 33 00 34 00 5C 00 75 00 44 00 44 00 46 00 45 00 5C 00 75 00 44 00 38 00 33 00 34 00 5C 00 75 00 44 00 44 00 46 00 46 00 22 00")
WRITE_STRING_TEST("non-BMP noncharacter hex escape sequences -> UTF-16BE", UTF16BE, UTF16BE, NO_REPLACE, "\xD8\x34\xDD\xFE\xD8\x34\xDD\xFF", "00 22 00 5C 00 75 00 44 00 38 00 33 00 34 00 5C 00 75 00 44 00 44 00 46 00 45 00 5C 00 75 00 44 00 38 00 33 00 34 00 5C 00 75 00 44 00 44 00 46 00 46 00 22")
WRITE_STRING_TEST("non-BMP noncharacter hex escape sequences -> UTF-32LE", UTF16BE, UTF32LE, NO_REPLACE, "\xD8\x34\xDD\xFE\xD8\x34\xDD\xFF", "22 00 00 00 5C 00 00 00 75 00 00 00 44 00 00 00 38 00 00 00 33 00 00 00 34 00 00 00 5C 00 00 00 75 00 00 00 44 00 00 00 44 00 00 00 46 00 00 00 45 00 00 00 5C 00 00 00 75 00 00 00 44 00 00 00 38 00 00 00 33 00 00 00 34 00 00 00 5C 00 00 00 75 00 00 00 44 00 00 00 44 00 00 00 46 00 00 00 46 00 00 00 22 00 00 00")
WRITE_STRING_TEST("non-BMP noncharacter hex escape sequences -> UTF-32BE", UTF16BE, UTF32BE, NO_REPLACE, "\xD8\x34\xDD\xFE\xD8\x34\xDD\xFF", "00 00 00 22 00 00 00 5C 00 00 00 75 00 00 00 44 00 00 00 38 00 00 00 33 00 00 00 34 00 00 00 5C 00 00 00 75 00 00 00 44 00 00 00 44 00 00 00 46 00 00 00 45 00 00 00 5C 00 00 00 75 00 00 00 44 00 00 00 38 00 00 00 33 00 00 00 34 00 00 00 5C 00 00 00 75 00 00 00 44 00 00 00 44 00 00 00 46 00 00 00 46 00 00 00 22")

WRITE_STRING_TEST("replacement character in original string (1)", UTF8,    UTF8, NO_REPLACE, "\xEF\xBF\xBD", "22 EF BF BD 22")
WRITE_STRING_TEST("replacement character in original string (2)", UTF16LE, UTF8, NO_REPLACE, "\xFD\xFF", "22 EF BF BD 22")
WRITE_STRING_TEST("replacement character in original string (3)", UTF16BE, UTF8, NO_REPLACE, "\xFF\xFD", "22 EF BF BD 22")
WRITE_STRING_TEST("replacement character in original string (4)", UTF32LE, UTF8, NO_REPLACE, "\xFD\xFF\x00\x00", "22 EF BF BD 22")
WRITE_STRING_TEST("replacement character in original string (5)", UTF32BE, UTF8, NO_REPLACE, "\x00\x00\xFF\xFD", "22 EF BF BD 22")

WRITE_STRING_TEST("very long string", UTF8, UTF8, NO_REPLACE,
                  "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
                  "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF",
                  "22 "
                  "30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 "
                  "22")

/* invalid input encoding sequences */

WRITE_STRING_TEST("UTF-8 truncated sequence (1)", UTF8, UTF8, NO_REPLACE, "\xC2", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 truncated sequence (2)", UTF8, UTF8, NO_REPLACE, "\xE0", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 truncated sequence (3)", UTF8, UTF8, NO_REPLACE, "\xE0\xBF", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 truncated sequence (4)", UTF8, UTF8, NO_REPLACE, "\xF0\xBF", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 truncated sequence (5)", UTF8, UTF8, NO_REPLACE, "\xF0\xBF\xBF", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 overlong 2-byte sequence not allowed (1)", UTF8, UTF8, NO_REPLACE, "\xC0", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 overlong 2-byte sequence not allowed (2)", UTF8, UTF8, NO_REPLACE, "\xC1", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 overlong 3-byte sequence not allowed (1)", UTF8, UTF8, NO_REPLACE, "\xE0\x80", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 overlong 3-byte sequence not allowed (2)", UTF8, UTF8, NO_REPLACE, "\xE0\x9F", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 encoded surrogate not allowed (1)", UTF8, UTF8, NO_REPLACE, "\xED\xA0", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 encoded surrogate not allowed (2)", UTF8, UTF8, NO_REPLACE, "\xED\xBF", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 overlong 4-byte sequence not allowed (1)", UTF8, UTF8, NO_REPLACE, "\xF0\x80", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 overlong 4-byte sequence not allowed (2)", UTF8, UTF8, NO_REPLACE, "\xF0\x8F", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 encoded out-of-range codepoint not allowed (1)", UTF8, UTF8, NO_REPLACE, "\xF4\x90", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid leading byte not allowed (1)", UTF8, UTF8, NO_REPLACE, "\x80", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid leading byte not allowed (2)", UTF8, UTF8, NO_REPLACE, "\xBF", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid leading byte not allowed (3)", UTF8, UTF8, NO_REPLACE, "\xF5", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid leading byte not allowed (4)", UTF8, UTF8, NO_REPLACE, "\xFF", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (1)", UTF8, UTF8, NO_REPLACE, "\xC2\x7F", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (2)", UTF8, UTF8, NO_REPLACE, "\xC2\xC0", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (3)", UTF8, UTF8, NO_REPLACE, "\xE1\x7F", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (4)", UTF8, UTF8, NO_REPLACE, "\xE1\xC0", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (5)", UTF8, UTF8, NO_REPLACE, "\xE1\xBF\x7F", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (6)", UTF8, UTF8, NO_REPLACE, "\xE1\xBF\xC0", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (7)", UTF8, UTF8, NO_REPLACE, "\xF1\x7F", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (8)", UTF8, UTF8, NO_REPLACE, "\xF1\xC0", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (9)", UTF8, UTF8, NO_REPLACE, "\xF1\xBF\x7F", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (10)", UTF8, UTF8, NO_REPLACE, "\xF1\xBF\xC0", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (11)", UTF8, UTF8, NO_REPLACE, "\xF1\xBF\xBF\x7F", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-8 invalid continuation byte not allowed (12)", UTF8, UTF8, NO_REPLACE, "\xF1\xBF\xBF\xC0", "22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16LE truncated sequence", UTF16LE, UTF16LE, NO_REPLACE, " ", "22 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16LE standalone trailing surrogate not allowed (1)", UTF16LE, UTF16LE, NO_REPLACE, "\x00\xDC", "22 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16LE standalone trailing surrogate not allowed (2)", UTF16LE, UTF16LE, NO_REPLACE, "\xFF\xDF", "22 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16LE standalone leading surrogate not allowed (1)", UTF16LE, UTF16LE, NO_REPLACE, "\x00\xD8\x00_", "22 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16LE standalone leading surrogate not allowed (2)", UTF16LE, UTF16LE, NO_REPLACE, "\xFF\xDB\x00_", "22 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16LE standalone leading surrogate not allowed (3)", UTF16LE, UTF16LE, NO_REPLACE, "\xFF\xDB_", "22 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16BE truncated sequence", UTF16BE, UTF16BE, NO_REPLACE, "\x00", "00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16BE standalone trailing surrogate not allowed (1)", UTF16BE, UTF16BE, NO_REPLACE, "\xDC\x00", "00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16BE standalone trailing surrogate not allowed (2)", UTF16BE, UTF16BE, NO_REPLACE, "\xDF\xFF", "00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16BE standalone leading surrogate not allowed (1)", UTF16BE, UTF16BE, NO_REPLACE, "\xD8\x00\x00_", "00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16BE standalone leading surrogate not allowed (2)", UTF16BE, UTF16BE, NO_REPLACE, "\xDB\xFF\x00_", "00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-16BE standalone leading surrogate not allowed (3)", UTF16BE, UTF16BE, NO_REPLACE, "\xDB\xFF", "00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32LE truncated sequence (1)", UTF32LE, UTF32LE, NO_REPLACE, " ", "22 00 00 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32LE truncated sequence (2)", UTF32LE, UTF32LE, NO_REPLACE, " \x00", "22 00 00 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32LE truncated sequence (3)", UTF32LE, UTF32LE, NO_REPLACE, " \x00\x00", "22 00 00 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32LE encoded surrogate not allowed (1)", UTF32LE, UTF32LE, NO_REPLACE, "\x00\xD8\x00\x00", "22 00 00 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32LE encoded surrogate not allowed (2)", UTF32LE, UTF32LE, NO_REPLACE, "\x00\xDF\x00\x00", "22 00 00 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32LE encoded out-of-range codepoint not allowed (1)", UTF32LE, UTF32LE, NO_REPLACE, "\x00\x00\x11\x00", "22 00 00 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32LE encoded out-of-range codepoint not allowed (2)", UTF32LE, UTF32LE, NO_REPLACE, "\xFF\xFF\xFF\xFF", "22 00 00 00 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32BE truncated sequence (1)", UTF32BE, UTF32BE, NO_REPLACE, "\x00", "00 00 00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32BE truncated sequence (2)", UTF32BE, UTF32BE, NO_REPLACE, "\x00\x00", "00 00 00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32BE truncated sequence (3)", UTF32BE, UTF32BE, NO_REPLACE, "\x00\x00\x00", "00 00 00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32BE encoded surrogate not allowed (1)", UTF32BE, UTF32BE, NO_REPLACE, "\x00\x00\xD8\x00", "00 00 00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32BE encoded surrogate not allowed (2)", UTF32BE, UTF32BE, NO_REPLACE, "\x00\x00\xDF\xFF", "00 00 00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32BE encoded out-of-range codepoint not allowed (1)", UTF32BE, UTF32BE, NO_REPLACE, "\x00\x11\x00\x00", "00 00 00 22 !(InvalidEncodingSequence)")
WRITE_STRING_TEST("UTF-32BE encoded out-of-range codepoint not allowed (2)", UTF32BE, UTF32BE, NO_REPLACE, "\xFF\xFF\xFF\xFF", "00 00 00 22 !(InvalidEncodingSequence)")

/* replace invalid input encoding sequences */

WRITE_STRING_TEST("replace UTF-8 truncated 2-byte sequence (1)", UTF8, UTF8, REPLACE, "abc\xC2", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 truncated 3-byte sequence (1)", UTF8, UTF8, REPLACE, "abc\xE0", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 truncated 3-byte sequence (2)", UTF8, UTF8, REPLACE, "abc\xE0\xBF", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 truncated 4-byte sequence (1)", UTF8, UTF8, REPLACE, "abc\xF0", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 truncated 4-byte sequence (2)", UTF8, UTF8, REPLACE, "abc\xF0\xBF", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 truncated 4-byte sequence (3)", UTF8, UTF8, REPLACE, "abc\xF0\xBF\xBF", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 overlong 2-byte sequence (1)", UTF8, UTF8, REPLACE, "abc\xC0", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 overlong 2-byte sequence (2)", UTF8, UTF8, REPLACE, "abc\xC1", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 overlong 3-byte sequence (1)", UTF8, UTF8, REPLACE, "abc\xE0\x80", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 overlong 3-byte sequence (2)", UTF8, UTF8, REPLACE, "abc\xE0\x9F", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 encoded surrogate (1)", UTF8, UTF8, REPLACE, "abc\xED\xA0", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 encoded surrogate (2)", UTF8, UTF8, REPLACE, "abc\xED\xBF", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 overlong 4-byte sequence (1)", UTF8, UTF8, REPLACE, "abc\xF0\x80", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 overlong 4-byte sequence (2)", UTF8, UTF8, REPLACE, "abc\xF0\x8F", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 encoded out-of-range codepoint (1)", UTF8, UTF8, REPLACE, "abc\xF4\x90", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 invalid leading byte (1)", UTF8, UTF8, REPLACE, "abc\x80", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 invalid leading byte (2)", UTF8, UTF8, REPLACE, "abc\xBF", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 invalid leading byte (3)", UTF8, UTF8, REPLACE, "abc\xF5", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 invalid leading byte (4)", UTF8, UTF8, REPLACE, "abc\xFF", "22 61 62 63 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (1)", UTF8, UTF8, REPLACE, "abc\xC2\x7F", "22 61 62 63 5C 75 46 46 46 44 5C 75 30 30 37 46 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (2)", UTF8, UTF8, REPLACE, "abc\xC2\xC0", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (3)", UTF8, UTF8, REPLACE, "abc\xE1\x7F", "22 61 62 63 5C 75 46 46 46 44 5C 75 30 30 37 46 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (4)", UTF8, UTF8, REPLACE, "abc\xE1\xC0", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (5)", UTF8, UTF8, REPLACE, "abc\xE1\xBF\x7F", "22 61 62 63 5C 75 46 46 46 44 5C 75 30 30 37 46 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (6)", UTF8, UTF8, REPLACE, "abc\xE1\xBF\xC0", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (7)", UTF8, UTF8, REPLACE, "abc\xF1\x7F", "22 61 62 63 5C 75 46 46 46 44 5C 75 30 30 37 46 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (8)", UTF8, UTF8, REPLACE, "abc\xF1\xC0", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (9)", UTF8, UTF8, REPLACE, "abc\xF1\xBF\x7F", "22 61 62 63 5C 75 46 46 46 44 5C 75 30 30 37 46 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (10)", UTF8, UTF8, REPLACE, "abc\xF1\xBF\xC0", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (11)", UTF8, UTF8, REPLACE, "abc\xF1\xBF\xBF\x7F", "22 61 62 63 5C 75 46 46 46 44 5C 75 30 30 37 46 22")
WRITE_STRING_TEST("replace UTF-8 invalid continuation byte (12)", UTF8, UTF8, REPLACE, "abc\xF1\xBF\xBF\xC0", "22 61 62 63 5C 75 46 46 46 44 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("Unicode 5.2.0 replacement example", UTF8, UTF8, REPLACE, "\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64", "22 61 5C 75 46 46 46 44 5C 75 46 46 46 44 5C 75 46 46 46 44 62 5C 75 46 46 46 44 63 5C 75 46 46 46 44 5C 75 46 46 46 44 64 22")
WRITE_STRING_TEST("replace UTF-16LE standalone trailing surrogate (1)", UTF16LE, UTF8, REPLACE, "_\x00" "\x00\xDC", "22 5F 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-16LE standalone trailing surrogate (2)", UTF16LE, UTF8, REPLACE, "_\x00" "\xFF\xDF", "22 5F 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-16LE standalone leading surrogate (1)", UTF16LE, UTF8, REPLACE,  "_\x00" "\x00\xD8" "_\x00", "22 5F 5C 75 46 46 46 44 5F 22")
WRITE_STRING_TEST("replace UTF-16LE standalone leading surrogate (2)", UTF16LE, UTF8, REPLACE,  "_\x00" "\xFF\xDB" "_\x00", "22 5F 5C 75 46 46 46 44 5F 22")
WRITE_STRING_TEST("replace UTF-16BE standalone trailing surrogate (1)", UTF16BE, UTF8, REPLACE, "\x00_" "\xDC\x00", "22 5F 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-16BE standalone trailing surrogate (2)", UTF16BE, UTF8, REPLACE, "\x00_" "\xDF\xFF", "22 5F 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-16BE standalone leading surrogate (1)", UTF16BE, UTF8, REPLACE,  "\x00_" "\xD8\x00" "\x00_", "22 5F 5C 75 46 46 46 44 5F 22")
WRITE_STRING_TEST("replace UTF-16BE standalone leading surrogate (2)", UTF16BE, UTF8, REPLACE,  "\x00_" "\xDB\xFF" "\x00_", "22 5F 5C 75 46 46 46 44 5F 22")
WRITE_STRING_TEST("replace UTF-32LE encoded surrogate (1)", UTF32LE, UTF8, REPLACE, "\x00\xD8\x00\x00", "22 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-32LE encoded surrogate (2)", UTF32LE, UTF8, REPLACE, "\xFF\xDF\x00\x00", "22 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-32LE encoded out-of-range codepoint (1)", UTF32LE, UTF8, REPLACE, "\x00\x00\x11\x00", "22 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-32LE encoded out-of-range codepoint (2)", UTF32LE, UTF8, REPLACE, "\x00\x00\x00\x01", "22 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-32BE encoded surrogate (1)", UTF32BE, UTF8, REPLACE, "\x00\x00\xD8\x00", "22 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-32BE encoded surrogate (2)", UTF32BE, UTF8, REPLACE, "\x00\x00\xDF\xFF", "22 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-32BE encoded out-of-range codepoint (1)", UTF32BE, UTF8, REPLACE, "\x00\x11\x00\x00", "22 5C 75 46 46 46 44 22")
WRITE_STRING_TEST("replace UTF-32BE encoded out-of-range codepoint (2)", UTF32BE, UTF8, REPLACE, "\x01\x00\x00\x00", "22 5C 75 46 46 46 44 22")

};

static void TestWriterWriteString()
{
    size_t i;
    for  (i = 0; i < sizeof(s_writeStringTests)/sizeof(s_writeStringTests[0]); i++)
    {
        RunWriteStringTest(&s_writeStringTests[i]);
    }
}

static void TestWriterWriteStringWithInvalidParameters()
{
    JSON_Writer writer = NULL;
    printf("Test writing string with invalid parameters ... ");

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterWriteString(writer, NULL, 1, JSON_UTF8, JSON_Failure) &&
        CheckWriterWriteString(writer, "a", 1, JSON_UnknownEncoding, JSON_Failure) &&
        CheckWriterWriteString(writer, "a", 1, (JSON_Encoding)(JSON_UTF32BE + 1), JSON_Failure))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
}

static void RunWriteNumberTest(const WriteTest* pTest)
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    WriterState state;
    printf("Test writing number %s ... ", pTest->pName);

    InitWriterSettings(&settings);
    settings.outputEncoding = pTest->outputEncoding;

    InitWriterState(&state);
    ResetOutput();

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, settings.outputEncoding, JSON_Success))
    {
        if (JSON_Writer_WriteNumber(writer, pTest->pInput, pTest->length) != JSON_Success)
        {
            state.error = JSON_Writer_GetError(writer);
            if (state.error != JSON_Error_None)
            {
                OutputSeparator();
                OutputFormatted("!(%s)", errorNames[state.error]);
            }
        }
        if (CheckWriterState(writer, &state) && CheckOutput(pTest->pOutput))
        {
            printf("OK\n");
        }
        else
        {
            s_failureCount++;
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
    ResetOutput();
}

#define WRITE_NUMBER_TEST(name, out_enc, input, output) { name, JSON_UnknownEncoding, JSON_##out_enc, NO_REPLACE, input, sizeof(input) - 1, output },

static const WriteTest s_writeNumberTests[] =
{

WRITE_NUMBER_TEST("-0.1e+2 -> UTF-8",       UTF8,    "-0.1e+2", "2D 30 2E 31 65 2B 32")
WRITE_NUMBER_TEST("-0.1e+2 -> UTF-16LE",    UTF16LE, "-0.1e+2", "2D 00 30 00 2E 00 31 00 65 00 2B 00 32 00")
WRITE_NUMBER_TEST("-0.1e+2 -> UTF-16BE",    UTF16BE, "-0.1e+2", "00 2D 00 30 00 2E 00 31 00 65 00 2B 00 32")
WRITE_NUMBER_TEST("-0.1e+2 -> UTF-32LE",    UTF32LE, "-0.1e+2", "2D 00 00 00 30 00 00 00 2E 00 00 00 31 00 00 00 65 00 00 00 2B 00 00 00 32 00 00 00")
WRITE_NUMBER_TEST("-0.1e+2 -> UTF-32BE",    UTF32BE, "-0.1e+2", "00 00 00 2D 00 00 00 30 00 00 00 2E 00 00 00 31 00 00 00 65 00 00 00 2B 00 00 00 32")

WRITE_NUMBER_TEST("bad decimal (1)", UTF8, "-", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad decimal (2)", UTF8, " ", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad decimal (3)", UTF8, " 1", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad decimal (4)", UTF8, "1 ", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad decimal (5)", UTF8, "01", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad decimal (6)", UTF8, "1x", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad decimal (7)", UTF8, "1.", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad decimal (8)", UTF8, "1e", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad decimal (9)", UTF8, "1e+", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad decimal (10)", UTF8, "1e-", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad decimal (11)", UTF8, "1ex", "!(InvalidNumber)")

WRITE_NUMBER_TEST("hex (1)", UTF8, "0x0", "30 78 30")
WRITE_NUMBER_TEST("hex (1)", UTF8, "0X0", "30 58 30")
WRITE_NUMBER_TEST("hex (2)", UTF8, "0x0123456789ABCDEF", "30 78 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46")
WRITE_NUMBER_TEST("hex (3)", UTF8, "0X0123456789abcdef", "30 58 30 31 32 33 34 35 36 37 38 39 61 62 63 64 65 66")

WRITE_NUMBER_TEST("bad hex not allowed (1)", UTF8, "0x", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad hex not allowed (2)", UTF8, "0X", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad hex not allowed (3)", UTF8, "0x1.", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad hex not allowed (4)", UTF8, "0x1.0", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad hex not allowed (5)", UTF8, "0x1e+", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad hex not allowed (6)", UTF8, "0x1e-", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad hex not allowed (7)", UTF8, "0x1e+1", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad hex not allowed (8)", UTF8, "0x1e-1", "!(InvalidNumber)")
WRITE_NUMBER_TEST("bad hex not allowed (9)", UTF8, "-0x1", "!(InvalidNumber)")

};

static void TestWriterWriteNumber()
{
    size_t i;
    for  (i = 0; i < sizeof(s_writeNumberTests)/sizeof(s_writeNumberTests[0]); i++)
    {
        RunWriteNumberTest(&s_writeNumberTests[i]);
    }
}

static void TestWriterWriteNumberWithInvalidParameters()
{
    JSON_Writer writer = NULL;
    printf("Test writing number with invalid parameters ... ");

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterWriteNumber(writer, NULL, 1, JSON_Failure) &&
        CheckWriterWriteNumber(writer, "1", 2, JSON_Failure))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
}

#define WRITE_SPECIAL_NUMBER_TEST(name, out_enc, input, output) { name, JSON_UnknownEncoding, JSON_##out_enc, NO_REPLACE, NULL, (size_t)input, output },

static void RunWriteSpecialNumberTest(const WriteTest* pTest)
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    WriterState state;
    printf("Test writing special number %s ... ", pTest->pName);

    InitWriterSettings(&settings);
    settings.outputEncoding = pTest->outputEncoding;

    InitWriterState(&state);
    ResetOutput();

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, settings.outputEncoding, JSON_Success))
    {
        if (JSON_Writer_WriteSpecialNumber(writer, (JSON_SpecialNumber)pTest->length) != JSON_Success)
        {
            state.error = JSON_Writer_GetError(writer);
            if (state.error != JSON_Error_None)
            {
                OutputSeparator();
                OutputFormatted("!(%s)", errorNames[state.error]);
            }
        }
        if (CheckWriterState(writer, &state) && CheckOutput(pTest->pOutput))
        {
            printf("OK\n");
        }
        else
        {
            s_failureCount++;
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
    ResetOutput();
}

static const WriteTest s_writeSpecialNumberTests[] =
{

WRITE_SPECIAL_NUMBER_TEST("NaN -> UTF-8",           UTF8,    JSON_NaN, "4E 61 4E")
WRITE_SPECIAL_NUMBER_TEST("NaN -> UTF-16LE",        UTF16LE, JSON_NaN, "4E 00 61 00 4E 00")
WRITE_SPECIAL_NUMBER_TEST("NaN -> UTF-16BE",        UTF16BE, JSON_NaN, "00 4E 00 61 00 4E")
WRITE_SPECIAL_NUMBER_TEST("NaN -> UTF-32LE",        UTF32LE, JSON_NaN, "4E 00 00 00 61 00 00 00 4E 00 00 00")
WRITE_SPECIAL_NUMBER_TEST("NaN -> UTF-32BE",        UTF32BE, JSON_NaN, "00 00 00 4E 00 00 00 61 00 00 00 4E")
WRITE_SPECIAL_NUMBER_TEST("Infinity -> UTF-8",      UTF8,    JSON_Infinity, "49 6E 66 69 6E 69 74 79")
WRITE_SPECIAL_NUMBER_TEST("Infinity -> UTF-16LE",   UTF16LE, JSON_Infinity, "49 00 6E 00 66 00 69 00 6E 00 69 00 74 00 79 00")
WRITE_SPECIAL_NUMBER_TEST("Infinity -> UTF-16BE",   UTF16BE, JSON_Infinity, "00 49 00 6E 00 66 00 69 00 6E 00 69 00 74 00 79")
WRITE_SPECIAL_NUMBER_TEST("Infinity -> UTF-32LE",   UTF32LE, JSON_Infinity, "49 00 00 00 6E 00 00 00 66 00 00 00 69 00 00 00 6E 00 00 00 69 00 00 00 74 00 00 00 79 00 00 00")
WRITE_SPECIAL_NUMBER_TEST("Infinity -> UTF-32BE",   UTF32BE, JSON_Infinity, "00 00 00 49 00 00 00 6E 00 00 00 66 00 00 00 69 00 00 00 6E 00 00 00 69 00 00 00 74 00 00 00 79")
WRITE_SPECIAL_NUMBER_TEST("-Infinity -> UTF-8",     UTF8,    JSON_NegativeInfinity, "2D 49 6E 66 69 6E 69 74 79")
WRITE_SPECIAL_NUMBER_TEST("-Infinity -> UTF-16LE",  UTF16LE, JSON_NegativeInfinity, "2D 00 49 00 6E 00 66 00 69 00 6E 00 69 00 74 00 79 00")
WRITE_SPECIAL_NUMBER_TEST("-Infinity -> UTF-16BE",  UTF16BE, JSON_NegativeInfinity, "00 2D 00 49 00 6E 00 66 00 69 00 6E 00 69 00 74 00 79")
WRITE_SPECIAL_NUMBER_TEST("-Infinity -> UTF-32LE",  UTF32LE, JSON_NegativeInfinity, "2D 00 00 00 49 00 00 00 6E 00 00 00 66 00 00 00 69 00 00 00 6E 00 00 00 69 00 00 00 74 00 00 00 79 00 00 00")
WRITE_SPECIAL_NUMBER_TEST("-Infinity -> UTF-32BE",  UTF32BE, JSON_NegativeInfinity, "00 00 00 2D 00 00 00 49 00 00 00 6E 00 00 00 66 00 00 00 69 00 00 00 6E 00 00 00 69 00 00 00 74 00 00 00 79")

};

static void TestWriterWriteSpecialNumber()
{
    size_t i;
    for  (i = 0; i < sizeof(s_writeSpecialNumberTests)/sizeof(s_writeSpecialNumberTests[0]); i++)
    {
        RunWriteSpecialNumberTest(&s_writeSpecialNumberTests[i]);
    }
}

#define WRITE_ARRAY_TEST(name, out_enc, output) { name, JSON_UnknownEncoding, JSON_##out_enc, NO_REPLACE, NULL, 0, output },

static void RunWriteArrayTest(const WriteTest* pTest)
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    WriterState state;
    printf("Test writing array %s ... ", pTest->pName);

    InitWriterSettings(&settings);
    settings.outputEncoding = pTest->outputEncoding;

    InitWriterState(&state);
    ResetOutput();

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, settings.outputEncoding, JSON_Success))
    {
        if (JSON_Writer_WriteStartArray(writer) != JSON_Success ||
            JSON_Writer_WriteStartArray(writer) != JSON_Success ||
            JSON_Writer_WriteEndArray(writer) != JSON_Success ||
            JSON_Writer_WriteComma(writer) != JSON_Success ||
            JSON_Writer_WriteNumber(writer, "0", 1) != JSON_Success ||
            JSON_Writer_WriteComma(writer) != JSON_Success ||
            JSON_Writer_WriteString(writer, "a", 1, JSON_UTF8) != JSON_Success ||
            JSON_Writer_WriteEndArray(writer) != JSON_Success)
        {
            state.error = JSON_Writer_GetError(writer);
            if (state.error != JSON_Error_None)
            {
                OutputSeparator();
                OutputFormatted("!(%s)", errorNames[state.error]);
            }
        }
        if (CheckWriterState(writer, &state) && CheckOutput(pTest->pOutput))
        {
            printf("OK\n");
        }
        else
        {
            s_failureCount++;
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
    ResetOutput();
}

static const WriteTest s_writeArrayTests[] =
{

WRITE_ARRAY_TEST("-> UTF-8",    UTF8,    "5B 5B 5D 2C 30 2C 22 61 22 5D")
WRITE_ARRAY_TEST("-> UTF-16LE", UTF16LE, "5B 00 5B 00 5D 00 2C 00 30 00 2C 00 22 00 61 00 22 00 5D 00")
WRITE_ARRAY_TEST("-> UTF-16BE", UTF16BE, "00 5B 00 5B 00 5D 00 2C 00 30 00 2C 00 22 00 61 00 22 00 5D")
WRITE_ARRAY_TEST("-> UTF-32LE", UTF32LE, "5B 00 00 00 5B 00 00 00 5D 00 00 00 2C 00 00 00 30 00 00 00 2C 00 00 00 22 00 00 00 61 00 00 00 22 00 00 00 5D 00 00 00")
WRITE_ARRAY_TEST("-> UTF-32BE", UTF32BE, "00 00 00 5B 00 00 00 5B 00 00 00 5D 00 00 00 2C 00 00 00 30 00 00 00 2C 00 00 00 22 00 00 00 61 00 00 00 22 00 00 00 5D")

};

static void TestWriterWriteArray()
{
    size_t i;
    for  (i = 0; i < sizeof(s_writeArrayTests)/sizeof(s_writeArrayTests[0]); i++)
    {
        RunWriteArrayTest(&s_writeArrayTests[i]);
    }
}

#define WRITE_OBJECT_TEST(name, out_enc, output) { name, JSON_UnknownEncoding, JSON_##out_enc, NO_REPLACE, NULL, 0, output },

static void RunWriteObjectTest(const WriteTest* pTest)
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    WriterState state;
    printf("Test writing object %s ... ", pTest->pName);

    InitWriterSettings(&settings);
    settings.outputEncoding = pTest->outputEncoding;

    InitWriterState(&state);
    ResetOutput();

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, settings.outputEncoding, JSON_Success))
    {
        if (JSON_Writer_WriteStartObject(writer) != JSON_Success ||
            JSON_Writer_WriteString(writer, "a", 1, JSON_UTF8) != JSON_Success ||
            JSON_Writer_WriteColon(writer) != JSON_Success ||
            JSON_Writer_WriteStartObject(writer) != JSON_Success ||
            JSON_Writer_WriteEndObject(writer) != JSON_Success ||
            JSON_Writer_WriteComma(writer) != JSON_Success ||
            JSON_Writer_WriteString(writer, "b", 1, JSON_UTF8) != JSON_Success ||
            JSON_Writer_WriteColon(writer) != JSON_Success ||
            JSON_Writer_WriteNumber(writer, "0", 1) != JSON_Success ||
            JSON_Writer_WriteEndObject(writer) != JSON_Success)
        {
            state.error = JSON_Writer_GetError(writer);
            if (state.error != JSON_Error_None)
            {
                OutputSeparator();
                OutputFormatted("!(%s)", errorNames[state.error]);
            }
        }
        if (CheckWriterState(writer, &state) && CheckOutput(pTest->pOutput))
        {
            printf("OK\n");
        }
        else
        {
            s_failureCount++;
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
    ResetOutput();
}

static const WriteTest s_writeObjectTests[] =
{

WRITE_OBJECT_TEST("-> UTF-8",    UTF8,    "7B 22 61 22 3A 7B 7D 2C 22 62 22 3A 30 7D")
WRITE_OBJECT_TEST("-> UTF-16LE", UTF16LE, "7B 00 22 00 61 00 22 00 3A 00 7B 00 7D 00 2C 00 22 00 62 00 22 00 3A 00 30 00 7D 00")
WRITE_OBJECT_TEST("-> UTF-16BE", UTF16BE, "00 7B 00 22 00 61 00 22 00 3A 00 7B 00 7D 00 2C 00 22 00 62 00 22 00 3A 00 30 00 7D")
WRITE_OBJECT_TEST("-> UTF-32LE", UTF32LE, "7B 00 00 00 22 00 00 00 61 00 00 00 22 00 00 00 3A 00 00 00 7B 00 00 00 7D 00 00 00 2C 00 00 00 22 00 00 00 62 00 00 00 22 00 00 00 3A 00 00 00 30 00 00 00 7D 00 00 00")
WRITE_OBJECT_TEST("-> UTF-32BE", UTF32BE, "00 00 00 7B 00 00 00 22 00 00 00 61 00 00 00 22 00 00 00 3A 00 00 00 7B 00 00 00 7D 00 00 00 2C 00 00 00 22 00 00 00 62 00 00 00 22 00 00 00 3A 00 00 00 30 00 00 00 7D")

};

static void TestWriterWriteObject()
{
    size_t i;
    for  (i = 0; i < sizeof(s_writeObjectTests)/sizeof(s_writeObjectTests[0]); i++)
    {
        RunWriteObjectTest(&s_writeObjectTests[i]);
    }
}

#define WRITE_SPACE_TEST(name, out_enc, count, output) { name, JSON_UnknownEncoding, JSON_##out_enc, NO_REPLACE, NULL, count, output },

static void RunWriteSpaceTest(const WriteTest* pTest)
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    WriterState state;
    printf("Test writing space %s ... ", pTest->pName);

    InitWriterSettings(&settings);
    settings.outputEncoding = pTest->outputEncoding;

    InitWriterState(&state);
    ResetOutput();

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, settings.outputEncoding, JSON_Success))
    {
        if (JSON_Writer_WriteSpace(writer, pTest->length) != JSON_Success)
        {
            state.error = JSON_Writer_GetError(writer);
            if (state.error != JSON_Error_None)
            {
                OutputSeparator();
                OutputFormatted("!(%s)", errorNames[state.error]);
            }
        }
        if (CheckWriterState(writer, &state) && CheckOutput(pTest->pOutput))
        {
            printf("OK\n");
        }
        else
        {
            s_failureCount++;
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
    ResetOutput();
}

static const WriteTest s_writeSpaceTests[] =
{

WRITE_SPACE_TEST("-> UTF-8",    UTF8,    1, "20")
WRITE_SPACE_TEST("-> UTF-16LE", UTF16LE, 1, "20 00")
WRITE_SPACE_TEST("-> UTF-16BE", UTF16BE, 1, "00 20")
WRITE_SPACE_TEST("-> UTF-32LE", UTF32LE, 1, "20 00 00 00")
WRITE_SPACE_TEST("-> UTF-32BE", UTF32BE, 1, "00 00 00 20")

WRITE_SPACE_TEST("(2) -> UTF-8",    UTF8,    2, "20 20")
WRITE_SPACE_TEST("(2) -> UTF-16LE", UTF16LE, 2, "20 00 20 00")
WRITE_SPACE_TEST("(2) -> UTF-16BE", UTF16BE, 2, "00 20 00 20")
WRITE_SPACE_TEST("(2) -> UTF-32LE", UTF32LE, 2, "20 00 00 00 20 00 00 00")
WRITE_SPACE_TEST("(2) -> UTF-32BE", UTF32BE, 2, "00 00 00 20 00 00 00 20")

WRITE_SPACE_TEST("(3) -> UTF-8",    UTF8,    3, "20 20 20")
WRITE_SPACE_TEST("(3) -> UTF-16LE", UTF16LE, 3, "20 00 20 00 20 00")
WRITE_SPACE_TEST("(3) -> UTF-16BE", UTF16BE, 3, "00 20 00 20 00 20")
WRITE_SPACE_TEST("(3) -> UTF-32LE", UTF32LE, 3, "20 00 00 00 20 00 00 00 20 00 00 00")
WRITE_SPACE_TEST("(3) -> UTF-32BE", UTF32BE, 3, "00 00 00 20 00 00 00 20 00 00 00 20")

WRITE_SPACE_TEST("(15) -> UTF-8",    UTF8,    15, "20 20 20 20 20 20 20 20 20 20 20 20 20 20 20")
WRITE_SPACE_TEST("(15) -> UTF-16LE", UTF16LE, 15, "20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00")
WRITE_SPACE_TEST("(15) -> UTF-16BE", UTF16BE, 15, "00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20 00 20")
WRITE_SPACE_TEST("(15) -> UTF-32LE", UTF32LE, 15, "20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00")
WRITE_SPACE_TEST("(15) -> UTF-32BE", UTF32BE, 15, "00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00 00 20")

};

static void TestWriterWriteSpace()
{
    size_t i;
    for  (i = 0; i < sizeof(s_writeSpaceTests)/sizeof(s_writeSpaceTests[0]); i++)
    {
        RunWriteSpaceTest(&s_writeSpaceTests[i]);
    }
}

#define WRITE_NEWLINE_TEST(name, out_enc, crlf, output) { name, JSON_UnknownEncoding, JSON_##out_enc, NO_REPLACE, NULL, crlf, output },

#define NO_CRLF  JSON_False
#define CRLF     JSON_True

static void RunWriteNewLineTest(const WriteTest* pTest)
{
    JSON_Writer writer = NULL;
    WriterSettings settings;
    WriterState state;
    printf("Test writing newline %s ... ", pTest->pName);

    InitWriterSettings(&settings);
    settings.outputEncoding = pTest->outputEncoding;
    settings.useCRLF = (JSON_Boolean)pTest->length;

    InitWriterState(&state);
    ResetOutput();

    if (CheckWriterCreateWithCustomMemorySuite(&ReallocHandler, &FreeHandler, JSON_Success, &writer) &&
        CheckWriterSetOutputHandler(writer, &OutputHandler, JSON_Success) &&
        CheckWriterSetOutputEncoding(writer, settings.outputEncoding, JSON_Success) &&
        CheckWriterSetUseCRLF(writer, settings.useCRLF, JSON_Success))
    {
        if (JSON_Writer_WriteNewLine(writer) != JSON_Success)
        {
            state.error = JSON_Writer_GetError(writer);
            if (state.error != JSON_Error_None)
            {
                OutputSeparator();
                OutputFormatted("!(%s)", errorNames[state.error]);
            }
        }
        if (CheckWriterState(writer, &state) && CheckOutput(pTest->pOutput))
        {
            printf("OK\n");
        }
        else
        {
            s_failureCount++;
        }
    }
    else
    {
        s_failureCount++;
    }
    JSON_Writer_Free(writer);
    ResetOutput();
}

static const WriteTest s_writeNewLineTests[] =
{

WRITE_NEWLINE_TEST("-> UTF-8",    UTF8,    NO_CRLF, "0A")
WRITE_NEWLINE_TEST("-> UTF-16LE", UTF16LE, NO_CRLF, "0A 00")
WRITE_NEWLINE_TEST("-> UTF-16BE", UTF16BE, NO_CRLF, "00 0A")
WRITE_NEWLINE_TEST("-> UTF-32LE", UTF32LE, NO_CRLF, "0A 00 00 00")
WRITE_NEWLINE_TEST("-> UTF-32BE", UTF32BE, NO_CRLF, "00 00 00 0A")
WRITE_NEWLINE_TEST("-> UTF-8",    UTF8,    CRLF,    "0D 0A")
WRITE_NEWLINE_TEST("-> UTF-16LE", UTF16LE, CRLF,    "0D 00 0A 00")
WRITE_NEWLINE_TEST("-> UTF-16BE", UTF16BE, CRLF,    "00 0D 00 0A")
WRITE_NEWLINE_TEST("-> UTF-32LE", UTF32LE, CRLF,    "0D 00 00 00 0A 00 00 00")
WRITE_NEWLINE_TEST("-> UTF-32BE", UTF32BE, CRLF,    "00 00 00 0D 00 00 00 0A")

};

static void TestWriterWriteNewLine()
{
    size_t i;
    for  (i = 0; i < sizeof(s_writeNewLineTests)/sizeof(s_writeNewLineTests[0]); i++)
    {
        RunWriteNewLineTest(&s_writeNewLineTests[i]);
    }
}

static void TestErrorStrings()
{
    printf("Test error strings ... ");
    if (CheckErrorString(JSON_Error_None, "no error") &&
        CheckErrorString(JSON_Error_OutOfMemory, "could not allocate enough memory") &&
        CheckErrorString(JSON_Error_AbortedByHandler, "the operation was aborted by a handler") &&
        CheckErrorString(JSON_Error_BOMNotAllowed, "the input begins with a byte-order mark (BOM), which is not allowed by RFC 4627") &&
        CheckErrorString(JSON_Error_InvalidEncodingSequence, "the input contains a byte or sequence of bytes that is not valid for the input encoding") &&
        CheckErrorString(JSON_Error_UnknownToken, "the input contains an unknown token") &&
        CheckErrorString(JSON_Error_UnexpectedToken, "the input contains an unexpected token") &&
        CheckErrorString(JSON_Error_IncompleteToken,  "the input ends in the middle of a token") &&
        CheckErrorString(JSON_Error_ExpectedMoreTokens, "the input ends when more tokens are expected") &&
        CheckErrorString(JSON_Error_UnescapedControlCharacter, "the input contains a string containing an unescaped control character (U+0000 - U+001F)") &&
        CheckErrorString(JSON_Error_InvalidEscapeSequence, "the input contains a string containing an invalid escape sequence") &&
        CheckErrorString(JSON_Error_UnpairedSurrogateEscapeSequence, "the input contains a string containing an unmatched UTF-16 surrogate codepoint") &&
        CheckErrorString(JSON_Error_TooLongString, "the input contains a string that is too long") &&
        CheckErrorString(JSON_Error_InvalidNumber, "the input contains an invalid number") &&
        CheckErrorString(JSON_Error_TooLongNumber, "the input contains a number that is too long") &&
        CheckErrorString(JSON_Error_DuplicateObjectMember, "the input contains an object with duplicate members") &&
        CheckErrorString((JSON_Error)-1, "") &&
        CheckErrorString((JSON_Error)1000, ""))
    {
        printf("OK\n");
    }
    else
    {
        s_failureCount++;
    }
}

static void TestNoLeaks()
{
    printf("Checking for memory leaks ... ");
    if (!s_blocksAllocated && !s_bytesAllocated)
    {
        printf("OK\n");
    }
    else
    {
        printf("FAILURE: %d blocks (%d bytes) leaked\n", (int)s_blocksAllocated, (int)s_bytesAllocated);
        s_failureCount++;
    }
}

int main(int argc, char* argv[])
{
    (void)argc; /* unused */
    (void)argv; /* unused */

    TestParserCreate();
    TestParserCreateWithCustomMemorySuite();
    TestParserCreateMallocFailure();
    TestParserMissing();
    TestParserGetErrorLocationNullLocation();
    TestParserGetErrorLocationNoError();
    TestParserGetTokenLocationOutsideHandler();
    TestParserSetSettings();
    TestParserSetInvalidSettings();
    TestParserSetHandlers();
    TestParserReset();
    TestParserMisbehaveInCallbacks();
    TestParserAbortInCallbacks();
    TestParserStringMallocFailure();
    TestParserStringReallocFailure();
    TestParserStackMallocFailure();
    TestParserStackReallocFailure();
    TestParserDuplicateMemberTrackingMallocFailure();
    TestParserParse();

    TestWriterCreate();
    TestWriterCreateWithCustomMemorySuite();
    TestWriterCreateMallocFailure();
    TestWriterMissing();
    TestWriterSetSettings();
    TestWriterSetInvalidSettings();
    TestWriterSetHandlers();
    TestWriterReset();
    TestWriterMisbehaveInCallbacks();
    TestWriterAbortInCallbacks();
    TestWriterStackMallocFailure();
    TestWriterStackReallocFailure();
    TestWriterWriteNull();
    TestWriterWriteBoolean();
    TestWriterWriteString();
    TestWriterWriteStringWithInvalidParameters();
    TestWriterWriteNumber();
    TestWriterWriteNumberWithInvalidParameters();
    TestWriterWriteSpecialNumber();
    TestWriterWriteArray();
    TestWriterWriteObject();
    TestWriterWriteSpace();
    TestWriterWriteNewLine();

    TestErrorStrings();
    TestNoLeaks();

    if (s_failureCount)
    {
        printf("Error: %d failures.\n", s_failureCount);
        printf("All tests passed.\n");
    }
    return s_failureCount;
}
