//
//  omxHelper.c
//  OpenMAX
//
//  Created by Michael Kwasnicki on 29.08.17.
//  Copyright © 2017 Michael Kwasnicki. All rights reserved.
//


#include "omxHelper.h"

#include <assert.h>
#include <stdio.h>

#include "cHelper.h"



const char *omxBoolEnum[] = {"OMX_FALSE", "OMX_TRUE"};
const char *omxDirTypeEnum[] = {"OMX_DirInput", "OMX_DirOutput"};
const char *omxPortDomainTypeEnum[] = {
    "OMX_PortDomainAudio",
    "OMX_PortDomainVideo",
    "OMX_PortDomainImage",
    "OMX_PortDomainOther"
};



const char *omxImageCodingTypeEnum(OMX_IMAGE_CODINGTYPE eCompressionFormat) {
    static char unknown[32];

    switch (eCompressionFormat) {
        case OMX_IMAGE_CodingUnused:     return "OMX_IMAGE_CodingUnused";
        case OMX_IMAGE_CodingAutoDetect: return "OMX_IMAGE_CodingAutoDetect";
        case OMX_IMAGE_CodingJPEG:       return "OMX_IMAGE_CodingJPEG";
        case OMX_IMAGE_CodingJPEG2K:     return "OMX_IMAGE_CodingJPEG2K";
        case OMX_IMAGE_CodingEXIF:       return "OMX_IMAGE_CodingEXIF";
        case OMX_IMAGE_CodingTIFF:       return "OMX_IMAGE_CodingTIFF";
        case OMX_IMAGE_CodingGIF:        return "OMX_IMAGE_CodingGIF";
        case OMX_IMAGE_CodingPNG:        return "OMX_IMAGE_CodingPNG";
        case OMX_IMAGE_CodingLZW:        return "OMX_IMAGE_CodingLZW";
        case OMX_IMAGE_CodingBMP:        return "OMX_IMAGE_CodingBMP";

        case OMX_IMAGE_CodingTGA:        return "OMX_IMAGE_CodingTGA";
        case OMX_IMAGE_CodingPPM:        return "OMX_IMAGE_CodingPPM";

        default: {
            snprintf(unknown, sizeof(unknown), "OMX_IMAGE_CODINGTYPE 0x%08x", eCompressionFormat);
            return unknown;
        }
    }
}



const char *omxColorFormatTypeEnum(OMX_COLOR_FORMATTYPE eColorFormat) {
    static char unknown[32];

    switch (eColorFormat) {
        case OMX_COLOR_FormatUnused:                 return "OMX_COLOR_FormatUnused";
        case OMX_COLOR_FormatMonochrome:             return "OMX_COLOR_FormatMonochrome";
        case OMX_COLOR_Format8bitRGB332:             return "OMX_COLOR_Format8bitRGB332";
        case OMX_COLOR_Format12bitRGB444:            return "OMX_COLOR_Format12bitRGB444";
        case OMX_COLOR_Format16bitARGB4444:          return "OMX_COLOR_Format16bitARGB4444";
        case OMX_COLOR_Format16bitARGB1555:          return "OMX_COLOR_Format16bitARGB1555";
        case OMX_COLOR_Format16bitRGB565:            return "OMX_COLOR_Format16bitRGB565";
        case OMX_COLOR_Format16bitBGR565:            return "OMX_COLOR_Format16bitBGR565";
        case OMX_COLOR_Format18bitRGB666:            return "OMX_COLOR_Format18bitRGB666";
        case OMX_COLOR_Format18bitARGB1665:          return "OMX_COLOR_Format18bitARGB1665";
        case OMX_COLOR_Format19bitARGB1666:          return "OMX_COLOR_Format19bitARGB1666 ";
        case OMX_COLOR_Format24bitRGB888:            return "OMX_COLOR_Format24bitRGB888";
        case OMX_COLOR_Format24bitBGR888:            return "OMX_COLOR_Format24bitBGR888";
        case OMX_COLOR_Format24bitARGB1887:          return "OMX_COLOR_Format24bitARGB1887";
        case OMX_COLOR_Format25bitARGB1888:          return "OMX_COLOR_Format25bitARGB1888";
        case OMX_COLOR_Format32bitBGRA8888:          return "OMX_COLOR_Format32bitBGRA8888";
        case OMX_COLOR_Format32bitARGB8888:          return "OMX_COLOR_Format32bitARGB8888";
        case OMX_COLOR_FormatYUV411Planar:           return "OMX_COLOR_FormatYUV411Planar";
        case OMX_COLOR_FormatYUV411PackedPlanar:     return "OMX_COLOR_FormatYUV411PackedPlanar";
        case OMX_COLOR_FormatYUV420Planar:           return "OMX_COLOR_FormatYUV420Planar";
        case OMX_COLOR_FormatYUV420PackedPlanar:     return "OMX_COLOR_FormatYUV420PackedPlanar";
        case OMX_COLOR_FormatYUV420SemiPlanar:       return "OMX_COLOR_FormatYUV420SemiPlanar";
        case OMX_COLOR_FormatYUV422Planar:           return "OMX_COLOR_FormatYUV422Planar";
        case OMX_COLOR_FormatYUV422PackedPlanar:     return "OMX_COLOR_FormatYUV422PackedPlanar";
        case OMX_COLOR_FormatYUV422SemiPlanar:       return "OMX_COLOR_FormatYUV422SemiPlanar";
        case OMX_COLOR_FormatYCbYCr:                 return "OMX_COLOR_FormatYCbYCr";
        case OMX_COLOR_FormatYCrYCb:                 return "OMX_COLOR_FormatYCrYCb";
        case OMX_COLOR_FormatCbYCrY:                 return "OMX_COLOR_FormatCbYCrY";
        case OMX_COLOR_FormatCrYCbY:                 return "OMX_COLOR_FormatCrYCbY";
        case OMX_COLOR_FormatYUV444Interleaved:      return "OMX_COLOR_FormatYUV444Interleaved";
        case OMX_COLOR_FormatRawBayer8bit:           return "OMX_COLOR_FormatRawBayer8bit";
        case OMX_COLOR_FormatRawBayer10bit:          return "OMX_COLOR_FormatRawBayer10bit";
        case OMX_COLOR_FormatRawBayer8bitcompressed: return "OMX_COLOR_FormatRawBayer8bitcompressed";
        case OMX_COLOR_FormatL2:                     return "OMX_COLOR_FormatL2 ";
        case OMX_COLOR_FormatL4:                     return "OMX_COLOR_FormatL4 ";
        case OMX_COLOR_FormatL8:                     return "OMX_COLOR_FormatL8 ";
        case OMX_COLOR_FormatL16:                    return "OMX_COLOR_FormatL16 ";
        case OMX_COLOR_FormatL24:                    return "OMX_COLOR_FormatL24 ";
        case OMX_COLOR_FormatL32:                    return "OMX_COLOR_FormatL32";
        case OMX_COLOR_FormatYUV420PackedSemiPlanar: return "OMX_COLOR_FormatYUV420PackedSemiPlanar";
        case OMX_COLOR_FormatYUV422PackedSemiPlanar: return "OMX_COLOR_FormatYUV422PackedSemiPlanar";
        case OMX_COLOR_Format18BitBGR666:            return "OMX_COLOR_Format18BitBGR666";
        case OMX_COLOR_Format24BitARGB6666:          return "OMX_COLOR_Format24BitARGB6666";
        case OMX_COLOR_Format24BitABGR6666:          return "OMX_COLOR_Format24BitABGR6666";

        case OMX_COLOR_Format32bitABGR8888:          return "OMX_COLOR_Format32bitABGR8888";
        case OMX_COLOR_Format8bitPalette:            return "OMX_COLOR_Format8bitPalette";
        case OMX_COLOR_FormatYUVUV128:               return "OMX_COLOR_FormatYUVUV128";
        case OMX_COLOR_FormatRawBayer12bit:          return "OMX_COLOR_FormatRawBayer12bit";
        case OMX_COLOR_FormatBRCMEGL:                return "OMX_COLOR_FormatBRCMEGL";
        case OMX_COLOR_FormatBRCMOpaque:             return "OMX_COLOR_FormatBRCMOpaque";
        case OMX_COLOR_FormatYVU420PackedPlanar:     return "OMX_COLOR_FormatYVU420PackedPlanar";
        case OMX_COLOR_FormatYVU420PackedSemiPlanar: return "OMX_COLOR_FormatYVU420PackedSemiPlanar";
        case OMX_COLOR_FormatRawBayer16bit:          return "OMX_COLOR_FormatRawBayer16bit";
        case OMX_COLOR_FormatYUV420_16PackedPlanar:  return "OMX_COLOR_FormatYUV420_16PackedPlanar";
        case OMX_COLOR_FormatYUVUV64_16:             return "OMX_COLOR_FormatYUVUV64_16";
        default: {
            snprintf(unknown, sizeof(unknown), "OMX_COLOR_FORMATTYPE 0x%08x", eColorFormat);
            return unknown;
        }
    }
}



const char *omxEventTypeEnum(OMX_EVENTTYPE eEvent) {
    static char unknown[32];

    switch (eEvent) {
        case OMX_EventCmdComplete:               return "OMX_EventCmdComplete";
        case OMX_EventError:                     return "OMX_EventError";
        case OMX_EventMark:                      return "OMX_EventMark";
        case OMX_EventPortSettingsChanged:       return "OMX_EventPortSettingsChanged";
        case OMX_EventBufferFlag:                return "OMX_EventBufferFlag";
        case OMX_EventResourcesAcquired:         return "OMX_EventResourcesAcquired";
        case OMX_EventComponentResumed:          return "OMX_EventComponentResumed";
        case OMX_EventDynamicResourcesAvailable: return "OMX_EventDynamicResourcesAvailable";
        case OMX_EventPortFormatDetected:        return "OMX_EventPortFormatDetected";

        case OMX_EventParamOrConfigChanged:      return "OMX_EventParamOrConfigChanged";

        default: {
            snprintf(unknown, sizeof(unknown), "OMX_EVENTTYPE 0x%08x", eEvent);
            return unknown;
        }
    }
}



const char *omxStateTypeEnum(OMX_STATETYPE eState) {
    static char unknown[32];

    switch (eState) {
        case OMX_StateInvalid:          return "OMX_StateInvalid";
        case OMX_StateLoaded:           return "OMX_StateLoaded";
        case OMX_StateIdle:             return "OMX_StateIdle";
        case OMX_StateExecuting:        return "OMX_StateExecuting";
        case OMX_StatePause:            return "OMX_StatePause";
        case OMX_StateWaitForResources: return "OMX_StateWaitForResources";

        default: {
            snprintf(unknown, sizeof(unknown), "OMX_STATETYPE 0x%08x", eState);
            return unknown;
        }
    }
}



const char *omxCommandTypeEnum(OMX_COMMANDTYPE eCommand) {
    static char unknown[32];

    switch (eCommand) {
        case OMX_CommandStateSet:    return "OMX_CommandStateSet";
        case OMX_CommandFlush:       return "OMX_CommandFlush";
        case OMX_CommandPortDisable: return "OMX_CommandPortDisable";
        case OMX_CommandPortEnable:  return "OMX_CommandPortEnable";
        case OMX_CommandMarkBuffer:  return "OMX_CommandMarkBuffer";

        default: {
            snprintf(unknown, sizeof(unknown), "OMX_COMMANDTYPE 0x%08x", eCommand);
            return unknown;
        }
    }
}



void omxDumpParamPortDefinition(OMX_PARAM_PORTDEFINITIONTYPE portDefinition) {
    printf(LEVEL_2 "nPortIndex:         %u\n", portDefinition.nPortIndex);
    printf(LEVEL_2 "eDir:               %s\n", omxDirTypeEnum[portDefinition.eDir]);
    printf(LEVEL_2 "nBufferCountActual: %u\n", portDefinition.nBufferCountActual);
    printf(LEVEL_2 "nBufferCountMin:    %u\n", portDefinition.nBufferCountMin);
    printf(LEVEL_2 "nBufferSize:        %u\n", portDefinition.nBufferSize);
    printf(LEVEL_2 "bEnabled:           %s\n", omxBoolEnum[portDefinition.bEnabled]);
    printf(LEVEL_2 "bPopulated:         %s\n", omxBoolEnum[portDefinition.bPopulated]);
    printf(LEVEL_2 "bBuffersContiguous: %s\n", omxBoolEnum[portDefinition.bBuffersContiguous]);
    printf(LEVEL_2 "nBufferAlignment:   %u\n", portDefinition.nBufferAlignment);
    printf(LEVEL_2 "eDomain:            %s\n", omxPortDomainTypeEnum[portDefinition.eDomain]);
}



void omxDumpImagePortDefinition(OMX_IMAGE_PORTDEFINITIONTYPE image) {
    printf(LEVEL_3 "cMIMEType:             %s\n", image.cMIMEType);
    printf(LEVEL_3 "nFrameWidth:           %u\n", image.nFrameWidth);
    printf(LEVEL_3 "nFrameHeight:          %u\n", image.nFrameHeight);
    printf(LEVEL_3 "nStride:               %d\n", image.nStride);
    printf(LEVEL_3 "nSliceHeight:          %u\n", image.nSliceHeight);
    printf(LEVEL_3 "bFlagErrorConcealment: %s\n", omxBoolEnum[image.bFlagErrorConcealment]);
    printf(LEVEL_3 "eCompressionFormat:    %s\n", omxImageCodingTypeEnum(image.eCompressionFormat));
    printf(LEVEL_3 "eColorFormat:          %s\n", omxColorFormatTypeEnum(image.eColorFormat));
}



void omxAssertState(OMX_HANDLETYPE handle, OMX_STATETYPE state) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_STATETYPE omxState = OMX_StateInvalid;
    omxErr = OMX_GetState(handle, &omxState);
    assert(omxErr == OMX_ErrorNone);
    assert(omxState == state);
}



void omxEnablePort(OMX_HANDLETYPE omxHandle, OMX_U32 portIndex, OMX_BOOL enabled) {
    static const OMX_COMMANDTYPE command[2] = {OMX_CommandPortDisable, OMX_CommandPortEnable};
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDefinition;
    OMX_INIT_STRUCTURE(portDefinition);
    portDefinition.nPortIndex = portIndex;

    omxErr = OMX_SendCommand(omxHandle, command[enabled], portIndex, NULL);
    assert(omxErr == OMX_ErrorNone);

    do {
        omxErr = OMX_GetParameter(omxHandle, OMX_IndexParamPortDefinition, &portDefinition);
        assert(omxErr == OMX_ErrorNone);
    } while (portDefinition.bEnabled != enabled);
}



void omxSwitchToState(OMX_HANDLETYPE omxHandle, OMX_STATETYPE state) {
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_STATETYPE omxState = OMX_StateInvalid;
    omxErr = OMX_GetState(omxHandle, &omxState);
    assert(omxErr == OMX_ErrorNone);

    if (omxState != state) {
        omxErr = OMX_SendCommand(omxHandle, OMX_CommandStateSet, state, NULL);
        printf("%x -> %x : %x\n", omxState, state, omxErr);
        assert(omxErr == OMX_ErrorNone);

        int c = 10;

        do {
            c--;
            omxErr = OMX_GetState(omxHandle, &omxState);
            printf("test : %x -> %x : %x\n", omxState, state, omxErr);
            assert(omxErr == OMX_ErrorNone);
            //sleep(1);
        } while (omxState != state && c);
    }
}

