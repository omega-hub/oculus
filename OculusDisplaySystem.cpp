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

// 8/12/14 LOGIC CHANGE
// Now 'button' processing for Keyboard events works same as gamepads:
// Flag is set for button down events and stays set until AFTER the corresponding
// button up, so isButtonUp(ButtonName) works consistently and as expected.
#define HANDLE_KEY_FLAG(keycode, flag) \
    if(key == keycode && type == Event::Down) sKeyFlags |= Event::flag; \
    if(key == keycode && type == Event::Up) keyFlagsToRemove |= Event::flag;

///////////////////////////////////////////////////////////////////////////////
static uint sKeyFlags = 0;
void keyboardButtonCallback( uint key, Event::Type type )
{
	ServiceManager* sm = SystemManager::instance()->getServiceManager();
    sm->lockEvents();

    Event* evt = sm->writeHead();
    evt->reset(type, Service::Keyboard, key);

    uint keyFlagsToRemove = 0;

    HANDLE_KEY_FLAG(296, Alt)
    HANDLE_KEY_FLAG(292, Shift)
    HANDLE_KEY_FLAG(294, Ctrl)

    // Convert arrow keys to buttons. This allows user code to do es. 
    // evt.isButtonDown(Event::ButtonLeft) without having to make a 
    // separate call to isKeyDown when using keyboards instead of gamepads.
    HANDLE_KEY_FLAG(KC_LEFT, ButtonLeft);
    HANDLE_KEY_FLAG(KC_RIGHT, ButtonRight);
    HANDLE_KEY_FLAG(KC_DOWN, ButtonDown);
    HANDLE_KEY_FLAG(KC_UP, ButtonUp);

    // Add some special keys as buttons
    HANDLE_KEY_FLAG(KC_RETURN, Button4);
    HANDLE_KEY_FLAG(KC_BACKSPACE, Button5);
    HANDLE_KEY_FLAG(KC_TAB, Button6);
    HANDLE_KEY_FLAG(KC_HOME, Button7);

    evt->setFlags(sKeyFlags);

    // Remove the bit of all buttons that have been unpressed.
    sKeyFlags &= ~keyFlagsToRemove;

	// If ESC is pressed, request exit.
	if(evt->isKeyDown(27)) SystemManager::instance()->postExitRequest();

    sm->unlockEvents();
}

///////////////////////////////////////////////////////////////////////////////
void glutKeyDown(unsigned char key, int x, int y)
{
    keyboardButtonCallback((uint)key, Event::Down);
}

///////////////////////////////////////////////////////////////////////////////
void glutKeyUp(unsigned char key, int x, int y)
{
    keyboardButtonCallback((uint)key, Event::Up);
}

///////////////////////////////////////////////////////////////////////////////
int sWheelAccum = 0;
void mouseWheelCallback(int btn, int wheel, int x, int y)
{
	// GLUT generates a ton of wheel events. We accumulate them and
	// generate single event when we pass a reasonable treshold
	sWheelAccum += wheel;

	if(abs(sWheelAccum) > 100)
	{
		sWheelAccum = 0;
		ServiceManager* sm = SystemManager::instance()->getServiceManager();
		sm->lockEvents();

		Event* evt = sm->writeHead();
		evt->reset(Event::Zoom, Service::Pointer);
		evt->setPosition(x, y);

		evt->setExtraDataType(Event::ExtraDataIntArray);
		evt->setExtraDataInt(0, wheel);

		sm->unlockEvents();
	}
}

///////////////////////////////////////////////////////////////////////////////
static Ray sPointerRay;
unsigned int sButtonFlags = 0;
void mouseMotionCallback(int x, int y)
{
	ServiceManager* sm = SystemManager::instance()->getServiceManager();
	sm->lockEvents();

	Event* evt = sm->writeHead();
	evt->reset(Event::Move, Service::Pointer);
	evt->setPosition(x, y);
	evt->setFlags(sButtonFlags);

	DisplaySystem* ds = SystemManager::instance()->getDisplaySystem();
	sPointerRay = ds->getViewRay(Vector2i(x, y));

	evt->setExtraDataType(Event::ExtraDataVector3Array);
	evt->setExtraDataVector3(0, sPointerRay.getOrigin());
	evt->setExtraDataVector3(1, sPointerRay.getDirection());

	sm->unlockEvents();
}

///////////////////////////////////////////////////////////////////////////////
void mouseButtonCallback(int button, int state, int x, int y)
{
	ServiceManager* sm = SystemManager::instance()->getServiceManager();
	sm->lockEvents();

	Event* evt = sm->writeHead();
	evt->reset(state ? Event::Up : Event::Down, Service::Pointer);

	if(button == 3) mouseWheelCallback(button, 1, x, y);
	if(button == 4) mouseWheelCallback(button, -1, x, y);

	// Update button flags
	if(evt->getType() == Event::Down)
	{
		if(button == GLUT_LEFT_BUTTON) sButtonFlags |= Event::Left;
		if(button == GLUT_RIGHT_BUTTON) sButtonFlags |= Event::Right;
		if(button == GLUT_MIDDLE_BUTTON) sButtonFlags |= Event::Middle;
	}
	else
	{
		if(button == GLUT_LEFT_BUTTON) sButtonFlags &= ~Event::Left;
		if(button == GLUT_RIGHT_BUTTON) sButtonFlags &= ~Event::Right;
		if(button == GLUT_MIDDLE_BUTTON) sButtonFlags &= ~Event::Middle;
	}

	evt->setPosition(x, y);
	// Note: buttons only contain active button flags, so we invoke
	// setFlags first, to generate ButtonDown events that
	// still contain the flag of the currently presset button.
	// This is needed to make vent.isButtonUp(button) calls working.
	if(state)
	{
		sButtonFlags = button;
		evt->setFlags(sButtonFlags);
	}
	else
	{
		evt->setFlags(sButtonFlags);
		sButtonFlags = button;
	}

	evt->setExtraDataType(Event::ExtraDataVector3Array);
	evt->setExtraDataVector3(0, sPointerRay.getOrigin());
	evt->setExtraDataVector3(1, sPointerRay.getDirection());

	sm->unlockEvents();
}

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

	//if(SystemManager::instance()->isExitRequested())
	//{
	//	exit(0);
	//}
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
void exitfunc()
{
	sInstance->exit();
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
	int winid = glutCreateWindow("oculus"); 

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

		// Register mouse/keyboard callbacks if enabled
		if(syscfg->getBoolValue("config/oculus/keyboardMouseEnabled", false))
		{
			glutKeyboardFunc(glutKeyDown); 
			glutKeyboardUpFunc(glutKeyUp); 
			glutPassiveMotionFunc(mouseMotionCallback);
			glutMotionFunc(mouseMotionCallback);
			glutMouseFunc(mouseButtonCallback);
			/** Apple's GLUT does not support the mouse wheel **/
	#ifndef __APPLE__
			glutMouseWheelFunc(mouseWheelCallback);
	#endif
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

	glutCloseFunc(exitfunc);

	while(!SystemManager::instance()->isExitRequested())
	{
		glutMainLoopEvent();
	}

	glutDestroyWindow(winid);
	glutExit();
}

///////////////////////////////////////////////////////////////////////////////
void OculusDisplaySystem::exit()
{
	sInstance = NULL;

	// No OVR functions involving memory are allowed after this.
	ovrHmd_Destroy(myHMD);
    ovr_Shutdown(); 
}
