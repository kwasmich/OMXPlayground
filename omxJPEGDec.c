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
    OMX_BUFFERHEADERTYPE *inputBuffer[3];
    OMX_BUFFERHEADERTYPE *outputBuffer;
    VCOS_SEMAPHORE_T handler_lock;
    VCOS_SEMAPHORE_T portChangeLock;
    OMX_U32 inputPortIndex;
    OMX_U32 outputPortIndex;
    bool inputReady;
    bool outputReady;
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
    puts("omxEmptyBufferDone");
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
    puts("omxFillBufferDone");
    ComponentContext *ctx = (ComponentContext*)pAppData;
    vcos_semaphore_wait(&ctx->handler_lock);
    ctx->outputReady = true;
    vcos_semaphore_post(&ctx->handler_lock);
    return OMX_ErrorNone;
}



static unsigned char *rawFromFileContents(const char *in_FILE_NAME, const bool in_ZERO_TERMINATE, int *out_fileSize) {
    unsigned char *fileContents = NULL;
    FILE *fileStream = fopen(in_FILE_NAME, "rb");

    if (!fileStream) {
        perror(in_FILE_NAME);
        *out_fileSize = 0;
        return fileContents;
    }

    int err = fseek(fileStream, 0L, SEEK_END);

    if (err != 0) {
        perror(in_FILE_NAME);
        *out_fileSize = 0;
        err = fclose(fileStream);

        if (err != 0) {
            perror(in_FILE_NAME);
        }

        fileStream = NULL;
        return fileContents;
    }

    long length = ftell(fileStream);

    if (length == -1) {
        perror(in_FILE_NAME);
        *out_fileSize = 0;
        err = fclose(fileStream);

        if (err != 0) {
            perror(in_FILE_NAME);
        }

        fileStream = NULL;
        return fileContents;
    }

    rewind(fileStream);

    size_t mallocCount = (in_ZERO_TERMINATE) ? length + 1 : length;
    fileContents = malloc(sizeof(unsigned char) * mallocCount);

    if (!fileContents) {
        perror(in_FILE_NAME);
        *out_fileSize = 0;
        err = fclose(fileStream);

        if (err != 0) {
            perror(in_FILE_NAME);
        }

        fileStream = NULL;
        return fileContents;
    }

    long readLength = fread(fileContents, sizeof(unsigned char), length, fileStream);
    assert(readLength == length);

    err = fclose(fileStream);

    if (err != 0) {
        perror(in_FILE_NAME);
    }

    fileStream = NULL;

    if (in_ZERO_TERMINATE) {
        fileContents[length] = 0;
    }

    *out_fileSize = length;
    return fileContents;
}



static void setupInputPort(ComponentContext *ctx) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = ctx->inputPortIndex;
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);

    OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
    OMX_INIT_STRUCTURE(imagePortFormat);
    imagePortFormat.nPortIndex = ctx->inputPortIndex;
    imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingJPEG; // OMX_IMAGE_CodingJPEG;
    omxErr = OMX_SetParameter(ctx->handle, OMX_IndexParamImagePortFormat, &imagePortFormat);
    omxAssert(omxErr);

    omxEnablePort(ctx->handle, ctx->inputPortIndex, OMX_TRUE);

    for (int i = 0; i < portDefinition.nBufferCountActual; i++) {
        omxErr = OMX_AllocateBuffer(ctx->handle, &ctx->inputBuffer[i], ctx->inputPortIndex, i, portDefinition.nBufferSize);
        omxAssert(omxErr);
    }
}



static void setupOutputPort(ComponentContext *ctx) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = ctx->outputPortIndex;
    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
    omxAssert(omxErr);
    printf("%d x %d\n", portDefinition.format.image.nFrameWidth, portDefinition.format.image.nFrameHeight);


    omxPrintPort(ctx->handle, ctx->outputPortIndex);


//    portDefinition.format.image.eColorFormat = OMX_COLOR_Format32bitABGR8888;
//    portDefinition.format.image.nSliceHeight = 256;
//    omxErr = OMX_SetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
//    omxAssert(omxErr);
//    OMX_INIT_STRUCTURE(portDefinition);
//    portDefinition.nPortIndex = ctx->outputPortIndex;
//    omxErr = OMX_GetParameter(ctx->handle, OMX_IndexParamPortDefinition, &portDefinition);
//    omxAssert(omxErr);


    omxEnablePort(ctx->handle, ctx->outputPortIndex, OMX_TRUE);
    omxPrintPort(ctx->handle, ctx->outputPortIndex);


    omxErr = OMX_AllocateBuffer(ctx->handle, &ctx->outputBuffer, ctx->outputPortIndex, NULL, portDefinition.nBufferSize);
    omxAssert(omxErr);



    //    portDefinition.format.image.nFrameWidth = rawImageWidth;
    //    portDefinition.format.image.nFrameHeight = rawImageHeight;
    //    portDefinition.format.image.bFlagErrorConcealment = OMX_FALSE;
    //    portDefinition.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
    //    portDefinition.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
    //
    //    omxErr = OMX_SetParameter(ctx.handle, OMX_IndexParamPortDefinition, &portDefinition);
    //    omxAssert(omxErr);
    //
    //    OMX_INIT_STRUCTURE(portDefinition);
    //    portDefinition.nPortIndex = ctx.outputPortIndex;
    //    omxErr = OMX_GetParameter(ctx.handle, OMX_IndexParamPortDefinition, &portDefinition);
    //    omxAssert(omxErr);
    //
    //
    //
    //    OMX_IMAGE_PARAM_QFACTORTYPE qFactor;
    //    OMX_INIT_STRUCTURE(qFactor);
    //    qFactor.nPortIndex = ctx.outputPortIndex;
    //    qFactor.nQFactor = outputQuality;
    //
    //    omxErr = OMX_SetParameter(ctx.handle, OMX_IndexParamQFactor, &qFactor);
    //    omxAssert(omxErr);
    //
    //
    //    omxEnablePort(ctx.handle, ctx.outputPortIndex, OMX_TRUE);
    //
    //
    //    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    //    OMX_INIT_STRUCTURE(portDefinition);
    //    portDefinition.nPortIndex = ctx.outputPortIndex;
    //    omxErr = OMX_GetParameter(ctx.handle, OMX_IndexParamPortDefinition, &portDefinition);
    //    omxAssert(omxErr);
    //    assert(portDefinition.eDir == OMX_DirOutput);
    //
    //    omxErr = OMX_AllocateBuffer(ctx.handle, &ctx.outputBuffer, ctx.outputPortIndex, NULL, portDefinition.nBufferSize);
    //    omxAssert(omxErr);
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




void omxJPEGDec() {
    size_t jpegDataSize = 0;
    uint8_t *jpegData = (uint8_t *)rawFromFileContents("36903_9_1.jpg", 0, &jpegDataSize);
    assert(jpegData && jpegDataSize > 0);
    printf("jpegDataSize: %zu\n", jpegDataSize);

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

    ComponentContext ctx;
    memset(&ctx, 0, sizeof(ctx));

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
    setupInputPort(&ctx);
    //prepareOutputPort(&ctx);
    omxSwitchToState(ctx.handle, OMX_StateExecuting);


    uint8_t *jpegDataPtr = jpegData;
    size_t jpegDataRemaining = jpegDataSize;
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
    return;
    //
    //
    //    int pos = 0;
    //    ctx.inputReady = true;
    //    ctx.outputReady = false;
    //
    //
    //    omxErr = OMX_FillThisBuffer(ctx.handle, ctx.outputBuffer);
    //    omxAssert(omxErr);
    //
    //    FILE * output = fopen("out.jpg", "wb");
    //
    //    while (true) {
    //        if (ctx.outputReady) {
    //            fwrite(ctx.outputBuffer->pBuffer + ctx.outputBuffer->nOffset, sizeof(uint8_t), ctx.outputBuffer->nFilledLen, output);
    //
    //            if (ctx.outputBuffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) {
    //                break;
    //            }
    //
    //            ctx.outputReady = false;
    //            omxErr = OMX_FillThisBuffer(ctx.handle, ctx.outputBuffer);
    //            omxAssert(omxErr);
    //        }
    //
    //        if (ctx.inputReady) {
    //            if (pos == rawImageSize) {
    //                ctx.inputReady = false;
    //                continue;
    //            }
    //
    //            memcpy(ctx.inputBuffer->pBuffer, &rawImage[pos], sliceSize);
    //            ctx.inputBuffer->nOffset = 0;
    //            ctx.inputBuffer->nFilledLen = sliceSize;
    //            pos += sliceSize;
    //
    //            if (pos + sliceSize > rawImageSize) {
    //                sliceSize = rawImageSize - pos;
    //            }
    //
    //            ctx.inputReady = false;
    //            omxErr = OMX_EmptyThisBuffer(ctx.handle, ctx.inputBuffer);
    //            omxAssert(omxErr);
    //        }
    //    }
    //
    //    fclose(output);
    //    puts(COLOR_RED "done" COLOR_NC " blaub " COLOR_GREEN "yes" COLOR_NC);
    //
    //
    //    puts("1");
    //    omxSwitchToState(ctx.handle, OMX_StateIdle);
    //    puts("2");
    //    omxEnablePort(ctx.handle, ctx.inputPortIndex, OMX_FALSE);
    //    puts("3");
    //    omxEnablePort(ctx.handle, ctx.outputPortIndex, OMX_FALSE);
    //    puts("4");
    //
    //
    //    // Deallocate input buffer
    //    {
    //        omxErr = OMX_FreeBuffer(ctx.handle, ctx.inputPortIndex, ctx.inputBuffer);
    //        omxAssert(omxErr);
    //    }
    //
    //    puts("5");
    //
    //    // Deallocate output buffer
    //    {
    //        omxErr = OMX_FreeBuffer(ctx.handle, ctx.outputPortIndex, ctx.outputBuffer);
    //        omxAssert(omxErr);
    //    }
    //    
    //    puts("6");
    //    
    //    
    //    omxSwitchToState(ctx.handle, OMX_StateLoaded);
    //    
    //    omxErr = OMX_FreeHandle(ctx.handle);
    //    omxAssert(omxErr);
    //    
    //    
    //    //    for (uint32_t i = 0; i < 3000; i++) {
    //    //        usleep(8000);
    //    //    }
    //    
    //    // insert code here...
    //    printf("Hello, World!\n");
    //    return 0;
}
