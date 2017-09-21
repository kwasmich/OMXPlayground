//
//  omxResize.c
//  OMXPlayground
//
//  Created by Michael Kwasnicki on 21.09.17.
//  Copyright © 2017 Michael Kwasnicki. All rights reserved.
//

#include "omxResize.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define OMX_SKIP64BIT
#include <IL/OMX_Broadcom.h>
#include <IL/OMX_Component.h>
#include <IL/OMX_Core.h>
#include <interface/vcos/vcos.h>

#include "cHelper.h"
#include "omxHelper.h"



typedef struct {
    OMX_HANDLETYPE handle;

    OMX_U32 inputPortIndex;
    OMX_PARAM_PORTDEFINITIONTYPE inputPortDefinition;
    OMX_CONFIG_RECTTYPE inputCommonInputCrop;
    //OMX_CONFIG_BRCMUSEPROPRIETARYCALLBACKTYPE inputBrcmUseProprietaryCallback;
    //OMX_PARAM_CAMERAPOOLTOENCODERFUNCTIONTYPE inputCameraPoolToEncoderFunction; // missing
    OMX_IMAGE_PARAM_PORTFORMATTYPE inputImagePortFormat;
    OMX_CONFIG_PORTBOOLEANTYPE inputBrcmSupportsSlices;
    OMX_BUFFERHEADERTYPE *inputBuffer;
    bool inputReady;

    OMX_U32 outputPortIndex;
    OMX_PARAM_PORTDEFINITIONTYPE outputPortDefinition;
    OMX_PARAM_RESIZETYPE outputResize;
    OMX_IMAGE_PARAM_PORTFORMATTYPE outputImagePortFormat;
    OMX_CONFIG_PORTBOOLEANTYPE outputBrcmSupportsSlices;
    OMX_BUFFERHEADERTYPE *outputBuffer;
    bool outputReady;
} OMXResize_s;



typedef struct {
    OMXResize_s resize;

    VCOS_SEMAPHORE_T handler_lock;
    VCOS_SEMAPHORE_T portChangeLock;
} OMXContext_s;



static OMX_ERRORTYPE omxEventHandler(
                                     OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_IN OMX_PTR pAppData,
                                     OMX_IN OMX_EVENTTYPE eEvent,
                                     OMX_IN OMX_U32 nData1,
                                     OMX_IN OMX_U32 nData2,
                                     OMX_IN OMX_PTR pEventData) {

    printf("eEvent: %s,  ", omxEventTypeEnum(eEvent));
    //printf("eEvent: %s,  nData1: %x,  nData2: %x\n", omxEventTypeEnum(eEvent), nData1, nData2);
    OMXContext_s* ctx = (OMXContext_s*)pAppData;

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
            printf("Port: %d    nData2: 0x%x\n", nData1, nData2);
            break;

        case OMX_EventError:
            printf(COLOR_RED "ErrorType: %s,  nData2: %x\n" COLOR_NC, omxErrorTypeEnum(nData1), nData2);

            if (nData1 != OMX_ErrorStreamCorrupt) {
                assert(NULL);
            }
            break;

        default:
            printf("nData1: 0x%x,  nData2: 0x%x\n", nData1, nData2);
            printf("unhandeled event 0x%x: 0x%x 0x%x\n", eEvent, nData1, nData2);
            break;
    }

    return OMX_ErrorNone;
}



static OMX_ERRORTYPE omxEmptyBufferDone(
                                        OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_PTR pAppData,
                                        OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    OMXContext_s *ctx = (OMXContext_s*)pAppData;
    vcos_semaphore_wait(&ctx->handler_lock);
    ctx->resize.inputReady = true;
    vcos_semaphore_post(&ctx->handler_lock);
    return OMX_ErrorNone;
}



static OMX_ERRORTYPE omxFillBufferDone(
                                       OMX_OUT OMX_HANDLETYPE hComponent,
                                       OMX_OUT OMX_PTR pAppData,
                                       OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {
    OMXContext_s *ctx = (OMXContext_s*)pAppData;
    vcos_semaphore_wait(&ctx->handler_lock);
    ctx->resize.outputReady = true;
    vcos_semaphore_post(&ctx->handler_lock);
    return OMX_ErrorNone;
}



static void omxGetPorts(OMXResize_s *component) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PORT_PARAM_TYPE ports;
    OMX_INIT_STRUCTURE(ports);
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamImageInit, &ports);
    omxAssert(omxErr);
    const OMX_U32 pEnd = ports.nStartPortNumber + ports.nPorts;

    for (OMX_U32 p = ports.nStartPortNumber; p < pEnd; p++) {
        OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
        OMX_INIT_STRUCTURE(portDefinition);
        portDefinition.nPortIndex = p;
        omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portDefinition);
        omxAssert(omxErr);

        if (portDefinition.eDir == OMX_DirInput) {
            assert(component->inputPortIndex == 0);
            component->inputPortIndex = p;
        }

        if (portDefinition.eDir == OMX_DirOutput) {
            assert(component->outputPortIndex == 0);
            component->outputPortIndex = p;
        }
    }
}



static void omxSetupInputPort(OMXResize_s *component, OMX_U32 nFrameWidth, OMX_U32 nFrameHeight, OMX_U32 nSliceHeight, OMX_U32 nChannels, OMX_COLOR_FORMATTYPE eColorFormat) {
    assert(nSliceHeight == 16);
    assert(omxAssertImagePortFormatSupported(component->handle, component->inputPortIndex, eColorFormat));

    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = &component->inputPortDefinition;
    OMX_INIT_STRUCTURE2(portDefinition);
    portDefinition->nPortIndex = component->inputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    portDefinition->format.image.nFrameWidth = nFrameWidth;
    portDefinition->format.image.nFrameHeight = nFrameHeight;
    portDefinition->format.image.nSliceHeight = 0;
    portDefinition->format.image.nStride = 0;
    portDefinition->format.image.bFlagErrorConcealment = OMX_FALSE;
    portDefinition->format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portDefinition->format.image.eColorFormat = eColorFormat;
    portDefinition->nBufferSize = nFrameWidth * nSliceHeight * nChannels;
    omxErr = OMX_SetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    OMX_CONFIG_RECTTYPE *commonInputCrop = &component->inputCommonInputCrop;
    OMX_INIT_STRUCTURE2(commonInputCrop);
    commonInputCrop->nPortIndex = component->inputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexConfigCommonInputCrop, commonInputCrop);
    omxAssert(omxErr);


    omxEnablePort(component->handle, component->inputPortIndex, OMX_TRUE);

    omxPrintPort(component->handle, component->inputPortIndex);
    printf("pos: %dx%d    dim: %dx%d\n", commonInputCrop->nLeft, commonInputCrop->nTop, commonInputCrop->nWidth, commonInputCrop->nHeight);


    omxErr = OMX_AllocateBuffer(component->handle, &component->inputBuffer, component->inputPortIndex, NULL, portDefinition->nBufferSize);
    omxAssert(omxErr);
}



static void omxSetupOutputPort(OMXResize_s *component, OMX_U32 nFrameWidth, OMX_U32 nFrameHeight, OMX_COLOR_FORMATTYPE eColorFormat) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = &component->outputPortDefinition;
    OMX_INIT_STRUCTURE2(portDefinition);
    portDefinition->nPortIndex = component->outputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    omxPrintPort(component->handle, component->outputPortIndex);

    portDefinition->format.image.nFrameWidth = nFrameWidth;
    portDefinition->format.image.nFrameHeight = nFrameHeight;
    portDefinition->format.image.nSliceHeight = 0;
    portDefinition->format.image.nStride = 0;
    portDefinition->format.image.bFlagErrorConcealment = OMX_FALSE;
    portDefinition->format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portDefinition->format.image.eColorFormat = eColorFormat;
//    portDefinition->nBufferSize = nFrameWidth * nSliceHeight * nChannels;
    omxErr = OMX_SetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    omxPrintPort(component->handle, component->outputPortIndex);


    omxEnablePort(component->handle, component->outputPortIndex, OMX_TRUE);


    omxErr = OMX_AllocateBuffer(component->handle, &component->outputBuffer, component->outputPortIndex, NULL, portDefinition->nBufferSize);
    omxAssert(omxErr);
}



static void omxFreeBuffers(OMXResize_s *component) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    omxErr = OMX_FreeBuffer(component->handle, component->inputPortIndex, component->inputBuffer);
    omxAssert(omxErr);
    omxErr = OMX_FreeBuffer(component->handle, component->outputPortIndex, component->outputBuffer);
    omxAssert(omxErr);
}



void omxResize() {
    OMX_S32 sliceHeight = 16;

    uint32_t rawImageWidth = 1440;
    uint32_t rawImageHeight = 1200;
    uint8_t rawImageChannels = 4;
    uint32_t sliceSize = rawImageWidth * sliceHeight * rawImageChannels;
    size_t rawImageSize = rawImageWidth * rawImageHeight * rawImageChannels;
    uint8_t *rawImage = (uint8_t *)malloc(rawImageSize);

    for (uint32_t y = 0; y < rawImageHeight; y++) {
        for (uint32_t x = 0; x < rawImageWidth; x++) {
            ssize_t index = (x + rawImageWidth * y) * rawImageChannels;
            rawImage[index + 0] = x % 256;
            rawImage[index + 1] = y % 256;
            rawImage[index + 2] = (x + y) % 256;
            rawImage[index + 3] = (x + y) % 256;
        }
    }


    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_STATETYPE omxState = OMX_StateInvalid;
    VCOS_STATUS_T vcosErr = VCOS_SUCCESS;

    OMXContext_s ctx;
    memset(&ctx, 0, sizeof(ctx));

    vcosErr = vcos_semaphore_create(&ctx.handler_lock, "handler_lock", 1);
    assert(vcosErr == VCOS_SUCCESS);

    OMX_STRING omxComponentName = "OMX.broadcom.resize";
    OMX_CALLBACKTYPE omxCallbacks;
    omxCallbacks.EventHandler = omxEventHandler;
    omxCallbacks.EmptyBufferDone = omxEmptyBufferDone;
    omxCallbacks.FillBufferDone = omxFillBufferDone;
    omxErr = OMX_GetHandle(&ctx.resize.handle, omxComponentName, &ctx, &omxCallbacks);
    omxAssert(omxErr);
    omxAssertState(ctx.resize.handle, OMX_StateLoaded);


    omxGetPorts(&ctx.resize);
    omxEnablePort(ctx.resize.handle, ctx.resize.inputPortIndex, OMX_FALSE);
    omxEnablePort(ctx.resize.handle, ctx.resize.outputPortIndex, OMX_FALSE);
    omxSwitchToState(ctx.resize.handle, OMX_StateIdle);
    omxSetupInputPort(&ctx.resize, rawImageWidth, rawImageHeight, sliceHeight, rawImageChannels, OMX_COLOR_Format32bitABGR8888);
    omxSetupOutputPort(&ctx.resize, 720, 600, OMX_COLOR_Format32bitABGR8888);
    omxSwitchToState(ctx.resize.handle, OMX_StateExecuting);

    //printf("%dx%d\n", ctx.resize.inputPortDefinition.format.image.nFrameWidth, ctx.resize.inputPortDefinition.format.image.nFrameHeight);

    int pos = 0;
    ctx.resize.inputReady = true;
    ctx.resize.outputReady = false;
    sliceHeight = 1200;
    sliceSize = rawImageWidth * sliceHeight * rawImageChannels;
    uint8_t *dummyBuffer = malloc(rawImageSize);

    omxErr = OMX_FillThisBuffer(ctx.resize.handle, ctx.resize.outputBuffer);
    omxAssert(omxErr);

    FILE * output = fopen("out2.data", "wb");

    while (true) {
        if (ctx.resize.outputReady) {
            ctx.resize.outputReady = false;
            //printf("got: %d\n", ctx.resize.outputBuffer->nFilledLen);
            fwrite(ctx.resize.outputBuffer->pBuffer + ctx.resize.outputBuffer->nOffset, sizeof(uint8_t), ctx.resize.outputBuffer->nFilledLen, output);

            if (ctx.resize.outputBuffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) {
                break;
            }

            omxErr = OMX_FillThisBuffer(ctx.resize.handle, ctx.resize.outputBuffer);
            omxAssert(omxErr);
        }

        if (ctx.resize.inputReady) {
            ctx.resize.inputReady = false;

            if (pos == rawImageSize) {
                continue;
            }

            memcpy(ctx.resize.inputBuffer->pBuffer, &rawImage[pos], sliceSize);
            ctx.resize.inputBuffer->nOffset = 0;
            ctx.resize.inputBuffer->nFilledLen = sliceSize;
            pos += sliceSize;

            if (pos + sliceSize > rawImageSize) {
                sliceSize = rawImageSize - pos;
            }

            omxErr = OMX_EmptyThisBuffer(ctx.resize.handle, ctx.resize.inputBuffer);
            omxAssert(omxErr);
        }
    }

    fclose(output);
    output = fopen("out1.data", "wb");
    fwrite(rawImage, sizeof(uint8_t), rawImageSize, output);
    fclose(output);


    omxSwitchToState(ctx.resize.handle, OMX_StateIdle);
    omxEnablePort(ctx.resize.handle, ctx.resize.inputPortIndex, OMX_FALSE);
    omxEnablePort(ctx.resize.handle, ctx.resize.outputPortIndex, OMX_FALSE);
    omxFreeBuffers(&ctx.resize);
    omxSwitchToState(ctx.resize.handle, OMX_StateLoaded);

    omxErr = OMX_FreeHandle(ctx.resize.handle);
    omxAssert(omxErr);
};
