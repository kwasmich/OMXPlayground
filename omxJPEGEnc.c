//
//  omxJPEGEnc.c
//  OpenMAX
//
//  Created by Michael Kwasnicki on 29.08.17.
//  Copyright © 2017 Michael Kwasnicki. All rights reserved.
//

// inspired by https://github.com/hopkinskong/rpi-omx-jpeg-encode


#include "omxJPEGEnc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define OMX_SKIP64BIT
#include <IL/OMX_Component.h>
#include <IL/OMX_Core.h>
#include <interface/vcos/vcos.h>

#include "cHelper.h"
#include "omxHelper.h"



typedef struct {
    OMX_HANDLETYPE handle;
    OMX_BUFFERHEADERTYPE* inputBuffer;
    OMX_BUFFERHEADERTYPE* outputBuffer;
    VCOS_SEMAPHORE_T handler_lock;
    OMX_U32 inputPortIndex;
    OMX_U32 outputPortIndex;
    bool inputReady;
    bool outputReady;
} ComponentContext;



static OMX_ERRORTYPE omxEventHandler(
                                     OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_IN OMX_PTR pAppData,
                                     OMX_IN OMX_EVENTTYPE eEvent,
                                     OMX_IN OMX_U32 nData1,
                                     OMX_IN OMX_U32 nData2,
                                     OMX_IN OMX_PTR pEventData) {

    printf("eEvent: %s,  ", omxEventTypeEnum(eEvent));
    //printf("eEvent: %s,  nData1: %x,  nData2: %x\n", omxEventTypeEnum(eEvent), nData1, nData2);
    ComponentContext* ctx = (ComponentContext*)pAppData;

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

        case OMX_EventError:
            printf(COLOR_RED "ErrorType: %s,  nData2: %x\n" COLOR_NC, omxErrorTypeEnum(nData1), nData2);

            if (nData1 != OMX_ErrorStreamCorrupt) {
                assert(NULL);
            }
            break;

        default:
            printf("nData1: %x,  nData2: %x\n", nData1, nData2);
            printf("unhandeled event %x: %x %x\n", eEvent, nData1, nData2);
            break;
    }

    return OMX_ErrorNone;
}



static OMX_ERRORTYPE omxEmptyBufferDone(
                                        OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_PTR pAppData,
                                        OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    ComponentContext *ctx = (ComponentContext*)pAppData;
    vcos_semaphore_wait(&ctx->handler_lock);
    ctx->inputReady = true;
    vcos_semaphore_post(&ctx->handler_lock);
    return OMX_ErrorNone;
}



static OMX_ERRORTYPE omxFillBufferDone(
                                       OMX_OUT OMX_HANDLETYPE hComponent,
                                       OMX_OUT OMX_PTR pAppData,
                                       OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {
    ComponentContext *ctx = (ComponentContext*)pAppData;
    vcos_semaphore_wait(&ctx->handler_lock);
    ctx->outputReady = true;
    vcos_semaphore_post(&ctx->handler_lock);
    return OMX_ErrorNone;
}



static void omxGetPorts(ComponentContext *ctx) {
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



static void omxSetupInputPort(ComponentContext *ctx, OMX_U32 nFrameWidth, OMX_U32 nFrameHeight, OMX_U32 nSliceHeight, OMX_U32 nChannels, OMX_COLOR_FORMATTYPE eColorFormat) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = ctx->inputPortIndex;
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);

    portDefinition.format.image.nFrameWidth = nFrameWidth;
    portDefinition.format.image.nFrameHeight = nFrameHeight;
    portDefinition.format.image.nSliceHeight = nSliceHeight;
    portDefinition.format.image.nStride = 0;
    portDefinition.format.image.bFlagErrorConcealment = OMX_FALSE;
    portDefinition.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portDefinition.format.image.eColorFormat = eColorFormat;
    portDefinition.nBufferSize = nFrameWidth * nSliceHeight * nChannels;
    omxErr = OMX_SetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);


    omxEnablePort(ctx->handle, ctx->inputPortIndex, OMX_TRUE);


    omxErr = OMX_AllocateBuffer(ctx->handle, &ctx->inputBuffer, ctx->inputPortIndex, NULL, portDefinition.nBufferSize);
    omxAssert(omxErr);
}



static void omxSetupOutputPort(ComponentContext *ctx, OMX_U32 nQFactor) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = ctx->outputPortIndex;
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);

    portDefinition.format.image.bFlagErrorConcealment = OMX_FALSE;
    portDefinition.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
    portDefinition.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
    omxErr = OMX_SetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);

    OMX_IMAGE_PARAM_QFACTORTYPE qFactor;
    OMX_INIT_STRUCTURE(qFactor);
    qFactor.nPortIndex = ctx->outputPortIndex;
    qFactor.nQFactor = nQFactor;
    omxErr = OMX_SetParameter(ctx->handle, OMX_IndexParamQFactor, &qFactor);
    omxAssert(omxErr);


    omxEnablePort(ctx->handle, ctx->outputPortIndex, OMX_TRUE);


    omxErr = OMX_AllocateBuffer(ctx->handle, &ctx->outputBuffer, ctx->outputPortIndex, NULL, portDefinition.nBufferSize);
    omxAssert(omxErr);
}



static void omxFreeBuffers(ComponentContext *ctx) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    omxErr = OMX_FreeBuffer(ctx->handle, ctx->inputPortIndex, ctx->inputBuffer);
    omxAssert(omxErr);
    omxErr = OMX_FreeBuffer(ctx->handle, ctx->outputPortIndex, ctx->outputBuffer);
    omxAssert(omxErr);
}



void omxJPEGEnc() {
    OMX_U32 outputQuality = 7;      // [1, 100] as in libJPEG 9
    OMX_S32 sliceHeight = 128;

    uint32_t rawImageWidth = 1440;
    uint32_t rawImageHeight = 1200;
    uint8_t rawImageChannels = 3;
    uint32_t sliceSize = rawImageWidth * sliceHeight * rawImageChannels;
    size_t rawImageSize = rawImageWidth * rawImageHeight * rawImageChannels;
    uint8_t *rawImage = (uint8_t *)malloc(rawImageSize);

    for (uint32_t y = 0; y < rawImageHeight; y++) {
        for (uint32_t x = 0; x < rawImageWidth; x++) {
            ssize_t index = (x + rawImageWidth * y) * rawImageChannels;
            rawImage[index + 0] = x % 256;
            rawImage[index + 1] = y % 256;
            rawImage[index + 2] = (x + y) % 256;
        }
    }


    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_STATETYPE omxState = OMX_StateInvalid;
    VCOS_STATUS_T vcosErr = VCOS_SUCCESS;

    ComponentContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    vcosErr = vcos_semaphore_create(&ctx.handler_lock, "handler_lock", 1);
    assert(vcosErr == VCOS_SUCCESS);

    OMX_STRING omxComponentName = "OMX.broadcom.image_encode";
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
    omxSetupInputPort(&ctx, rawImageWidth, rawImageHeight, sliceHeight, rawImageChannels, OMX_COLOR_Format24bitBGR888);
    omxSetupOutputPort(&ctx, outputQuality);
    omxSwitchToState(ctx.handle, OMX_StateExecuting);


    int pos = 0;
    ctx.inputReady = true;
    ctx.outputReady = false;

    omxErr = OMX_FillThisBuffer(ctx.handle, ctx.outputBuffer);
    omxAssert(omxErr);

    FILE * output = fopen("out.jpg", "wb");

    while (true) {
        if (ctx.outputReady) {
            ctx.outputReady = false;
            fwrite(ctx.outputBuffer->pBuffer + ctx.outputBuffer->nOffset, sizeof(uint8_t), ctx.outputBuffer->nFilledLen, output);

            if (ctx.outputBuffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) {
                break;
            }

            omxErr = OMX_FillThisBuffer(ctx.handle, ctx.outputBuffer);
            omxAssert(omxErr);
        }

        if (ctx.inputReady) {
            ctx.inputReady = false;

            if (pos == rawImageSize) {
                continue;
            }

            memcpy(ctx.inputBuffer->pBuffer, &rawImage[pos], sliceSize);
            ctx.inputBuffer->nOffset = 0;
            ctx.inputBuffer->nFilledLen = sliceSize;
            pos += sliceSize;

            if (pos + sliceSize > rawImageSize) {
                sliceSize = rawImageSize - pos;
            }

            omxErr = OMX_EmptyThisBuffer(ctx.handle, ctx.inputBuffer);
            omxAssert(omxErr);
        }
    }

    fclose(output);
    puts(COLOR_RED "done" COLOR_NC " blaub " COLOR_GREEN "yes" COLOR_NC);


    omxSwitchToState(ctx.handle, OMX_StateIdle);
    omxEnablePort(ctx.handle, ctx.inputPortIndex, OMX_FALSE);
    omxEnablePort(ctx.handle, ctx.outputPortIndex, OMX_FALSE);
    omxFreeBuffers(&ctx);
    omxSwitchToState(ctx.handle, OMX_StateLoaded);

    
    omxErr = OMX_FreeHandle(ctx.handle);
    omxAssert(omxErr);
}
