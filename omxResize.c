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
#include "mmapHelper.h"
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



typedef struct {
    OMX_U32 nWidth;
    OMX_U32 nHeight;
} OMXSize_t;


typedef struct {
    OMX_S32 nLeft;
    OMX_S32 nTop;
    OMX_U32 nWidth;
    OMX_U32 nHeight;
} OMXRect_t;



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



static void getPorts(OMXResize_s *component) {
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



static void setupInputPort(OMXResize_s *component, OMXSize_t frameSize, OMXRect_t cropRect, OMX_COLOR_FORMATTYPE eColorFormat) {
    assert(omxAssertImagePortFormatSupported(component->handle, component->inputPortIndex, eColorFormat));

    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_CONFIG_PORTBOOLEANTYPE *brcmSupportsSlices = &component->inputBrcmSupportsSlices;
    OMX_INIT_STRUCTURE2(brcmSupportsSlices);
    brcmSupportsSlices->nPortIndex = component->inputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamBrcmSupportsSlices, brcmSupportsSlices);
    omxAssert(omxErr);

    OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = &component->inputPortDefinition;
    OMX_INIT_STRUCTURE2(portDefinition);
    portDefinition->nPortIndex = component->inputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    portDefinition->format.image.nFrameWidth = frameSize.nWidth;
    portDefinition->format.image.nFrameHeight = frameSize.nHeight;
    portDefinition->format.image.nSliceHeight = (brcmSupportsSlices->bEnabled == OMX_TRUE) ? 16 : 0;
    portDefinition->format.image.nStride = 0;
    portDefinition->format.image.bFlagErrorConcealment = OMX_FALSE;
    portDefinition->format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portDefinition->format.image.eColorFormat = eColorFormat;
    omxErr = OMX_SetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    OMX_CONFIG_RECTTYPE *commonInputCrop = &component->inputCommonInputCrop;
    OMX_INIT_STRUCTURE2(commonInputCrop);
    commonInputCrop->nPortIndex = component->inputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexConfigCommonInputCrop, commonInputCrop);
    omxAssert(omxErr);

    commonInputCrop->nWidth = cropRect.nWidth;
    commonInputCrop->nHeight = cropRect.nHeight;
    commonInputCrop->nLeft = cropRect.nLeft;
    commonInputCrop->nTop = cropRect.nTop;
    omxErr = OMX_SetParameter(component->handle, OMX_IndexConfigCommonInputCrop, commonInputCrop);
    omxAssert(omxErr);

    omxErr = OMX_GetParameter(component->handle, OMX_IndexConfigCommonInputCrop, commonInputCrop);
    omxAssert(omxErr);


    omxEnablePort(component->handle, component->inputPortIndex, OMX_TRUE);

    omxPrintPort(component->handle, component->inputPortIndex);
    printf("pos: %dx%d    dim: %dx%d\n", commonInputCrop->nLeft, commonInputCrop->nTop, commonInputCrop->nWidth, commonInputCrop->nHeight);


    omxErr = OMX_AllocateBuffer(component->handle, &component->inputBuffer, component->inputPortIndex, NULL, portDefinition->nBufferSize);
    omxAssert(omxErr);
}



static void setupOutputPort(OMXResize_s *component, OMXSize_t frameSize, OMX_COLOR_FORMATTYPE eColorFormat) {
    assert(omxAssertImagePortFormatSupported(component->handle, component->outputPortIndex, eColorFormat));

    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_CONFIG_PORTBOOLEANTYPE *brcmSupportsSlices = &component->outputBrcmSupportsSlices;
    OMX_INIT_STRUCTURE2(brcmSupportsSlices);
    brcmSupportsSlices->nPortIndex = component->outputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamBrcmSupportsSlices, brcmSupportsSlices);
    omxAssert(omxErr);

    OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = &component->outputPortDefinition;
    OMX_INIT_STRUCTURE2(portDefinition);
    portDefinition->nPortIndex = component->outputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    omxPrintPort(component->handle, component->outputPortIndex);

    portDefinition->format.image.nFrameWidth = frameSize.nWidth;
    portDefinition->format.image.nFrameHeight = frameSize.nHeight;
    portDefinition->format.image.nSliceHeight = (brcmSupportsSlices->bEnabled == OMX_TRUE) ? 16 : frameSize.nHeight;
    portDefinition->format.image.nStride = 0;
    portDefinition->format.image.bFlagErrorConcealment = OMX_FALSE;
    portDefinition->format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portDefinition->format.image.eColorFormat = eColorFormat;
    omxErr = OMX_SetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
    omxAssert(omxErr);

    omxPrintPort(component->handle, component->outputPortIndex);


    omxEnablePort(component->handle, component->outputPortIndex, OMX_TRUE);


    omxErr = OMX_AllocateBuffer(component->handle, &component->outputBuffer, component->outputPortIndex, NULL, portDefinition->nBufferSize);
    omxAssert(omxErr);
}



static void freeBuffers(OMXResize_s *component) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    omxErr = OMX_FreeBuffer(component->handle, component->inputPortIndex, component->inputBuffer);
    omxAssert(omxErr);
    omxErr = OMX_FreeBuffer(component->handle, component->outputPortIndex, component->outputBuffer);
    omxAssert(omxErr);
}



void omxResize() {
    uint32_t rawImageWidth = 640;
    uint32_t rawImageHeight = 480;
    uint8_t rawImageChannels = 4;
    size_t rawImageSize = rawImageWidth * rawImageHeight * rawImageChannels;
    uint8_t *rawImage = (uint8_t *)malloc(rawImageSize);

    for (uint32_t y = 0; y < rawImageHeight; y++) {
        for (uint32_t x = 0; x < rawImageWidth; x++) {
            ssize_t index = (x + rawImageWidth * y) * rawImageChannels;
            rawImage[index + 0] = x % 256;
            rawImage[index + 1] = y % 256;
            rawImage[index + 2] = (x + y) % 256;
            rawImage[index + 3] = 255;
        }
    }

    OMXSize_t inputFrameSize = { .nWidth = rawImageWidth, .nHeight = rawImageHeight };
    //OMXRect_t inputFrameCrop = { .nWidth = 256, .nHeight = 256, .nLeft = 128, .nTop = 128 };
    OMXRect_t inputFrameCrop = { .nWidth = 0, .nHeight = 0, .nLeft = 0, .nTop = 0 };
    OMXSize_t outputFrameSize = { .nWidth = rawImageWidth, .nHeight = rawImageHeight };


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


    getPorts(&ctx.resize);
    omxEnablePort(ctx.resize.handle, ctx.resize.inputPortIndex, OMX_FALSE);
    omxEnablePort(ctx.resize.handle, ctx.resize.outputPortIndex, OMX_FALSE);
    omxSwitchToState(ctx.resize.handle, OMX_StateIdle);
    setupInputPort(&ctx.resize, inputFrameSize, inputFrameCrop, OMX_COLOR_Format32bitABGR8888);
    setupOutputPort(&ctx.resize, outputFrameSize, OMX_COLOR_Format32bitABGR8888);
    omxSwitchToState(ctx.resize.handle, OMX_StateExecuting);


    int pos = 0;
    ctx.resize.inputReady = true;
    ctx.resize.outputReady = false;
    uint32_t sliceSize = ctx.resize.inputBuffer->nAllocLen;

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
    freeBuffers(&ctx.resize);
    omxSwitchToState(ctx.resize.handle, OMX_StateLoaded);
    omxErr = OMX_FreeHandle(ctx.resize.handle);
    omxAssert(omxErr);
};
