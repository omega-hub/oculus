#include <omega.h>
#include <omegaGl.h>

#include "OculusDisplaySystem.h"

#include <../Src/OVR_CAPI_GL.h>


#define GLEW_MX
#include "GL/glew.h"

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include "GL/freeglut.h"
#endif

GLEWContext sGLEWContext;

using namespace omega;

// The global service instance, used by the python API to control the service.
class OculusDisplaySystem* sInstance = NULL;

///////////////////////////////////////////////////////////////////////////////
void displayCallback(void) 
{
	static float lt = 0.0f;
	static uint64 frame = 0;

	OculusDisplaySystem* ds = sInstance;
	Engine* as = ds->getEngine();
	Renderer* ac = ds->getRenderer();

	// Compute dt.
	float t = (float)((double)clock() / CLOCKS_PER_SEC);
	UpdateContext uc;
	uc.dt = t - lt;
	lt = t;

// Make sure we preserve the OpenGL context (some libraries like OSG may 
// play with it during initialization)
#ifdef OMEGA_OS_WIN
    HDC _hdc = wglGetCurrentDC();
    HGLRC _hglrc = wglGetCurrentContext();
#endif
	as->update(uc);

#ifdef OMEGA_OS_WIN
	wglMakeCurrent(_hdc, _hglrc);
#endif

	// setup the context viewport.
	DrawContext dc;
	DisplayTileConfig dtc(ds->getDisplayConfig());
	dtc.stereoMode = DisplayTileConfig::SideBySide;
	dtc.enabled = true;

	dc.tile = &dtc;
	dc.gpuContext = ds->getGpuContext();
	dc.renderer = ac;

	Camera* cam = Engine::instance()->getDefaultCamera();

	dc.camera = cam;

	// Process events.
	ServiceManager* im = SystemManager::instance()->getServiceManager();
	int av = im->getAvailableEvents();
	if(av != 0)
	{
		Event evts[OMICRON_MAX_EVENTS];
		im->getEvents(evts, ServiceManager::MaxEvents);

		// Dispatch events to application server.
		for( int evtNum = 0; evtNum < av; evtNum++)
		{
			as->handleEvent(evts[evtNum]);
		}
	}

	dc.drawFrame(frame++);

	glutPostRedisplay();

	// poll the input manager for new events.
	im->poll();

	if(SystemManager::instance()->isExitRequested())
	{
		exit(0);
	}
}

///////////////////////////////////////////////////////////////////////////////
OculusDisplaySystem::OculusDisplaySystem():
	myCamera(NULL),
	myInitialized(false)
{
	char* argv = "";
	int argcp = 1;

	// Setup and initialize Glut
	glutInit(&argcp, &argv);
}

///////////////////////////////////////////////////////////////////////////////
void OculusDisplaySystem::run() 
{
	sInstance = this;
	myHMD = NULL;

	// Initialize the Oculus Rift
	omsg("\n\n------- Oculus Rift Initialization");
	ovr_Initialize();
	if(!myHMD)
	{
		myHMD = ovrHmd_Create(0);
		if(!myHMD) owarn("Oculus Rift not detected,");
		else if(myHMD->ProductName[0] == '\0') owarn("Rift detected, display not enabled");
		else omsg("Oculus Rift Initialization OK\n\n");
	}

	if(!myHMD) return;

	glutInitWindowPosition(myHMD->WindowsPos.x, myHMD->WindowsPos.y);
	glutInitWindowSize(myHMD->Resolution.w, myHMD->Resolution.h);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH);
	glutCreateWindow("oculus"); 

	glewSetContext(&sGLEWContext);

	// Init glew
#ifndef __APPLE__
	glewInit();
#endif

	glutDisplayFunc(displayCallback); 
	myGpuContext = new GpuContext();

	// Setup and initialize the application server and client.
	ApplicationBase* app = SystemManager::instance()->getApplication();
	if(app)
	{
		myEngine = new Engine(app);
		myRenderer = new Renderer(myEngine);

		myEngine->initialize();

		// Register myself as a camera listener.
		myCamera = new OculusCamera(myEngine, myHMD);
		Config* syscfg = SystemManager::instance()->getSystemConfig();
		if(syscfg->exists("config/camera"))
		{
			Setting& s = syscfg->lookup("config/camera");
			myCamera->setup(s);
		}

		myEngine->setDefaultCamera(myCamera);
		myEngine->getScene()->addChild(myCamera);

		//myFrameBuffer = new RenderTarget();
		//myFrameBuffer->initialize(RenderTarget::TypeFrameBuffer, myResolution[0], myResolution[1]);
		//myAppClient->setup();
		myRenderer->setGpuContext(myGpuContext);
		myRenderer->initialize();
	}

    ovrHmd_SetEnabledCaps(myHMD, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);

	// Start the sensor which informs of the Rift's pose and motion
    ovrHmd_ConfigureTracking(myHMD,   ovrTrackingCap_Orientation |
                                    ovrTrackingCap_MagYawCorrection |
                                    ovrTrackingCap_Position, 0);

	glutMainLoop();

	sInstance = NULL;
	// No OVR functions involving memory are allowed after this.
    ovr_Shutdown(); 
}
