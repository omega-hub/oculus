#ifndef __OCULUS_DISPLAY_SYSTEM_H__
#define __OCULUS_DISPLAY_SYSTEM_H__
#include <omega.h>
#include <OVR.h>

#include "OculusCamera.h"

using namespace omega;

///////////////////////////////////////////////////////////////////////////////
// The oculus rift service implements the ICameraListener interface to perform
// postprocessing during rendering.
class OculusDisplaySystem: public DisplaySystem
{
public:
	OculusDisplaySystem();

	Engine* getEngine() { return myEngine; }
	Renderer* getRenderer() { return myRenderer; }
	GpuContext* getGpuContext() { return myGpuContext; }

	virtual void run();
	void exit();

private:
	bool myInitialized;
	Ref<OculusCamera> myCamera;
	Ref<GpuContext> myGpuContext;
	Ref<Renderer> myRenderer;
	Ref<Engine> myEngine;
	//Vector2f myViewportSize;
	
    /// OVR hardware
	ovrHmd myHMD;
};
#endif