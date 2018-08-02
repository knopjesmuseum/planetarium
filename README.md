# planetarium

```
LIBS = -lEGL -lGLESv2 -lfreeimage -lbcm_host -lvcos -lwiringPi -lconfig -L/opt/vc/lib
DIRS = -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
```

## dependencies
* lib EGL: https://www.mesa3d.org/egl.html
* glesv2: OpenGL graphics rendering library for embedded systems, version 2.0: http://nitlanguage.org/catalog/glesv2.html
* freeimage: Library for loading image files like JPG, PNG etc.
* bcm_host: bcm_host is the Broadcom hardware interface library
* vcos: RaspberryPi Video Core ... 
* wiringPi: library to interface Raspberry Pi GPIO pins
* config: C/C++ library for processing configuration files https://hyperrealm.github.io/libconfig/
* /opt/vc/lib: folder with installed libraries: https://github.com/raspberrypi/firmware/tree/master/opt/vc/lib 
