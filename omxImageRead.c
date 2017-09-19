//
//  omxImageRead.c
//  OpenMAX
//
//  Created by Michael Kwasnicki on 17.09.17.
//  Copyright © 2017 Michael Kwasnicki. All rights reserved.
//

#include "omxJPEGDec.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include <sys/param.h>

#define OMX_SKIP64BIT
#include <IL/OMX_Component.h>
#include <IL/OMX_Core.h>
#include <interface/vcos/vcos.h>

#include "cHelper.h"
#include "omxHelper.h"
#include "omxDump.h"



typedef struct {
    OMX_HANDLETYPE handle;
    OMX_BUFFERHEADERTYPE *outputBuffer[3];
    OMX_U32 outputPortIndex;
    bool outputReady;

    VCOS_SEMAPHORE_T handler_lock;
    VCOS_SEMAPHORE_T portChangeLock;
    int flushed;
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

            vcos_semaphore_wait(&ctx->handler_lock);

            if ( nData1 == OMX_CommandFlush ) {
                ctx->flushed = true;
            }

            vcos_semaphore_post(&ctx->handler_lock);
            break;

        case OMX_EventPortSettingsChanged:
            printf("Port: %d  nData2: %x\n", nData1, nData2);
            vcos_semaphore_post(&ctx->portChangeLock);
            break;

        case OMX_EventError:
            printf("nData1: %x,  nData2: %x\n", nData1, nData2);

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
    puts("omxEmptyBufferDone");
    ComponentContext *ctx = (ComponentContext*)pAppData;
    vcos_semaphore_wait(&ctx->handler_lock);
    vcos_semaphore_post(&ctx->handler_lock);
    return OMX_ErrorNone;
}



static OMX_ERRORTYPE omxFillBufferDone(
                                       OMX_OUT OMX_HANDLETYPE hComponent,
                                       OMX_OUT OMX_PTR pAppData,
                                       OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {
    puts("omxFillBufferDone");
    ComponentContext *ctx = (ComponentContext*)pAppData;
    vcos_semaphore_wait(&ctx->handler_lock);
    ctx->outputReady = true;
    vcos_semaphore_post(&ctx->handler_lock);
    return OMX_ErrorNone;
}



static void setupInputPort(ComponentContext *ctx) {
    char uri[] = "36903_9_1.jpg";
    size_t size = sizeof(OMX_PARAM_CONTENTURITYPE) + strlen(uri);
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_CONTENTURITYPE *contentURI = malloc(size);
    OMX_INIT_STRUCTURE_P(contentURI, size);
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamContentURI, contentURI);
    assert(omxErr == OMX_ErrorNone);

    strncpy(contentURI->contentURI, uri, strlen(uri));
    omxErr = OMX_SetParameter(ctx->handle, OMX_IndexParamContentURI, contentURI);
    assert(omxErr == OMX_ErrorNone);
    free(contentURI);
}



static void setupOutputPort(ComponentContext *ctx) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = ctx->outputPortIndex;
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    assert(omxErr == OMX_ErrorNone);
    printf("%d x %d\n", portDefinition.format.image.nFrameWidth, portDefinition.format.image.nFrameHeight);


    omxPrintPort(ctx->handle, ctx->outputPortIndex);


//    portDefinition.format.image.eColorFormat = OMX_COLOR_Format32bitABGR8888;
//    portDefinition.format.image.nSliceHeight = 256;
//    omxErr = OMX_SetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
//    assert(omxErr == OMX_ErrorNone);
//    OMX_INIT_STRUCTURE(portDefinition);
//    portDefinition.nPortIndex = ctx->outputPortIndex;
//    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
//    assert(omxErr == OMX_ErrorNone);


    omxEnablePort(ctx->handle, ctx->outputPortIndex, OMX_TRUE);
    omxPrintPort(ctx->handle, ctx->outputPortIndex);


    for (int i = 0; i < portDefinition.nBufferCountActual; i++) {
        omxErr = OMX_AllocateBuffer(ctx->handle, &ctx->outputBuffer[i], ctx->outputPortIndex, i, portDefinition.nBufferSize);
        assert(omxErr == OMX_ErrorNone);
    }


    //    portDefinition.format.image.nFrameWidth = rawImageWidth;
    //    portDefinition.format.image.nFrameHeight = rawImageHeight;
    //    portDefinition.format.image.bFlagErrorConcealment = OMX_FALSE;
    //    portDefinition.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
    //    portDefinition.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
    //
    //    omxErr = OMX_SetParameter(ctx.handle, OMX_IndexParamPortDefinition, &portDefinition);
    //    assert(omxErr == OMX_ErrorNone);
    //
    //    OMX_INIT_STRUCTURE(portDefinition);
    //    portDefinition.nPortIndex = ctx.outputPortIndex;
    //    omxErr = OMX_GetParameter(ctx.handle, OMX_IndexParamPortDefinition, &portDefinition);
    //    assert(omxErr == OMX_ErrorNone);
    //
    //
    //
    //    OMX_IMAGE_PARAM_QFACTORTYPE qFactor;
    //    OMX_INIT_STRUCTURE(qFactor);
    //    qFactor.nPortIndex = ctx.outputPortIndex;
    //    qFactor.nQFactor = outputQuality;
    //
    //    omxErr = OMX_SetParameter(ctx.handle, OMX_IndexParamQFactor, &qFactor);
    //    assert(omxErr == OMX_ErrorNone);
    //
    //
    //    omxEnablePort(ctx.handle, ctx.outputPortIndex, OMX_TRUE);
    //
    //
    //    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    //    OMX_INIT_STRUCTURE(portDefinition);
    //    portDefinition.nPortIndex = ctx.outputPortIndex;
    //    omxErr = OMX_GetParameter(ctx.handle, OMX_IndexParamPortDefinition, &portDefinition);
    //    assert(omxErr == OMX_ErrorNone);
    //    assert(portDefinition.eDir == OMX_DirOutput);
    //
    //    omxErr = OMX_AllocateBuffer(ctx.handle, &ctx.outputBuffer, ctx.outputPortIndex, NULL, portDefinition.nBufferSize);
    //    assert(omxErr == OMX_ErrorNone);
}



static void omxGetPorts(ComponentContext *ctx) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PORT_PARAM_TYPE ports;
    OMX_INIT_STRUCTURE(ports);
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamImageInit, &ports);
    assert(omxErr == OMX_ErrorNone);
    const OMX_U32 pEnd = ports.nStartPortNumber + ports.nPorts;

    for (OMX_U32 p = ports.nStartPortNumber; p < pEnd; p++) {
        OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
        OMX_INIT_STRUCTURE(portDefinition);
        portDefinition.nPortIndex = p;
        omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
        assert(omxErr == OMX_ErrorNone);

        //if (portDefinition.eDir == OMX_DirInput) {
        //    assert(ctx->inputPortIndex == 0);
        //    ctx->inputPortIndex = p;
        //}

        if (portDefinition.eDir == OMX_DirOutput) {
            assert(ctx->outputPortIndex == 0);
            ctx->outputPortIndex = p;
        }
    }
}




void omxImageRead() {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_STATETYPE omxState = OMX_StateInvalid;
    VCOS_STATUS_T vcosErr = VCOS_SUCCESS;

    bcm_host_init();
    omxErr = OMX_Init();
    assert(omxErr == OMX_ErrorNone);

    ComponentContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    vcosErr = vcos_semaphore_create(&ctx.handler_lock, "handler_lock", 1);
    assert(vcosErr == VCOS_SUCCESS);
    vcosErr = vcos_semaphore_create(&ctx.portChangeLock, "portChangeLock", 1);
    assert(vcosErr == VCOS_SUCCESS);

    OMX_STRING omxComponentName = "OMX.broadcom.image_read";
    OMX_CALLBACKTYPE omxCallbacks;
    omxCallbacks.EventHandler = omxEventHandler;
    omxCallbacks.EmptyBufferDone = omxEmptyBufferDone;
    omxCallbacks.FillBufferDone = omxFillBufferDone;

    omxErr = OMX_GetHandle(&ctx.handle, omxComponentName, &ctx, &omxCallbacks);
    assert(omxErr == OMX_ErrorNone);
    omxAssertState(ctx.handle, OMX_StateLoaded);
    puts("1"); sleep(1);

    omxGetPorts(&ctx);
    puts("2"); sleep(1);
    //omxEnablePort(ctx.handle, ctx.inputPortIndex, OMX_FALSE);
    omxEnablePort(ctx.handle, ctx.outputPortIndex, OMX_FALSE);
    setupInputPort(&ctx);
    setupOutputPort(&ctx);
    puts("3"); sleep(1);
    omxSwitchToState(ctx.handle, OMX_StateIdle);
//    puts("4"); sleep(1);
//    puts("5"); sleep(1);
//    //prepareOutputPort(&ctx);
//    omxSwitchToState(ctx.handle, OMX_StateExecuting);
//    puts("6"); sleep(1);
//
//    puts("wait for port change...");
//    vcos_semaphore_wait(&ctx.portChangeLock);
//    puts("continuing...");
//
//    setupOutputPort(&ctx);
//
//    puts("OMX_FillThisBuffer");
//    omxErr = OMX_FillThisBuffer(ctx.handle, ctx.outputBuffer);
//    assert(omxErr == OMX_ErrorNone);
//
//
//
//
    sleep(10);
    return;



    int bufferIndex = 0;
    ctx.outputReady = false;

    FILE * output = fopen("out.data", "wb");

    puts("OMX_FillThisBuffer");
    omxErr = OMX_FillThisBuffer(ctx.handle, ctx.outputBuffer[bufferIndex]);
    assert(omxErr == OMX_ErrorNone);

    while (true) {
        if (ctx.outputReady) {
            ctx.outputReady = false;
            puts("x");

//            fwrite(ctx.outputBuffer->pBuffer + ctx.outputBuffer->nOffset, sizeof(uint8_t), ctx.outputBuffer->nFilledLen, output);
//
//            printf("nFilledLen: %d\n", ctx.outputBuffer->nFilledLen);
//            printf("nFlags: 0x%08x\n", ctx.outputBuffer->nFlags);

//            if (ctx.outputBuffer->nFlags & OMX_BUFFERFLAG_EOS) {
//                puts("received OMX_BUFFERFLAG_EOS");
//                break;
//            }

            bufferIndex++;
            bufferIndex %= 3;
            puts("OMX_FillThisBuffer");
            omxErr = OMX_FillThisBuffer(ctx.handle, ctx.outputBuffer[bufferIndex]);
            assert(omxErr == OMX_ErrorNone);
        }
    }

    fclose(output);
    return;
}
