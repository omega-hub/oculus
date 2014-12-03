#ifndef __OCULUS_CAMERA_H__
#define __OCULUS_CAMERA_H__ 
#include <omega.h>

#include <OVR.h>
#include <../Src/OVR_CAPI_GL.h>

using namespace omega;

///////////////////////////////////////////////////////////////////////////////
class OculusCamera: public Camera
{
public:
	OculusCamera(Engine* e, ovrHmd hmd): 
		Camera(e),
		myHMD(hmd),
		myInitialized(false),
		mySpeedVector(0,0,0)
	{
	}

    virtual void handleEvent(const Event& evt);
	virtual void beginDraw(DrawContext& context);
	virtual void endDraw(DrawContext& context);
    virtual void startFrame(const FrameInfo& frame);
    virtual void finishFrame(const FrameInfo& frame);
	virtual void clear(DrawContext& context);
	virtual bool isEnabledInContext(const DrawContext& context) { return true; }
    virtual void updateTraversal(const UpdateContext& context);

	void initializeGraphics(const DrawContext& context);

private:
	bool myInitialized;
	Ref<RenderTarget> myRenderTarget;
	Ref<Texture> myRenderTexture;
	Ref<Texture> myDepthTexture;
	
    /// OVR hardware
	ovrTrackingState myHmdState;
	ovrHmd myHMD;
	ovrEyeRenderDesc myEyeRenderDesc[2];
	ovrPosef myEyeRenderPose[2];
	ovrRecti myEyeRenderViewport[2];
	ovrGLTexture myEyeTexture[2];

	// Navigation stuff
	Vector3f mySpeedVector;
};
#endif