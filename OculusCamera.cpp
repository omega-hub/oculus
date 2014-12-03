#include <omega.h>
#include <omegaGl.h>

#include <OVR.h>
#include <../Src/OVR_CAPI_GL.h>

#define GLEW_MX
#include "GL/glew.h"

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include "GL/freeglut.h"
#endif

#include "OculusCamera.h"

///////////////////////////////////////////////////////////////////////////////
void OculusCamera::handleEvent(const Event& evt)
{
	// If any button has been pressed, dismiss the HMD warning display
	if(myInitialized && evt.getType() == Event::Down)
	{
		ovrHmd_DismissHSWDisplay(myHMD);
	}

	// Implement simple gamepad-based navigation.
	if(evt.getServiceType() == Service::Controller)
	{
		float x = evt.getExtraDataFloat(0);
		float z = evt.getExtraDataFloat(1);

		float y = evt.getExtraDataFloat(3);
		float tresh = 0.3f;

		if(abs(x) < tresh) x = 0;
		if(abs(y) < tresh) y = 0;
		if(abs(z) < tresh) z = 0;

		ovrQuatf& ovro = myHmdState.HeadPose.ThePose.Orientation;
		Quaternion q(ovro.w, ovro.x, ovro.y, ovro.z);
		mySpeedVector = q * Vector3f(x,y,z);
	}

	Camera::handleEvent(evt);
}

///////////////////////////////////////////////////////////////////////////////
void OculusCamera::updateTraversal(const UpdateContext& context)
{
	translate(mySpeedVector * context.dt, Node::TransformWorld);
	Camera::updateTraversal(context);
}

///////////////////////////////////////////////////////////////////////////////
void OculusCamera::initializeGraphics(const DrawContext& context)
{
#ifdef OMEGA_OS_WIN
	// Windows: retrieve HWND of current window and attach it to oculus HMD
	HDC dc = wglGetCurrentDC();
	HWND wnd = WindowFromDC(dc);
	ovrHmd_AttachToWindow(myHMD, wnd, NULL, NULL);
#endif

    //Configure Stereo settings.
    OVR::Sizei recommenedTex0Size = ovrHmd_GetFovTextureSize(myHMD, ovrEye_Left,  myHMD->DefaultEyeFov[0], 1.0f);
    OVR::Sizei recommenedTex1Size = ovrHmd_GetFovTextureSize(myHMD, ovrEye_Right, myHMD->DefaultEyeFov[1], 1.0f);
	OVR::Sizei RenderTargetSize;
    RenderTargetSize.w = recommenedTex0Size.w + recommenedTex1Size.w;
    RenderTargetSize.h = max ( recommenedTex0Size.h, recommenedTex1Size.h );

	Renderer* r = context.renderer;
	myRenderTexture = r->createTexture();
	myRenderTexture->initialize(RenderTargetSize.w, RenderTargetSize.h);

    // The actual RT size may be different due to HW limits.
    RenderTargetSize.w = myRenderTexture->getWidth();
    RenderTargetSize.h = myRenderTexture->getHeight();

    // Initialize eye rendering information.
    // The viewport sizes are re-computed in case RenderTargetSize changed due to HW limitations.
    ovrFovPort eyeFov[2] = { myHMD->DefaultEyeFov[0], myHMD->DefaultEyeFov[1] } ;

    myEyeRenderViewport[0].Pos  = OVR::Vector2i(0,0);
    myEyeRenderViewport[0].Size = OVR::Sizei(RenderTargetSize.w / 2, RenderTargetSize.h);
    myEyeRenderViewport[1].Pos  = OVR::Vector2i((RenderTargetSize.w + 1) / 2, 0);
    myEyeRenderViewport[1].Size = myEyeRenderViewport[0].Size;

	myEyeTexture[0].OGL.Header.API = ovrRenderAPI_OpenGL;
	myEyeTexture[0].OGL.Header.TextureSize = RenderTargetSize;
	myEyeTexture[0].OGL.Header.RenderViewport = myEyeRenderViewport[0];
	myEyeTexture[0].OGL.TexId = myRenderTexture->getGLTexture();
    // Right eye uses the same texture, but different rendering viewport.
    myEyeTexture[1] = myEyeTexture[0];
    myEyeTexture[1].OGL.Header.RenderViewport = myEyeRenderViewport[1];

	// Configure OpenGL
	ovrGLConfig cfg;
	cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
	cfg.OGL.Header.RTSize = OVR::Sizei(myHMD->Resolution.w, myHMD->Resolution.h);
#ifdef OMEGA_OS_WIN
	cfg.OGL.Window = wnd;
	cfg.OGL.DC = dc;
#endif

	if(!ovrHmd_ConfigureRendering(myHMD, &cfg.Config,  
		ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette |
        ovrDistortionCap_TimeWarp | ovrDistortionCap_Overdrive,
								   eyeFov, myEyeRenderDesc))
	{
		owarn("ovrHmd_ConfigureRendering failed");
	}

	// Create render target
	myRenderTarget = r->createRenderTarget(RenderTarget::RenderToTexture);
	myDepthTexture = r->createTexture();
	myDepthTexture->initialize(RenderTargetSize.w, RenderTargetSize.h, GL_DEPTH_COMPONENT);
	myRenderTarget->setTextureTarget(myRenderTexture, myDepthTexture);

	myInitialized = true;
}

///////////////////////////////////////////////////////////////////////////////
void OculusCamera::startFrame(const FrameInfo& frame)
{
	if(!myInitialized) return;

	ovrHmd_BeginFrame(myHMD, frame.frameNum); 


	ovrVector3f hmdToEyeViewOffset[2] = 
		{ myEyeRenderDesc[0].HmdToEyeViewOffset, myEyeRenderDesc[1].HmdToEyeViewOffset };
	ovrHmd_GetEyePoses(myHMD, 0, hmdToEyeViewOffset, myEyeRenderPose, &myHmdState);


}

///////////////////////////////////////////////////////////////////////////////
void OculusCamera::finishFrame(const FrameInfo& frame)
{
	if(!myInitialized) return;
	// Let OVR do distortion rendering, Present and flush/sync
	ovrHmd_EndFrame(myHMD, myEyeRenderPose, &myEyeTexture[0].Texture);
}

///////////////////////////////////////////////////////////////////////////////
void OculusCamera::clear(DrawContext& context)
{
	if(!myInitialized) return;
	myRenderTarget->clear();
}

///////////////////////////////////////////////////////////////////////////////
void OculusCamera::beginDraw(DrawContext& context)
{
	// Create a render target if we have not done it yet.
	if(!myInitialized) initializeGraphics(context);

	if(context.task == DrawContext::SceneDrawTask)
	{
		static float     BodyYaw = 0;
		Vector3f h = getPosition();// + getHeadOffset();
		OVR::Vector3f HeadPos(h[0], h[1], h[2]);

		ovrRecti& erv = myEyeRenderViewport[(context.eye == DrawContext::EyeLeft) ? 0 : 1];
		ovrPosef& erp = myEyeRenderPose[(context.eye == DrawContext::EyeLeft) ? 0 : 1];
		ovrEyeRenderDesc& erd = myEyeRenderDesc[(context.eye == DrawContext::EyeLeft) ? 0 : 1];

		context.viewport.min[0] = erv.Pos.x;
		context.viewport.max[0] = erv.Pos.x + myEyeRenderViewport[0].Size.w;
		context.viewport.min[1] = erv.Pos.y;
		context.viewport.max[1] = erv.Pos.y + myEyeRenderViewport[0].Size.h;

		context.tile->activeRect = Rect(0,0,myRenderTexture->getWidth(),myRenderTexture->getHeight()); 

        OVR::Matrix4f rollPitchYaw = OVR::Matrix4f::RotationY(BodyYaw);
        OVR::Matrix4f finalRollPitchYaw = rollPitchYaw * OVR::Matrix4f(erp.Orientation);
        OVR::Vector3f finalUp            = finalRollPitchYaw.Transform(OVR::Vector3f(0,1,0));
        OVR::Vector3f finalForward       = finalRollPitchYaw.Transform(OVR::Vector3f(0,0,-1));
        OVR::Vector3f shiftedEyePos      = HeadPos + rollPitchYaw.Transform(erp.Position);
        OVR::Matrix4f view = OVR::Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp); 
		OVR::Matrix4f proj = ovrMatrix4f_Projection(erd.Fov, 0.01f, 10000.0f, true);

		for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
		{
			context.modelview(i, j) = view.M[i][j];
			context.projection(i, j) = proj.M[i][j];
		}
		myRenderTarget->bind();
	}
}

///////////////////////////////////////////////////////////////////////////////
void OculusCamera::endDraw(DrawContext& context)
{
	if(context.task == DrawContext::SceneDrawTask)
	{
		myRenderTarget->unbind();
	}
}

