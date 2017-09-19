//
//  omxHelper.h
//  OpenMAX
//
//  Created by Michael Kwasnicki on 29.08.17.
//  Copyright © 2017 Michael Kwasnicki. All rights reserved.
//

#ifndef omxHelper_h
#define omxHelper_h


#include <assert.h>
#include <string.h>

#define OMX_SKIP64BIT
#include <IL/OMX_Component.h>
#include <IL/OMX_Image.h>



#define OMX_INIT_STRUCTURE(a) \
memset(&(a), 0, sizeof(a)); \
(a).nSize = sizeof(a); \
(a).nVersion.nVersion = OMX_VERSION; \
(a).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
(a).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
(a).nVersion.s.nRevision = OMX_VERSION_REVISION; \
(a).nVersion.s.nStep = OMX_VERSION_STEP

#define OMX_INIT_STRUCTURE_P(a, size) \
memset((a), 0, size); \
(a)->nSize = size; \
(a)->nVersion.nVersion = OMX_VERSION; \
(a)->nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
(a)->nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
(a)->nVersion.s.nRevision = OMX_VERSION_REVISION; \
(a)->nVersion.s.nStep = OMX_VERSION_STEP


#define omxAssert(x) if (x != OMX_ErrorNone) { \
    printf("OMX_Error: %s\n", omxErrorTypeEnum(x)); \
    assert(x == OMX_ErrorNone); \
}


extern const char *omxBoolEnum[];
extern const char *omxDirTypeEnum[];
extern const char *omxPortDomainTypeEnum[];


const char *omxImageCodingTypeEnum(OMX_IMAGE_CODINGTYPE eCompressionFormat);
const char *omxColorFormatTypeEnum(OMX_COLOR_FORMATTYPE eColorFormat);
const char *omxEventTypeEnum(OMX_EVENTTYPE eEvent);
const char *omxStateTypeEnum(OMX_STATETYPE eState);
const char *omxCommandTypeEnum(OMX_COMMANDTYPE eCommand);
const char *omxErrorTypeEnum(OMX_ERRORTYPE eError);

void omxDumpParamPortDefinition(OMX_PARAM_PORTDEFINITIONTYPE portDefinition);
void omxDumpImagePortDefinition(OMX_IMAGE_PORTDEFINITIONTYPE image);

void omxAssertState(OMX_HANDLETYPE handle, OMX_STATETYPE state);
void omxEnablePort(OMX_HANDLETYPE omxHandle, OMX_U32 portIndex, OMX_BOOL enabled);
void omxSwitchToState(OMX_HANDLETYPE omxHandle, OMX_STATETYPE state);


#endif /* omxHelper_h */
