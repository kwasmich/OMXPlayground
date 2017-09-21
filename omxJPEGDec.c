//
//  omxJPEGDec.c
//  OpenMAX
//
//  Created by Michael Kwasnicki on 29.08.17.
//  Copyright © 2017 Michael Kwasnicki. All rights reserved.
//

#include "omxJPEGDec.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include <sys/param.h>  // MIN

#define OMX_SKIP64BIT
#include <IL/OMX_Broadcom.h>
#include <IL/OMX_Component.h>
#include <IL/OMX_Core.h>
#include <interface/vcos/vcos.h>

#include "cHelper.h"
#include "mmapHelper.h"
#include "omxHelper.h"
#include "omxDump.h"



typedef struct {
    OMX_HANDLETYPE handle;

    OMX_U32 inputPortIndex;
    //    OMX_PARAM_PORTDEFINITIONTYPE *inputPortDefinition;
    //    OMX_IMAGE_PARAM_PORTFORMATTYPE *inputImagePortFormat;
    OMX_BUFFERHEADERTYPE *inputBuffer[3];
    bool inputReady;

    OMX_U32 outputPortIndex;
    //    OMX_PARAM_PORTDEFINITIONTYPE *outputPortDefinition;
    //    OMX_PARAM_U32TYPE *outputNumAvailableStreams;
    //    OMX_PARAM_U32TYPE *outputActiveStream;
    //    OMX_IMAGE_PARAM_PORTFORMATTYPE *outputImagePortFormat;
    //    OMX_CONFIG_METADATAITEMCOUNTTYPE *outputMetadataItemCount;
    //    OMX_CONFIG_METADATAITEMTYPE *outputMetadataItem;
    //    OMX_PARAM_METADATAFILTERTYPE *outputMetadataFilterType;
    //    OMX_CONFIG_CONTAINERNODECOUNTTYPE *outputContainerNodeCount;
    //    OMX_CONFIG_CONTAINERNODEIDTYPE *outputCounterNodeID;
    //    OMX_PARAM_COLORSPACETYPE *outputColorSpace;
    OMX_BUFFERHEADERTYPE *outputBuffer;
    bool outputReady;

    VCOS_SEMAPHORE_T handler_lock;
    VCOS_SEMAPHORE_T portChangeLock;
} OMXImageDecode_s;







static OMX_ERRORTYPE omxEventHandler(
                                     OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_IN OMX_PTR pAppData,
                                     OMX_IN OMX_EVENTTYPE eEvent,
                                     OMX_IN OMX_U32 nData1,
                                     OMX_IN OMX_U32 nData2,
                                     OMX_IN OMX_PTR pEventData) {

    printf("eEvent: %s,  ", omxEventTypeEnum(eEvent));
    //printf("eEvent: %s,  nData1: %x,  nData2: %x\n", omxEventTypeEnum(eEvent), nData1, nData2);
    OMXImageDecode_s* ctx = (OMXImageDecode_s*)pAppData;

    switch(eEvent) {
        case OMX_EventCmdComplete:
            printf("Command: %s,  ", omxCommandTypeEnum(nData1));

            switch (nData1) {
                case OMX_CommandStateSet:
                    printf("State: %s\n", omxStateTypeEnum(nData2));
                    break;

                case OMX_CommandPortDisable:
                case OMX_CommandPortEnable:
                    printf("Port: %d\n", nData2);
                    break;

                default:
                    printf("nData2: 0x%x\n", nData2);
            }

            break;

        case OMX_EventPortSettingsChanged:
            printf("Port: %d  nData2: %x\n", nData1, nData2);
            vcos_semaphore_post(&ctx->portChangeLock);
            break;

        case OMX_EventError:
            printf(COLOR_RED "ErrorType: %s,  nData2: %x\n" COLOR_NC, omxErrorTypeEnum(nData1), nData2);

            if (nData1 != OMX_ErrorStreamCorrupt) {
                assert(NULL);
            }
            break;

        default:
            printf("unhandeled event 0x%08x: 0x%08x 0x%08x\n", eEvent, nData1, nData2);
            break;
    }

    return OMX_ErrorNone;
}



static OMX_ERRORTYPE omxEmptyBufferDone(
                                        OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_PTR pAppData,
                                        OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    puts("omxEmptyBufferDone");
    OMXImageDecode_s *ctx = (OMXImageDecode_s*)pAppData;
    vcos_semaphore_wait(&ctx->handler_lock);
    ctx->inputReady = true;
    vcos_semaphore_post(&ctx->handler_lock);
    return OMX_ErrorNone;
}



static OMX_ERRORTYPE omxFillBufferDone(
                                       OMX_OUT OMX_HANDLETYPE hComponent,
                                       OMX_OUT OMX_PTR pAppData,
                                       OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {
    puts("omxFillBufferDone");
    OMXImageDecode_s *ctx = (OMXImageDecode_s*)pAppData;
    vcos_semaphore_wait(&ctx->handler_lock);
    ctx->outputReady = true;
    vcos_semaphore_post(&ctx->handler_lock);
    return OMX_ErrorNone;
}



static void setupInputPort(OMXImageDecode_s *ctx, OMX_IMAGE_CODINGTYPE format) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = ctx->inputPortIndex;
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);

    //omxPrintPort(ctx->handle, ctx->inputPortIndex);

    OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
    OMX_INIT_STRUCTURE(imagePortFormat);
    imagePortFormat.nPortIndex = ctx->inputPortIndex;
    imagePortFormat.eCompressionFormat = format;
    omxErr = OMX_SetParameter(ctx->handle, OMX_IndexParamImagePortFormat, &imagePortFormat);
    omxAssert(omxErr);

    omxEnablePort(ctx->handle, ctx->inputPortIndex, OMX_TRUE);

    for (int i = 0; i < portDefinition.nBufferCountActual; i++) {
        omxErr = OMX_AllocateBuffer(ctx->handle, &ctx->inputBuffer[i], ctx->inputPortIndex, i, portDefinition.nBufferSize);
        omxAssert(omxErr);
    }
}



static void prepareOutputPort(OMXImageDecode_s *ctx) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = ctx->outputPortIndex;
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);
    portDefinition.format.image.eCompressionFormat = OMX_IMAGE_CodingAutoDetect;
    omxErr = OMX_SetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);
}



static void setupOutputPort(OMXImageDecode_s *ctx) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = ctx->outputPortIndex;
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);

    OMX_PARAM_U32TYPE numAvailableStreams;
    OMX_INIT_STRUCTURE(numAvailableStreams);
    numAvailableStreams.nPortIndex = ctx->outputPortIndex;
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamNumAvailableStreams, &numAvailableStreams);
    omxAssert(omxErr);
    assert(numAvailableStreams.nU32 > 0);

    //printf("%d x %d\n", portDefinition.format.image.nFrameWidth, portDefinition.format.image.nFrameHeight);
    //omxPrintPort(ctx->handle, ctx->outputPortIndex);

    omxEnablePort(ctx->handle, ctx->outputPortIndex, OMX_TRUE);

    omxErr = OMX_AllocateBuffer(ctx->handle, &ctx->outputBuffer, ctx->outputPortIndex, NULL, portDefinition.nBufferSize);
    omxAssert(omxErr);
}



static void omxGetPorts(OMXImageDecode_s *ctx) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PORT_PARAM_TYPE ports;
    OMX_INIT_STRUCTURE(ports);
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamImageInit, &ports);
    omxAssert(omxErr);
    const OMX_U32 pEnd = ports.nStartPortNumber + ports.nPorts;

    for (OMX_U32 p = ports.nStartPortNumber; p < pEnd; p++) {
        OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
        OMX_INIT_STRUCTURE(portDefinition);
        portDefinition.nPortIndex = p;
        omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
        omxAssert(omxErr);

        if (portDefinition.eDir == OMX_DirInput) {
            assert(ctx->inputPortIndex == 0);
            ctx->inputPortIndex = p;
        }

        if (portDefinition.eDir == OMX_DirOutput) {
            assert(ctx->outputPortIndex == 0);
            ctx->outputPortIndex = p;
        }
    }
}



void omxJPEGDec() {
    MapFile_s map;
    initMapFile(&map, "36903_9_1.jpg", MAP_RO);
    assert(map.len > 0);
    uint8_t *jpegData = map.data;
    printf("jpegDataSize: %zu\n", map.len);

    uint32_t rawImageWidth = 0;
    uint32_t rawImageHeight = 0;
    uint8_t rawImageChannels = 0;
    uint8_t *rawImage = NULL;


    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_STATETYPE omxState = OMX_StateInvalid;
    VCOS_STATUS_T vcosErr = VCOS_SUCCESS;

    bcm_host_init();
    omxErr = OMX_Init();
    omxAssert(omxErr);

    //omxListComponents();

    OMXImageDecode_s ctx;
    memset(&ctx, 0, sizeof(ctx));
    //ctx.inputPortDefinition = malloc(sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

    vcosErr = vcos_semaphore_create(&ctx.handler_lock, "handler_lock", 1);
    assert(vcosErr == VCOS_SUCCESS);
    vcosErr = vcos_semaphore_create(&ctx.portChangeLock, "portChangeLock", 1);
    assert(vcosErr == VCOS_SUCCESS);

    //OMX_HANDLETYPE ctx.handle = NULL;
    OMX_STRING omxComponentName = "OMX.broadcom.image_decode";
    OMX_CALLBACKTYPE omxCallbacks;
    omxCallbacks.EventHandler = omxEventHandler;
    omxCallbacks.EmptyBufferDone = omxEmptyBufferDone;
    omxCallbacks.FillBufferDone = omxFillBufferDone;

    omxErr = OMX_GetHandle(&ctx.handle, omxComponentName, &ctx, &omxCallbacks);
    omxAssert(omxErr);
    omxAssertState(ctx.handle, OMX_StateLoaded);


    omxGetPorts(&ctx);
    omxEnablePort(ctx.handle, ctx.inputPortIndex, OMX_FALSE);
    omxEnablePort(ctx.handle, ctx.outputPortIndex, OMX_FALSE);
    omxSwitchToState(ctx.handle, OMX_StateIdle);
    setupInputPort(&ctx, OMX_IMAGE_CodingJPEG);
    prepareOutputPort(&ctx);
    omxSwitchToState(ctx.handle, OMX_StateExecuting);


    uint8_t *jpegDataPtr = jpegData;
    size_t jpegDataRemaining = map.len;
    int bufferIndex = 0;
    ctx.inputReady = true;
    ctx.outputReady = false;

    FILE * output = fopen("out.data", "wb");

    while (true) {
        if (ctx.outputReady) {
            ctx.outputReady = false;

            fwrite(ctx.outputBuffer->pBuffer + ctx.outputBuffer->nOffset, sizeof(uint8_t), ctx.outputBuffer->nFilledLen, output);

            printf("nFilledLen: %d\n", ctx.outputBuffer->nFilledLen);
            printf("nFlags: 0x%08x\n", ctx.outputBuffer->nFlags);

            if (ctx.outputBuffer->nFlags & OMX_BUFFERFLAG_EOS) {
                puts("received OMX_BUFFERFLAG_EOS");
                break;
            }

            puts("OMX_FillThisBuffer");
            omxErr = OMX_FillThisBuffer(ctx.handle, ctx.outputBuffer);
            omxAssert(omxErr);
        }

        if (ctx.inputReady) {
            ctx.inputReady = false;

            if (jpegDataRemaining == 0) {
                continue;
            }

            OMX_BUFFERHEADERTYPE *inBuffer = ctx.inputBuffer[bufferIndex];
            bufferIndex++;
            bufferIndex %= 3;
            inBuffer->nFilledLen = MIN(jpegDataRemaining, inBuffer->nAllocLen);
            jpegDataRemaining -= inBuffer->nFilledLen;
            memcpy(inBuffer->pBuffer, jpegDataPtr, inBuffer->nFilledLen);
            jpegDataPtr += inBuffer->nFilledLen;

            inBuffer->nOffset = 0;
            inBuffer->nFlags = 0;

            if (jpegDataRemaining <= 0) {
                puts("signaling OMX_BUFFERFLAG_EOS");
                inBuffer->nFlags = OMX_BUFFERFLAG_EOS;
            }

            puts("OMX_EmptyThisBuffer");
            omxErr = OMX_EmptyThisBuffer(ctx.handle, inBuffer);
            omxAssert(omxErr);

            if (!ctx.outputBuffer) {
                puts("wait for port change...");
                vcos_semaphore_wait(&ctx.portChangeLock);
                puts("continuing...");

                setupOutputPort(&ctx);

                puts("OMX_FillThisBuffer");
                omxErr = OMX_FillThisBuffer(ctx.handle, ctx.outputBuffer);
                omxAssert(omxErr);
            }
        }
    }

    fclose(output);
    freeMapFile(&map);

    omxSwitchToState(ctx.handle, OMX_StateIdle);
    omxEnablePort(ctx.handle, ctx.inputPortIndex, OMX_FALSE);
    omxEnablePort(ctx.handle, ctx.outputPortIndex, OMX_FALSE);

    // Deallocate input buffer
    {
        OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
        OMX_INIT_STRUCTURE(portDefinition);
        portDefinition.nPortIndex = ctx.inputPortIndex;
        omxErr = OMX_GetParameter(ctx.handle, OMX_IndexParamPortDefinition, &portDefinition);
        omxAssert(omxErr);

        for (int i = 0; i < portDefinition.nBufferCountActual; i++) {
            omxErr = OMX_FreeBuffer(ctx.handle, ctx.inputPortIndex, ctx.inputBuffer[i]);
            omxAssert(omxErr);
        }
    }
    
    // Deallocate output buffer
    {
        omxErr = OMX_FreeBuffer(ctx.handle, ctx.outputPortIndex, ctx.outputBuffer);
        omxAssert(omxErr);
    }
    
    omxSwitchToState(ctx.handle, OMX_StateLoaded);
    omxErr = OMX_FreeHandle(ctx.handle);
    omxAssert(omxErr);
    
    // insert code here...
    printf("Hello, World!\n");
    return 0;
}
