//
//  omxDump.h
//  OpenMAX
//
//  Created by Michael Kwasnicki on 29.08.17.
//  Copyright © 2017 Michael Kwasnicki. All rights reserved.
//

#ifndef omxDump_h
#define omxDump_h


#define OMX_SKIP64BIT
#include <IL/OMX_Component.h>
#include <IL/OMX_Image.h>


void omxPrintPort(OMX_HANDLETYPE omxHandle, OMX_U32 portIndex);

void omxDump(OMX_U32 componentIndex);


#endif /* omxDump_h */
