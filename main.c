//
//  main.c
//  OpenMAX
//
//  Created by Michael Kwasnicki on 26.08.17.
//  Copyright © 2017 Michael Kwasnicki. All rights reserved.
//

// inspired by https://github.com/hopkinskong/rpi-omx-jpeg-encode


#include <stdio.h>

#include <bcm_host.h>
#define OMX_SKIP64BIT
#include <IL/OMX_Core.h>

#include "omxDump.h"
#include "omxHelper.h"
//#include "omxImageRead.h"
#include "omxJPEGDec.h"
#include "omxJPEGEnc.h"
#include "omxResize.h"
#include "omxTunnel.h"



static void destroy() {
    fputs("destroy\n", stderr);
    OMX_Deinit();
    bcm_host_deinit();
}



static void terminated(const int in_SIG) {
    fprintf(stderr, "\nTERMINATING due to signal %i\n", in_SIG);
    exit(1);
}



int main(int argc, const char * argv[]) {
    atexit(destroy);
    signal(SIGINT, terminated);

    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    
    bcm_host_init();
    omxErr = OMX_Init();
    omxAssert(omxErr);

    //omxDump();
    //omxImageRead();
    //omxJPEGDec();
    //omxJPEGEnc();
    //omxResize();
    omxTunnel();

    return 0;
}
