# rift - Oculus Rift support for omegalib
This module adds Oculus Rift support to omegalib。

**Version Requirement**: This module only works with omegalib version 6.0-beta3 and higher

## Building Instructions
This module builds like any other omegalib module, with a couple more steps. You need to have the Oculus SDK **version 0.4.4** on your machine. Let's assume you are on Windows and
installed the SDk in `C:\OculusSDK` and you want to 
add oculus support to a local omegalib master branch. Use the [omegalib maintenance tools](https://github.com/uic-evl/omegalib/wiki/MaintenanceTools) as follows:
```
omega add master oculus
omega set master OVR_INCLUDE_DIR C:/OculusSDK/LibOVR/Include
omega set master OVR_LIB C:/OculusSDK/LibOVR/Lib/Win32/VS2013/libovr.lib
omega build master
```
Istructions for OSX are similar but adjust the paths/library name accordingly.

## Using the rift module
To run your application using the Oculus Rift render, simply choose an oculus rift configuration at 
、startup. For instance when using orun:
```
> orun -c oculus/rift.cfg -s <script name>
```	

## How it works
the oculus module implements a new omegalib display system and camera that use the Oculus SDk to control
rendering. 
