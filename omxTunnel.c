//
//  omxTunnel.c
//  OMXPlayground
//
//  Created by Michael Kwasnicki on 27.09.17.
//  Copyright © 2017 Michael Kwasnicki. All rights reserved.
//

#include "omxTunnel.h"

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
} OMXImageDecode_s;



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
    OMXImageDecode_s imageDecode;
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
            vcos_semaphore_post(&ctx->portChangeLock);
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
    ctx->imageDecode.inputReady = true;
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
    ctx->imageDecode.outputReady = true;
    ctx->resize.outputReady = true;
    vcos_semaphore_post(&ctx->handler_lock);
    return OMX_ErrorNone;
}



static void getImageDecodePorts(OMXImageDecode_s *component) {
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



static void getResizePorts(OMXResize_s *component) {
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



static void setupImageDecodeInputPort(OMXImageDecode_s *component, OMX_IMAGE_CODINGTYPE format) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = component->inputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);

    //omxPrintPort(ctx->handle, ctx->inputPortIndex);

    OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
    OMX_INIT_STRUCTURE(imagePortFormat);
    imagePortFormat.nPortIndex = component->inputPortIndex;
    imagePortFormat.eCompressionFormat = format;
    omxErr = OMX_SetParameter(component->handle, OMX_IndexParamImagePortFormat, &imagePortFormat);
    omxAssert(omxErr);

    omxEnablePort(component->handle, component->inputPortIndex, OMX_TRUE);

    for (int i = 0; i < portDefinition.nBufferCountActual; i++) {
        omxErr = OMX_AllocateBuffer(component->handle, &component->inputBuffer[i], component->inputPortIndex, i, portDefinition.nBufferSize);
        omxAssert(omxErr);
    }
}



static void prepareImageDecodeOutputPort(OMXImageDecode_s *component) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = component->outputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);
    portDefinition.format.image.eCompressionFormat = OMX_IMAGE_CodingAutoDetect;
    omxErr = OMX_SetParameter(component->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);
}




static void setupImageDecodeOutputPort(OMXImageDecode_s *component) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = component->outputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);

    OMX_PARAM_U32TYPE numAvailableStreams;
    OMX_INIT_STRUCTURE(numAvailableStreams);
    numAvailableStreams.nPortIndex = component->outputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamNumAvailableStreams, &numAvailableStreams);
    omxAssert(omxErr);
    assert(numAvailableStreams.nU32 > 0);

    printf("%d x %d\n", portDefinition.format.image.nFrameWidth, portDefinition.format.image.nFrameHeight);
    omxPrintPort(component->handle, component->outputPortIndex);

    omxEnablePort(component->handle, component->outputPortIndex, OMX_TRUE);

//    omxErr = OMX_AllocateBuffer(component->handle, &component->outputBuffer, component->outputPortIndex, NULL, portDefinition.nBufferSize);
//    omxAssert(omxErr);
}



static void setupResizeInputPort(OMXResize_s *component, OMXSize_t frameSize, OMXRect_t cropRect, OMX_COLOR_FORMATTYPE eColorFormat) {
    assert(omxAssertImagePortFormatSupported(component->handle, component->inputPortIndex, eColorFormat));

    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
//    OMX_CONFIG_PORTBOOLEANTYPE *brcmSupportsSlices = &component->inputBrcmSupportsSlices;
//    OMX_INIT_STRUCTURE2(brcmSupportsSlices);
//    brcmSupportsSlices->nPortIndex = component->inputPortIndex;
//    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamBrcmSupportsSlices, brcmSupportsSlices);
//    omxAssert(omxErr);
//
//    OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = &component->inputPortDefinition;
//    OMX_INIT_STRUCTURE2(portDefinition);
//    portDefinition->nPortIndex = component->inputPortIndex;
//    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
//    omxAssert(omxErr);
//
//    portDefinition->format.image.nFrameWidth = frameSize.nWidth;
//    portDefinition->format.image.nFrameHeight = frameSize.nHeight;
//    portDefinition->format.image.nSliceHeight = (brcmSupportsSlices->bEnabled == OMX_TRUE) ? 16 : 0;
//    portDefinition->format.image.nStride = 0;
//    portDefinition->format.image.bFlagErrorConcealment = OMX_FALSE;
//    portDefinition->format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
//    portDefinition->format.image.eColorFormat = eColorFormat;
//    omxErr = OMX_SetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
//    omxAssert(omxErr);
//
//    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, portDefinition);
//    omxAssert(omxErr);

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

//
//    omxErr = OMX_AllocateBuffer(component->handle, &component->inputBuffer, component->inputPortIndex, NULL, portDefinition->nBufferSize);
//    omxAssert(omxErr);
}



static void setupResizeOutputPort(OMXResize_s *component, OMXSize_t frameSize, OMX_COLOR_FORMATTYPE eColorFormat) {
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

    //omxPrintPort(component->handle, component->outputPortIndex);

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

    //omxPrintPort(component->handle, component->outputPortIndex);


    omxEnablePort(component->handle, component->outputPortIndex, OMX_TRUE);


    omxErr = OMX_AllocateBuffer(component->handle, &component->outputBuffer, component->outputPortIndex, NULL, portDefinition->nBufferSize);
    omxAssert(omxErr);
}



static void freeImageDecodeBuffers(OMXImageDecode_s *component) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = component->inputPortIndex;
    omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);

    for (int i = 0; i < portDefinition.nBufferCountActual; i++) {
        omxErr = OMX_FreeBuffer(component->handle, component->inputPortIndex, component->inputBuffer[i]);
        omxAssert(omxErr);
    }

//    omxErr = OMX_FreeBuffer(component->handle, component->outputPortIndex, component->outputBuffer);
//    omxAssert(omxErr);
}



static void freeResizeBuffers(OMXResize_s *component) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
//    omxErr = OMX_FreeBuffer(component->handle, component->inputPortIndex, component->inputBuffer);
//    omxAssert(omxErr);
    omxErr = OMX_FreeBuffer(component->handle, component->outputPortIndex, component->outputBuffer);
    omxAssert(omxErr);
}



void omxTunnel() {
    MapFile_s map;
    initMapFile(&map, "36903_9_1.jpg", MAP_RO);
    assert(map.len > 0);
    uint8_t *jpegData = map.data;
    printf("jpegDataSize: %zu\n", map.len);

    uint32_t rawImageWidth = 0;
    uint32_t rawImageHeight = 0;
    uint8_t rawImageChannels = 0;
    uint8_t *rawImage = NULL;


    OMXSize_t inputFrameSize = { .nWidth = rawImageWidth, .nHeight = rawImageHeight };
    //OMXRect_t inputFrameCrop = { .nWidth = 256, .nHeight = 256, .nLeft = 128, .nTop = 128 };
    OMXRect_t inputFrameCrop = { .nWidth = 0, .nHeight = 0, .nLeft = 0, .nTop = 0 };
    OMXSize_t outputFrameSize = { .nWidth = 256, .nHeight = 256 };


    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_STATETYPE omxState = OMX_StateInvalid;
    VCOS_STATUS_T vcosErr = VCOS_SUCCESS;

    bcm_host_init();
    omxErr = OMX_Init();
    omxAssert(omxErr);

    //omxListComponents();

    OMXContext_s ctx;
    memset(&ctx, 0, sizeof(ctx));
    //ctx.inputPortDefinition = malloc(sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

    vcosErr = vcos_semaphore_create(&ctx.handler_lock, "handler_lock", 1);
    assert(vcosErr == VCOS_SUCCESS);
    vcosErr = vcos_semaphore_create(&ctx.portChangeLock, "portChangeLock", 1);
    assert(vcosErr == VCOS_SUCCESS);

    OMX_CALLBACKTYPE omxCallbacks;
    omxCallbacks.EventHandler = omxEventHandler;
    omxCallbacks.EmptyBufferDone = omxEmptyBufferDone;
    omxCallbacks.FillBufferDone = omxFillBufferDone;

    {
        OMX_STRING omxComponentName = "OMX.broadcom.image_decode";
        omxErr = OMX_GetHandle(&ctx.imageDecode.handle, omxComponentName, &ctx, &omxCallbacks);
        omxAssert(omxErr);
        omxAssertState(ctx.imageDecode.handle, OMX_StateLoaded);
        getImageDecodePorts(&ctx.imageDecode);
        omxEnablePort(ctx.imageDecode.handle, ctx.imageDecode.inputPortIndex, OMX_FALSE);
        omxEnablePort(ctx.imageDecode.handle, ctx.imageDecode.outputPortIndex, OMX_FALSE);
    }

    {
        OMX_STRING omxComponentName = "OMX.broadcom.resize";
        omxErr = OMX_GetHandle(&ctx.resize.handle, omxComponentName, &ctx, &omxCallbacks);
        omxAssert(omxErr);
        omxAssertState(ctx.resize.handle, OMX_StateLoaded);
        getResizePorts(&ctx.resize);
        omxEnablePort(ctx.resize.handle, ctx.resize.inputPortIndex, OMX_FALSE);
        omxEnablePort(ctx.resize.handle, ctx.resize.outputPortIndex, OMX_FALSE);
    }

    omxErr = OMX_SetupTunnel(
                             ctx.imageDecode.handle,
                             ctx.imageDecode.outputPortIndex,
                             ctx.resize.handle,
                             ctx.resize.inputPortIndex
                             );
    omxAssert(omxErr);

    {
        omxSwitchToState(ctx.imageDecode.handle, OMX_StateIdle);
        setupImageDecodeInputPort(&ctx.imageDecode, OMX_IMAGE_CodingJPEG);
        prepareImageDecodeOutputPort(&ctx.imageDecode);
        omxSwitchToState(ctx.imageDecode.handle, OMX_StateExecuting);
    }

    {
        omxSwitchToState(ctx.resize.handle, OMX_StateIdle);
        //setupResizeInputPort(&ctx.resize, inputFrameSize, inputFrameCrop, OMX_COLOR_Format32bitABGR8888);
        setupResizeOutputPort(&ctx.resize, outputFrameSize, OMX_COLOR_Format32bitABGR8888);
        omxSwitchToState(ctx.resize.handle, OMX_StateExecuting);
    }









    uint8_t *jpegDataPtr = jpegData;
    size_t jpegDataRemaining = map.len;
    int bufferIndex = 0;
    ctx.imageDecode.inputReady = true;
    ctx.resize.outputReady = false;

    FILE * output = fopen("out.data", "wb");

    bool once = true;

    while (true) {
        if (ctx.resize.outputReady) {
            ctx.resize.outputReady = false;

            fwrite(ctx.resize.outputBuffer->pBuffer + ctx.resize.outputBuffer->nOffset, sizeof(uint8_t), ctx.resize.outputBuffer->nFilledLen, output);

            printf("nFilledLen: %d\n", ctx.resize.outputBuffer->nFilledLen);
            printf("nFlags: 0x%08x\n", ctx.resize.outputBuffer->nFlags);

            if (ctx.resize.outputBuffer->nFlags & OMX_BUFFERFLAG_EOS) {
                puts("received OMX_BUFFERFLAG_EOS");
                break;
            }

            puts("OMX_FillThisBuffer");
            omxErr = OMX_FillThisBuffer(ctx.resize.handle, ctx.resize.outputBuffer);
            omxAssert(omxErr);
        }

        if (ctx.imageDecode.inputReady) {
            ctx.imageDecode.inputReady = false;

            if (jpegDataRemaining == 0) {
                continue;
            }

            OMX_BUFFERHEADERTYPE *inBuffer = ctx.imageDecode.inputBuffer[bufferIndex];
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
            omxErr = OMX_EmptyThisBuffer(ctx.imageDecode.handle, inBuffer);
            omxAssert(omxErr);

            if (once) {
                once = false;
                puts("wait for port change...");
                vcos_semaphore_wait(&ctx.portChangeLock);
                puts("continuing...");

                setupResizeInputPort(&ctx.resize, inputFrameSize, inputFrameCrop, OMX_COLOR_Format32bitABGR8888);
                setupImageDecodeOutputPort(&ctx.imageDecode);
                puts("########################");

                puts("OMX_FillThisBuffer");
                omxErr = OMX_FillThisBuffer(ctx.resize.handle, ctx.resize.outputBuffer);
                omxAssert(omxErr);
            }
        }
    }

    fclose(output);
    freeMapFile(&map);











    omxSwitchToState(ctx.imageDecode.handle, OMX_StateIdle);
    omxEnablePort(ctx.imageDecode.handle, ctx.imageDecode.inputPortIndex, OMX_FALSE);
    //omxEnablePort(ctx.imageDecode.handle, ctx.imageDecode.outputPortIndex, OMX_FALSE);
    freeImageDecodeBuffers(&ctx.imageDecode);
    omxSwitchToState(ctx.imageDecode.handle, OMX_StateLoaded);
    omxErr = OMX_FreeHandle(ctx.imageDecode.handle);
    omxAssert(omxErr);

    omxSwitchToState(ctx.resize.handle, OMX_StateIdle);
    //omxEnablePort(ctx.resize.handle, ctx.resize.inputPortIndex, OMX_FALSE);
    omxEnablePort(ctx.resize.handle, ctx.resize.outputPortIndex, OMX_FALSE);
    freeResizeBuffers(&ctx.resize);
    omxSwitchToState(ctx.resize.handle, OMX_StateLoaded);
    omxErr = OMX_FreeHandle(ctx.resize.handle);
    omxAssert(omxErr);

    // insert code here...
    printf("Hello, World!\n");
    return 0;
}
