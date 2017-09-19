# OMXPlayground #
OpenMAX Playground for Raspberry Pi's VideoCore IV



## Remarks ##



### image_read ###

No success so far.



### image_decode ###

> [...] the output port supports several colour formats. However, the component doesn't support colour format changing,
> so the colour format will be set to match that emitted by the encountered image format.

You need to use image_resize in order to change the colour format to the desired (for instance JPEG -> RAW corresponds
to YUV -> RGB).

