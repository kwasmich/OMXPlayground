//
//  omxDump.c
//  OpenMAX
//
//  Created by Michael Kwasnicki on 29.08.17.
//  Copyright © 2017 Michael Kwasnicki. All rights reserved.
//

#include "omxDump.h"



#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define OMX_SKIP64BIT
#include <IL/OMX_Component.h>
#include <IL/OMX_Core.h>

#include "cHelper.h"
#include "omxHelper.h"




static void omxListComponents() {
    char stringBackingStore[256];
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_STRING componentName = stringBackingStore;

    puts(COLOR_YELLOW "*************************************" COLOR_NC);
    puts(COLOR_YELLOW "**  Listing OpenMAX IL Components  **" COLOR_NC);
    puts(COLOR_YELLOW "*************************************" COLOR_NC);

    for (int i = 0; omxErr == OMX_ErrorNone; i++) {
        omxErr = OMX_ComponentNameEnum(componentName, 256, i);

        if (omxErr == OMX_ErrorNone) {
            printf("%2i: %s\n", i, componentName);
        }
    }

    puts("");
}



static void omxPrintComponentVersion(OMX_HANDLETYPE omxHandle) {
    char stringBackingStore[256];
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_STRING pComponentName = stringBackingStore;
    OMX_VERSIONTYPE pComponentVersion;
    OMX_VERSIONTYPE pSpecVersion;
    OMX_UUIDTYPE pComponentUUID;
    omxErr = OMX_GetComponentVersion(omxHandle, pComponentName, &pComponentVersion, &pSpecVersion, &pComponentUUID);
    omxAssert(omxErr);

    puts(COLOR_MAGENTA "**  Component Version  **" COLOR_NC);
    printf(LEVEL_1 "pComponentName:    %s\n", pComponentName);
    printf(LEVEL_1 "pComponentVersion: %d.%d.%d.%d\n", pComponentVersion.s.nVersionMajor, pComponentVersion.s.nVersionMinor, pComponentVersion.s.nRevision, pComponentVersion.s.nStep);
    printf(LEVEL_1 "pSpecVersion:      %d.%d.%d.%d\n", pSpecVersion.s.nVersionMajor, pSpecVersion.s.nVersionMinor, pSpecVersion.s.nRevision, pSpecVersion.s.nStep);
    printf(LEVEL_1 "pComponentUUID:    %s\n", pComponentUUID);
    puts("");
}




static void omxListImagePortFormats(OMX_HANDLETYPE omxHandle, OMX_U32 nPortIndex) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_IMAGE_PARAM_PORTFORMATTYPE portformat;
    OMX_INIT_STRUCTURE(portformat);
    portformat.nPortIndex = nPortIndex;
    int i = 0;

    do {
        portformat.nIndex = i;
        omxErr = OMX_GetParameter(omxHandle, OMX_IndexParamImagePortFormat, &portformat);

        if (omxErr == OMX_ErrorNone) {
            //printf(LEVEL_2 "%d:\n", i);

            if (portformat.eCompressionFormat != OMX_IMAGE_CodingUnused) {
                printf(LEVEL_3 "eCompressionFormat: %s\n", omxImageCodingTypeEnum(portformat.eCompressionFormat));
            }

            if (portformat.eColorFormat != OMX_COLOR_FormatUnused) {
                printf(LEVEL_3 "eColorFormat:       %s\n", omxColorFormatTypeEnum(portformat.eColorFormat));
            }

            i++;
        }
    } while (omxErr == OMX_ErrorNone);
}



void omxPrintPort(OMX_HANDLETYPE omxHandle, OMX_U32 portIndex) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = portIndex;
    omxErr = OMX_GetParameter(omxHandle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);
    puts("");
    puts(LEVEL_1 COLOR_CYAN "**  Port  **" COLOR_NC);

    omxDumpParamPortDefinition(portDefinition);

    if (portDefinition.eDomain == OMX_PortDomainImage) {
        puts("");
        puts(LEVEL_2 COLOR_RED "**  Image Domain  **" COLOR_NC);

        omxDumpImagePortDefinition(portDefinition.format.image);

        puts("");
        puts(LEVEL_2 COLOR_RED "**  Supported Image Formats  **" COLOR_NC);
        omxListImagePortFormats(omxHandle, portIndex);
    } else {
        assert(false);
    }
}



static void omxListPorts(OMX_HANDLETYPE omxHandle) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PORT_PARAM_TYPE ports;
    OMX_INIT_STRUCTURE(ports);

    OMX_INDEXTYPE types[] = {
        OMX_IndexParamAudioInit,
        OMX_IndexParamVideoInit,
        OMX_IndexParamImageInit,
        OMX_IndexParamOtherInit
    };

    char *typeStrings[] = {
        "OMX_IndexParamAudioInit",
        "OMX_IndexParamVideoInit",
        "OMX_IndexParamImageInit",
        "OMX_IndexParamOtherInit"
    };

    puts(COLOR_MAGENTA "**  Component Ports  **" COLOR_NC);

    for (int i = 0; i < 4; i++) {
        omxErr == OMX_GetParameter(omxHandle, types[i], &ports);
        omxAssert(omxErr);

        if (ports.nPorts > 0) {
            printf(LEVEL_1 "Domain: %s\n", typeStrings[i]);

            for (int p = ports.nStartPortNumber; p < ports.nStartPortNumber + ports.nPorts; p++) {
                omxPrintPort(omxHandle, p);
            }
        }
    }

    puts("");
}



static OMX_ERRORTYPE omxEventHandler(
                              OMX_IN OMX_HANDLETYPE hComponent,
                              OMX_IN OMX_PTR pAppData,
                              OMX_IN OMX_EVENTTYPE eEvent,
                              OMX_IN OMX_U32 nData1,
                              OMX_IN OMX_U32 nData2,
                              OMX_IN OMX_PTR pEventData) {

    printf("eEvent: %x,  nData1: %x,  nData2: %x\n", eEvent, nData1, nData2);

    switch(eEvent) {
        case OMX_EventCmdComplete:
            break;

        case OMX_EventError:
            printf(COLOR_RED "ErrorType: %s,  nData2: %x\n" COLOR_NC, omxErrorTypeEnum(nData1), nData2);

            if (nData1 != OMX_ErrorStreamCorrupt) {
                assert(NULL);
            }
            break;

        default:
            printf("unhandeled event %x: %x %x\n", eEvent, nData1, nData2);
            break;
    }

    return OMX_ErrorNone;
}



static OMX_ERRORTYPE omxEmptyBufferDone(
                                 OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_PTR pAppData,
                                 OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    return OMX_ErrorNone;
}



static OMX_ERRORTYPE omxFillBufferDone(
                                OMX_OUT OMX_HANDLETYPE hComponent,
                                OMX_OUT OMX_PTR pAppData,
                                OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {
    return OMX_ErrorNone;
}



void omxDump() {
    char stringBackingStore[256];
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_STATETYPE omxState = OMX_StateInvalid;


    omxListComponents();


    OMX_STRING omxComponentName = stringBackingStore;
    omxErr = OMX_ComponentNameEnum(omxComponentName, 256, 14);
    omxAssert(omxErr);

    puts(COLOR_GREEN "*************************************************************" COLOR_NC);
    printf(COLOR_GREEN "**  Showing informations for %28s  **\n" COLOR_NC, omxComponentName);
    puts(COLOR_GREEN "*************************************************************" COLOR_NC);

    OMX_HANDLETYPE omxHandle;
    OMX_CALLBACKTYPE omxCallbacks;
    omxCallbacks.EventHandler = omxEventHandler;
    omxCallbacks.EmptyBufferDone = omxEmptyBufferDone;
    omxCallbacks.FillBufferDone = omxFillBufferDone;
    omxErr = OMX_GetHandle(&omxHandle, omxComponentName, NULL, &omxCallbacks);
    omxAssert(omxErr);
    omxErr = OMX_GetState(omxHandle, &omxState);
    omxAssert(omxErr);
    assert(omxState == OMX_StateLoaded);


    omxPrintComponentVersion(omxHandle);
    omxListPorts(omxHandle);

    
    omxErr = OMX_FreeHandle(omxHandle);
    omxAssert(omxErr);
    
    // insert code here...
    printf("Hello, World!\n");
}
