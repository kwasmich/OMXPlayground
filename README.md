# OMXPlayground #
OpenMAX Playground for Raspberry Pi's VideoCore IV



## Remarks ##



### image_read ###

This is not going to work. Quote from an Engineer [Reading an image using OMX.broadcom.image_read](https://www.raspberrypi.org/forums/viewtopic.php?f=70&t=85992&p=1213366#p1213366)

> image_read isn't going to work, and I'm surprised if it is actually in the build. (I'll look at removing it tomorrow if it is).

> When the chip was used as a co-processor there was a mode where it could also act as the main processor too, and that is the mode that image_read (and image_write) were intended for.

> With the Pi the GPU has no direct access to the SD card or other storage, so it can't directly read the image data. You need to be looking at image_decode and passing in the source data via buffers.



### image_decode ###

> [...] the output port supports several colour formats. However, the component doesn't support colour format changing,
> so the colour format will be set to match that emitted by the encountered image format.

You need to use image_resize in order to change the colour format to the desired (for instance JPEG -> RAW corresponds
to YUV -> RGB).

