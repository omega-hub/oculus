# rift - Oculus Rift support for omegalib
This module adds Oculus Rift support to omegalib。

## Building the rift module
This module builds like any other omegalib module. You need to have the Oculus SDK on your machine,
and set the `OVR_INCLUDE_DIR` and `OVR_LIB` to the SDK include directory and library file respectively.

## Using the rift module
To run your application using the Oculus Rift render, simply choose an oculus rift configuration at 
、startup. For instance when using orun:
```
> orun -c oculus/rift.cfg -s <script name>
```	

## How it works
the oculus module implements a new omegalib display system and camera that use the Oculus SDk to control
rendering. 
