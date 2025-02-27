#ifdef __ANDROID__
// BEGIN_INCLUDE(all)
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <android/choreographer.h>
#include <android/log.h>
#include <android/sensor.h>
#include <android/set_abort_message.h>
#include "android_native_app_glue.h"
#include <jni.h>
#include <android/native_activity.h>
#include "android_tools.h"
#include "android/window.h"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>

#define LOG_TAG "native-activity"

#define _LOG(priority, fmt, ...) \
  ((void)__android_log_print(priority, LOG_TAG, fmt, ##__VA_ARGS__))

//#define LOGE(fmt, ...) _LOG(ANDROID_LOG_ERROR, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) _LOG(ANDROID_LOG_ERROR, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) _LOG(ANDROID_LOG_WARN, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) _LOG(ANDROID_LOG_INFO, fmt, ##__VA_ARGS__)

// event type
#define EVENT_TYPE_POINTER_DOWN 0
#define EVENT_TYPE_POINTER_UP 1
#define EVENT_TYPE_POINTER_MOVE 2

static const int SOURCE_TOUCH_NAVIGATION = 0x00200000;

[[noreturn]] __attribute__((__format__(__printf__, 1, 2))) static void fatal(
    const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char* buf;
    if (vasprintf(&buf, fmt, ap) < 0) {
        android_set_abort_message("failed for format error message");
    }
    else {
        android_set_abort_message(buf);
        // Also log directly, since the default Android Studio logcat filter hides
        // the backtrace which would otherwise show the abort message.
        LOGE("%s", buf);
    }
    std::abort();
}

#define CHECK_NOT_NULL(value)                                           \
  do {                                                                  \
    if ((value) == nullptr) {                                           \
      fatal("%s:%d:%s must not be null", __PRETTY_FUNCTION__, __LINE__, \
            #value);                                                    \
    }                                                                   \
  } while (false)

/* Irrlicht stuff */
#include <irrlicht.h>
#include "CMainMenu.h"
#include "CDemo.h"
using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

//! we want the lights follow the model when it's moving
class CSceneNodeAnimatorFollowBoundingBox : public irr::scene::ISceneNodeAnimator
{
public:

    //! constructor
    CSceneNodeAnimatorFollowBoundingBox(irr::scene::ISceneNode* tofollow,
        const core::vector3df& offset, u32 frequency, s32 phase)
        : Offset(offset), ToFollow(tofollow), Frequency(frequency), Phase(phase)
    {
        if (ToFollow)
            ToFollow->grab();
    }

    //! destructor
    virtual ~CSceneNodeAnimatorFollowBoundingBox()
    {
        if (ToFollow)
            ToFollow->drop();
    }

    //! animates a scene node
    virtual void animateNode(irr::scene::ISceneNode* node, u32 timeMs)
    {
        if (0 == node || node->getType() != irr::scene::ESNT_LIGHT)
            return;

        irr::scene::ILightSceneNode* l = (irr::scene::ILightSceneNode*)node;

        if (ToFollow)
        {
            core::vector3df now = l->getPosition();
            now += ToFollow->getBoundingBox().getCenter();
            now += Offset;
            l->setPosition(now);
        }



#ifdef __ANDROID__
        irr::video::SColorHSL color;
        irr::video::SColorf rgb(0);
        color.Hue = ((timeMs + Phase) % Frequency) * (2.f * irr::core::PI / Frequency);
        color.Saturation = 1.f;
        color.Luminance = 0.5f;
        irr::video::SColorf rgb_color = rgb.toSColor();
        color.toRGB(rgb_color);
        irr::video::SColorf rgb_true(rgb_color);

        video::SLight light = l->getLightData();
        light.DiffuseColor = rgb_true; //rgb;
        l->setLightData(light);
#else
        irr::video::SColorHSL color;
        irr::video::SColorf rgb(0);
        color.Hue = ((timeMs + Phase) % Frequency) * (2.f * irr::core::PI / Frequency);
        color.Saturation = 1.f;
        color.Luminance = 0.5f;
        irr::video::SColor rgb_color = rgb.toSColor();
        color.toRGB(rgb_color);
        irr::video::SColorf rgb_true(rgb_color);

        video::SLight light = l->getLightData();
        light.DiffuseColor = rgb_true; //rgb;
        l->setLightData(light);
#endif

    }

    virtual scene::ISceneNodeAnimator* createClone(scene::ISceneNode* node, scene::ISceneManager* newManager = 0) { return 0; }
private:

    core::vector3df Offset;
    irr::scene::ISceneNode* ToFollow;
    s32 Frequency;
    s32 Phase;
};



void CMainMenu_loop(IrrlichtDevice* device, ITexture* irrlichtBack)
{
    u32 loop = 0;	// loop is reset when the app is destroyed unlike runCounter
    static u32 runCounter = 0;	// static's seem to survive even an app-destroy message (not sure if that's guaranteed).
    while (device->run())
    {
        /*
            The window seems to be always active in this setup.
            That's because when it's not active Android will stop the code from running.
        */
        if (device->isWindowActive())
        {
            device->getVideoDriver()->beginScene(true, true, SColor(0, 0, 0, 0));
            if (irrlichtBack)
                device->getVideoDriver()->draw2DImage(irrlichtBack, core::position2d<int>(0, 0));

            device->getSceneManager()->drawAll();
            device->getGUIEnvironment()->drawAll();
            device->getVideoDriver()->endScene();
        }
        device->yield(); // probably nicer to the battery
        ++runCounter;
        ++loop;
    }
}


class MyEventReceiver : public IEventReceiver
{
public:
    MyEventReceiver(android_app* app)
        : Device(0), AndroidApp(app), SpriteToMove(0), TouchID(-1)
    {
    }

    void Init(IrrlichtDevice* device)
    {
        Device = device;
    }

    virtual bool OnEvent(const SEvent& event)
    {
        if (event.EventType == EET_TOUCH_INPUT_EVENT)
        {
            /*
                For now we fake mouse-events. Touch-events will be handled inside Irrlicht in the future, but until
                that is implemented you can use this workaround to get a GUI which works at least for simple elements like
                buttons. That workaround does ignore multi-touch events - if you need several buttons pressed at the same
                time you have to handle that yourself.
            */
            SEvent fakeMouseEvent;
            fakeMouseEvent.EventType = EET_MOUSE_INPUT_EVENT;
            fakeMouseEvent.MouseInput.X = event.TouchInput.X;
            fakeMouseEvent.MouseInput.Y = event.TouchInput.Y;
            fakeMouseEvent.MouseInput.Shift = false;
            fakeMouseEvent.MouseInput.Control = false;
            fakeMouseEvent.MouseInput.ButtonStates = 0;
            fakeMouseEvent.MouseInput.Event = EMIE_COUNT;

            switch (event.TouchInput.Event)
            {
            case ETIE_PRESSED_DOWN:
            {
                // We only work with the first for now.force opengl error
                if (TouchID == -1)
                {
                    fakeMouseEvent.MouseInput.Event = EMIE_LMOUSE_PRESSED_DOWN;
                    if (Device)
                    {
                    }
                }
                break;
            }
            case ETIE_MOVED:
                if (TouchID == event.TouchInput.ID)
                {
                    fakeMouseEvent.MouseInput.Event = EMIE_MOUSE_MOVED;
                    fakeMouseEvent.MouseInput.ButtonStates = EMBSM_LEFT;
                }
                break;
            case ETIE_LEFT_UP:
                if (TouchID == event.TouchInput.ID)
                {
                    fakeMouseEvent.MouseInput.Event = EMIE_LMOUSE_LEFT_UP;
                    break;
                }
            default:
                break;
            }

            if (fakeMouseEvent.MouseInput.Event != EMIE_COUNT && Device)
            {
                Device->postEventFromUser(fakeMouseEvent);
            }
        }
        else if (event.EventType == EET_GUI_EVENT)
        {
            /*
                Show and hide the soft input keyboard when an edit-box get's the focus.
            */
            s32 id = event.GUIEvent.Caller->getID();
            switch (id)
            {

            }
            switch (event.GUIEvent.EventType)
            {
            case EGET_EDITBOX_ENTER:
                if (event.GUIEvent.Caller->getType() == EGUIET_EDIT_BOX)
                {
                    if (Device->getGUIEnvironment())
                        Device->getGUIEnvironment()->setFocus(NULL);
                    android::setSoftInputVisibility(AndroidApp, false);
                }
                break;
            case EGET_ELEMENT_FOCUS_LOST:
                if (event.GUIEvent.Caller->getType() == EGUIET_EDIT_BOX)
                {
                    /* 	Unfortunatly this only works on some android devices.
                        On other devices Android passes through touch-input events when the virtual keyboard is clicked while blocking those events in areas where the keyboard isn't.
                        Very likely an Android bug as it only happens in certain cases (like Android Lollipop with landscape mode on MotoG, but also some reports from other devices).
                        Or maybe Irrlicht still does something wrong.
                        Can't figure it out so far - so be warned - with landscape mode you might be better off writing your own keyboard.
                    */
                    android::setSoftInputVisibility(AndroidApp, false);
                }
                break;
            case EGET_ELEMENT_FOCUSED:
                if (event.GUIEvent.Caller->getType() == EGUIET_EDIT_BOX)
                {
                    android::setSoftInputVisibility(AndroidApp, true);
                }
                break;
            default:
                break;
            }
        }

        return false;
    }
private:
    IrrlichtDevice* Device;
    android_app* AndroidApp;
    gui::IGUIElement* SpriteToMove;
    core::rect<s32> SpriteStartRect;
    core::position2d<irr::s32> TouchStartPos;
    s32 TouchID;


};

/**
 * Our saved state data.
 */
struct SavedState {
    float angle;
    int32_t x;
    int32_t y;

    //ref : Endless-Turnnel  CookedEvent
    int type;
    int motionPointerId;
    bool motionIsOnScreen;
    float motionX, motionY;
    float motionMinX, motionMaxX;
    float motionMinY, motionMaxY;
};

/**
 * Shared state for our app.
 */
struct Engine {
    android_app* app;

    ASensorManager* sensorManager;
    const ASensor* accelerometerSensor;
    ASensorEventQueue* sensorEventQueue;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
    SavedState state;

    void CreateSensorListener(ALooper_callbackFunc callback) {
        CHECK_NOT_NULL(app);

        sensorManager = ASensorManager_getInstance();
        if (sensorManager == nullptr) {
            return;
        }

        accelerometerSensor = ASensorManager_getDefaultSensor(
            sensorManager, ASENSOR_TYPE_ACCELEROMETER);
        sensorEventQueue = ASensorManager_createEventQueue(
            sensorManager, app->looper, ALOOPER_POLL_CALLBACK, callback, this);
    }

    /// Resumes ticking the application.
    void Resume() {
        // Checked to make sure we don't double schedule Choreographer.
        if (!running_) {
            running_ = true;
            ScheduleNextTick();
        }
    }

    /// Pauses ticking the application.
    ///
    /// When paused, sensor and input events will still be processed, but the
    /// update and render parts of the loop will not run.
    void Pause() { running_ = false; }

private:
    bool running_;

    void ScheduleNextTick() {
        AChoreographer_postFrameCallback(AChoreographer_getInstance(), Tick, this);
    }

    /// Entry point for Choreographer.
    ///
    /// The first argument (the frame time) is not used as it is not needed for
    /// this sample. If you copy from this sample and make use of that argument,
    /// note that there's an API bug: that time is a signed 32-bit nanosecond
    /// counter on 32-bit systems, so it will roll over every ~2 seconds. If your
    /// minSdkVersion is 29 or higher, use AChoreographer_postFrameCallback64
    /// instead, which is 64-bits for all architectures. Otherwise, bitwise-and
    /// the value with the upper bits from CLOCK_MONOTONIC.
    ///
    /// \param data The Engine being ticked.
    static void Tick(long, void* data) {
        CHECK_NOT_NULL(data);
        auto engine = reinterpret_cast<Engine*>(data);
        engine->DoTick();
    }

    void DoTick() {
        if (!running_) {
            return;
        }

        // Input and sensor feedback is handled via their own callbacks.
        // Choreographer ensures that those callbacks run before this callback does.

        // Choreographer does not continuously schedule the callback. We have to re-
        // register the callback each time we're ticked.
        ScheduleNextTick();
        Update();
        DrawFrame();
    }

    void Update() {
        state.angle += .01f;
        if (state.angle > 1) {
            state.angle = 0;
        }
    }

    void DrawFrame() {
        if (display == nullptr) {
            // No display.
            return;
        }

        // Just fill the screen with a color.
        glClearColor(((float)state.x) / width, state.angle,
            ((float)state.y) / height, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        eglSwapBuffers(display, surface);
    }
};

/**
 * Initialize an EGL context for the current display.
 */
static int engine_init_display(Engine* engine) {
    // initialize OpenGL ES and EGL

    /*
     * Here specify the attributes of the desired configuration.
     * Below, we select an EGLConfig with at least 8 bits per color
     * component compatible with on-screen windows
     */
    const EGLint attribs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                              EGL_BLUE_SIZE,    8,
                              EGL_GREEN_SIZE,   8,
                              EGL_RED_SIZE,     8,
                              EGL_NONE };
    EGLint w, h, format;
    EGLint numConfigs;
    EGLConfig config = nullptr;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display, nullptr, nullptr);

    /* Here, the application chooses the configuration it desires.
     * find the best match if possible, otherwise use the very first one
     */
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);
    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    assert(supportedConfigs);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs,
        &numConfigs);
    assert(numConfigs);
    auto i = 0;
    for (; i < numConfigs; i++) {
        auto& cfg = supportedConfigs[i];
        EGLint r, g, b, d;
        if (eglGetConfigAttrib(display, cfg, EGL_RED_SIZE, &r) &&
            eglGetConfigAttrib(display, cfg, EGL_GREEN_SIZE, &g) &&
            eglGetConfigAttrib(display, cfg, EGL_BLUE_SIZE, &b) &&
            eglGetConfigAttrib(display, cfg, EGL_DEPTH_SIZE, &d) && r == 8 &&
            g == 8 && b == 8 && d == 0) {
            config = supportedConfigs[i];
            break;
        }
    }
    if (i == numConfigs) {
        config = supportedConfigs[0];
    }

    if (config == nullptr) {
        LOGW("Unable to initialize EGLConfig");
        return -1;
    }

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
     * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
     * As soon as we picked a EGLConfig, we can safely reconfigure the
     * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    surface =
        eglCreateWindowSurface(display, config, engine->app->window, nullptr);

    /* A version of OpenGL has not been specified here.  This will default to
     * OpenGL 1.0.  You will need to change this if you want to use the newer
     * features of OpenGL like shaders. */
    context = eglCreateContext(display, config, nullptr, nullptr);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGW("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    engine->display = display;
    engine->context = context;
    engine->surface = surface;
    engine->width = w;
    engine->height = h;
    engine->state.angle = 0;

    // Check openGL on the system
    auto opengl_info = { GL_VENDOR, GL_RENDERER, GL_VERSION, GL_EXTENSIONS };
    for (auto name : opengl_info) {
        auto info = glGetString(name);
        LOGI("OpenGL Info: %s", info);
    }
    // Initialize GL state.
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_DEPTH_TEST);

    return 0;
}

/**
 * Tear down the EGL context currently associated with the display.
 */
static void engine_term_display(Engine* engine) {
    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
            EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->display, engine->context);
        }
        if (engine->surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->display, engine->surface);
        }
        eglTerminate(engine->display);
    }
    engine->Pause();
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
}

/**
 * Process the next input event.
 */
static int32_t engine_handle_input(android_app* app,
    AInputEvent* event) {
    auto* engine = (Engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        engine->state.x = AMotionEvent_getX(event, 0);
        engine->state.y = AMotionEvent_getY(event, 0);
        return 1;
    }
    return 0;
}

/**
 * Process the next main command.
 */
static void engine_handle_cmd(android_app* app, int32_t cmd) {
    auto* engine = (Engine*)app->userData;
    switch (cmd) {
    case APP_CMD_SAVE_STATE:
        // The system has asked us to save our current state.  Do so.
        engine->app->savedState = malloc(sizeof(SavedState));
        *((SavedState*)engine->app->savedState) = engine->state;
        engine->app->savedStateSize = sizeof(SavedState);
        break;
    case APP_CMD_INIT_WINDOW:
        // The window is being shown, get it ready.
        if (engine->app->window != nullptr) {
            engine_init_display(engine);
        }
        break;
    case APP_CMD_TERM_WINDOW:
        // The window is being hidden or closed, clean it up.
        engine_term_display(engine);
        break;
    case APP_CMD_GAINED_FOCUS:
        // When our app gains focus, we start monitoring the accelerometer.
        if (engine->accelerometerSensor != nullptr) {
            ASensorEventQueue_enableSensor(engine->sensorEventQueue,
                engine->accelerometerSensor);
            // We'd like to get 60 events per second (in us).
            ASensorEventQueue_setEventRate(engine->sensorEventQueue,
                engine->accelerometerSensor,
                (1000L / 60) * 1000);
        }
        engine->Resume();
        break;
    case APP_CMD_LOST_FOCUS:
        // When our app loses focus, we stop monitoring the accelerometer.
        // This is to avoid consuming battery while not being used.
        if (engine->accelerometerSensor != nullptr) {
            ASensorEventQueue_disableSensor(engine->sensorEventQueue,
                engine->accelerometerSensor);
        }
        engine->Pause();
        break;
    default:
        break;
    }
}

int OnSensorEvent(int /* fd */, int /* events */, void* data) {
    CHECK_NOT_NULL(data);
    Engine* engine = reinterpret_cast<Engine*>(data);

    CHECK_NOT_NULL(engine->accelerometerSensor);
    ASensorEvent event;
    while (ASensorEventQueue_getEvents(engine->sensorEventQueue, &event, 1) > 0) {
        LOGI("accelerometer: x=%f y=%f z=%f", event.acceleration.x,
            event.acceleration.y, event.acceleration.z);
    }

    // From the docs:
    //
    // Implementations should return 1 to continue receiving callbacks, or 0 to
    // have this file descriptor and callback unregistered from the looper.
    return 1;
}

//void test(android_app* state) {
//    stringc mediaPath = "media/";
//    irr::android::SDisplayMetrics displayMetrics;
//    memset(&displayMetrics, 0, sizeof displayMetrics);
//    irr::android::getDisplayMetrics(state, displayMetrics);
//    video::E_DRIVER_TYPE driverType = video::EDT_OGLES2;
//
//    SIrrlichtCreationParameters param;
//    param.DriverType = driverType;				// android:glEsVersion in AndroidManifest.xml should be "0x00020000"
//    param.WindowSize = core::dimension2d<u32>(displayMetrics.widthPixels, displayMetrics.heightPixels);	// using 0,0 it will automatically set it to the maximal size
//    param.PrivateData = state;
//    param.Bits = 24;
//    param.ZBufferBits = 16;
//    param.AntiAlias = 0;
//    MyEventReceiver receiver;
//    param.EventReceiver = &receiver;
//    IrrlichtDevice* device = createDeviceEx(param);
//  
//    video::IVideoDriver* driver = device->getVideoDriver();
//    scene::ISceneManager* smgr = device->getSceneManager();
//    io::IFileSystem* fs = device->getFileSystem();
//    ILogger* logger = device->getLogger();
//
//    for (u32 i = 0; i < fs->getFileArchiveCount(); ++i)
//    {
//        io::IFileArchive* archive = fs->getFileArchive(i);
//        if (archive->getType() == io::E_FILE_ARCHIVE_TYPE::EFAT_ANDROID_ASSET)
//        {
//            archive->addDirectoryToFileList(mediaPath);
//            break;
//        }
//    }
//
//    device->getFileSystem()->addFileArchive(mediaPath + "map-20kdm2.pk3", true, true, io::EFAT_ZIP);
//    scene::IAnimatedMesh* mesh = smgr->getMesh("20kdm2.bsp");
//    scene::ISceneNode* node = 0;
//
//    if (mesh)
//        node = smgr->addOctreeSceneNode(mesh->getMesh(0), 0, -1, 1024);
//    if (node)
//        node->setPosition(core::vector3df(-1300, -144, -1249));
//    smgr->addCameraSceneNodeFPS();
//    //device->getCursorControl()->setVisible(false);
//
//    int lastFPS = -1;
//
//    while (device->run())
//    {
//        if (device->isWindowActive())
//        {
//            driver->beginScene(video::ECBF_COLOR | video::ECBF_DEPTH, video::SColor(255, 200, 200, 200));
//            smgr->drawAll();
//            driver->endScene();
//
//            int fps = driver->getFPS();
//
//            if (lastFPS != fps)
//            {
//                core::stringw str = L"Irrlicht Engine - Quake 3 Map example [";
//                str += driver->getName();
//                str += L"] FPS:";
//                str += fps;
//
//                device->setWindowCaption(str.c_str());
//                lastFPS = fps;
//            }
//            device->yield();
//        }
//        else
//            device->yield();
//    }
//
//    /*
//    In the end, delete the Irrlicht device.
//    */
//    device->drop();
//
//}


/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
 void android_CMainMenu(Engine* engine, android_app* state) {

     stringc mediaPath = "media/";
     /* Irrlicht stuff */
     MyEventReceiver receiver(state);
     SIrrlichtCreationParameters param;
     //	param.DriverType = EDT_OGLES1;				// android:glEsVersion in AndroidManifest.xml should be "0x00010000" (requesting 0x00020000 will also guarantee that ES1 works)
     param.DriverType = EDT_OGLES2;				// android:glEsVersion in AndroidManifest.xml should be "0x00020000"
     param.WindowSize = dimension2d<u32>(512, 384);	// using 0,0 it will automatically set it to the maximal size
     param.PrivateData = state;
     param.Bits = 24;
     param.ZBufferBits = 16;
     param.AntiAlias = 0;
     param.EventReceiver = &receiver;
 
     IrrlichtDevice* device = createDeviceEx(param);
     if (device == 0)
         return;
 
     receiver.Init(device);
 
     IVideoDriver* driver = device->getVideoDriver();
     ISceneManager* smgr = device->getSceneManager();
     IGUIEnvironment* guienv = device->getGUIEnvironment();
     ILogger* logger = device->getLogger();
     IFileSystem* fs = device->getFileSystem();
 
     /* Access to the Android native window. You often need this when accessing NDK functions like we are doing here.
        Note that windowWidth/windowHeight have already subtracted things like the taskbar which your device might have,
        so you get the real size of your render-window.
     */
     ANativeWindow* nativeWindow = static_cast<ANativeWindow*>(driver->getExposedVideoData().OGLESAndroid.Window);
     int32_t windowWidth = ANativeWindow_getWidth(state->window);
     int32_t windowHeight = ANativeWindow_getHeight(state->window);
 
     /* Get display metrics. We are accessing the Java functions of the JVM directly in this case as there is no NDK function for that yet.
        Checkout android_tools.cpp if you want to know how that is done. */
     irr::android::SDisplayMetrics displayMetrics;
     memset(&displayMetrics, 0, sizeof displayMetrics);
     irr::android::getDisplayMetrics(state, displayMetrics);
 
     char strDisplay[1000];
     sprintf(strDisplay, "Window size:(%d/%d)\nDisplay size:(%d/%d)", windowWidth, windowHeight, displayMetrics.widthPixels, displayMetrics.heightPixels);
     logger->log(strDisplay);
 
     core::dimension2d<s32> dim(driver->getScreenSize());
     sprintf(strDisplay, "getScreenSize:(%d/%d)", dim.Width, dim.Height);
     logger->log(strDisplay);
 
    
 
     // The Android assets file-system does not know which sub-directories it has (blame google).
     // So we have to add all sub-directories in assets manually. Otherwise we could still open the files,
     // but existFile checks will fail (which are for example needed by getFont).
     for (u32 i = 0; i < fs->getFileArchiveCount(); ++i)
     {
         IFileArchive* archive = fs->getFileArchive(i);
         if (archive->getType() == EFAT_ANDROID_ASSET)
         {
             archive->addDirectoryToFileList(mediaPath);
             break;
         }
     }
 
     /* Set the font-size depending on your device.
        dpi=dots per inch. 1 inch = 2.54 cm. */
     IGUISkin* skin = guienv->getSkin();
     IGUIFont* font = 0;
     if (displayMetrics.xdpi < 100)	// just guessing some value where fontsize might start to get too small
         font = guienv->getFont(mediaPath + "fonthaettenschweiler.bmp");
     else
         font = guienv->getFont(mediaPath + "bigfont.png");
     if (font)
         skin->setFont(font);
 
     /* CMainMenu */
     s32 selected;
     bool start;
     bool fullscreen;
     bool music;
     bool shadows;
     bool additive;
     bool transparent;
     bool vsync;
     bool aa;
     bool isServer;
     gui::IGUIButton* startButton;
     irr::gui::IGUIEditBox* nameEditBox;
     video::SColor SkinColor[gui::EGDC_COUNT];
 
     // add images
 
     const s32 leftX = 260;
 
     // add tab control
     gui::IGUITabControl* tabctrl = guienv->addTabControl(core::rect<int>(leftX, 10, 512 - 10, 384 - 10),
         0, true, true);
     gui::IGUITab* optTab = tabctrl->addTab(L"Demo");
     gui::IGUITab* aboutTab = tabctrl->addTab(L"About");
 
     // add list box
 
     gui::IGUIListBox* box = guienv->addListBox(core::rect<int>(10, 10, 220, 120), optTab, 1);
     box->addItem(L"OpenGL 1.5");
     box->addItem(L"Direct3D 8.1");
     box->addItem(L"Direct3D 9.0c");
     box->addItem(L"Burning's Video 0.39");
     box->addItem(L"Irrlicht Software Renderer 1.0");
     box->setSelected(selected);
 
     // add button
 
     startButton = guienv->addButton(core::rect<int>(30, 295, 200, 324), optTab, 2, L"Start Demo");
 
     // add checkbox
 
     const s32 d = 50;
 
     guienv->addCheckBox(fullscreen, core::rect<int>(20, 85 + d, 130, 110 + d),
         optTab, 3, L"Fullscreen");
     guienv->addCheckBox(music, core::rect<int>(135, 85 + d, 245, 110 + d),
         optTab, 4, L"Music & Sfx");
     guienv->addCheckBox(shadows, core::rect<int>(20, 110 + d, 135, 135 + d),
         optTab, 5, L"Realtime shadows");
     guienv->addCheckBox(additive, core::rect<int>(20, 135 + d, 230, 160 + d),
         optTab, 6, L"Old HW compatible blending");
     guienv->addCheckBox(vsync, core::rect<int>(20, 160 + d, 230, 185 + d),
         optTab, 7, L"Vertical synchronisation");
     guienv->addCheckBox(aa, core::rect<int>(135, 110 + d, 245, 135 + d),
         optTab, 8, L"Antialiasing");
     guienv->addCheckBox(isServer, core::rect<int>(135, 160 + d, 245, 185 + d),
         optTab, 9, L"Server");
 
     // RakNet: Add edit box
     nameEditBox = guienv->addEditBox(L"Your name here", core::rect<int>(20, 185 + d, 230, 210 + d), true, optTab, 9);
 
 
     // add about text
 
     wchar_t* text2 = L"This is the tech demo of the Irrlicht engine. To start, "\
         L"select a video driver which works best with your hardware and press 'Start Demo'.\n"\
         L"What you currently see is displayed using the Burning Software Renderer (Thomas Alten).\n"\
         L"The Irrlicht Engine was written by me, Nikolaus Gebhardt. The models, "\
         L"maps and textures were placed at my disposal by B.Collins, M.Cook and J.Marton. The music was created by "\
         L"M.Rohde and is played back by irrKlang.\n"\
         L"For more informations, please visit the homepage of the Irrlicht engine:\nhttp://irrlicht.sourceforge.net\n"\
         L"\n*** MULTIPLAYER UPDATE ***\n"\
         L"Peer to peer multiplayer added in two days using RakNet.\n"\
         L"For a description of the networking design, see included readme.txt .\n";
 
     guienv->addStaticText(text2, core::rect<int>(10, 10, 230, 320),
         true, true, aboutTab);
 
     // add md2 model
 
     scene::IAnimatedMesh* mesh = smgr->getMesh(mediaPath + "faerie.md2");
     scene::IAnimatedMeshSceneNode* modelNode = smgr->addAnimatedMeshSceneNode(mesh);
     if (modelNode)
     {
         modelNode->setPosition(core::vector3df(0.f, 0.f, -5.f));
         modelNode->setMaterialTexture(0, driver->getTexture(mediaPath + "faerie2.bmp"));
         modelNode->setMaterialFlag(video::EMF_LIGHTING, true);
         modelNode->getMaterial(0).Shininess = 28.f;
         modelNode->getMaterial(0).NormalizeNormals = true;
         modelNode->setMD2Animation(scene::EMAT_STAND);
     }
 
     // set ambient light (no sun light in the catacombs)
     smgr->setAmbientLight(video::SColorf(0.f, 0.f, 0.f));
 
     scene::ISceneNodeAnimator* anim;
     scene::ISceneNode* bill;
 
     // add light 1 (sunset orange)
     scene::ILightSceneNode* light1 =
         smgr->addLightSceneNode(0, core::vector3df(10.f, 10.f, 0),
             video::SColorf(0.86f, 0.38f, 0.05f), 200.0f);
 
     // add fly circle animator to light 1
     anim = smgr->createFlyCircleAnimator(core::vector3df(0, 0, 0), 30.0f, -0.004f, core::vector3df(0.41f, 0.4f, 0.0f));
     light1->addAnimator(anim);
     anim->drop();
 
     // let the lights follow the model...
     anim = new CSceneNodeAnimatorFollowBoundingBox(modelNode, core::vector3df(0, 16, 0), 4000, 0);
     //light1->addAnimator(anim);
     anim->drop();
 
     // attach billboard to the light
     bill = smgr->addBillboardSceneNode(light1, core::dimension2d<f32>(10, 10));
     bill->setMaterialFlag(video::EMF_LIGHTING, false);
     bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
     bill->setMaterialTexture(0, driver->getTexture(mediaPath + "particlered.bmp"));
 
 #if 1
     // add light 2 (nearly red)
     scene::ILightSceneNode* light2 =
         smgr->addLightSceneNode(0, core::vector3df(0, 1, 0),
             video::SColorf(0.9f, 1.0f, 0.f, 0.0f), 200.0f);
 
     // add fly circle animator to light 1
     anim = smgr->createFlyCircleAnimator(core::vector3df(0, 0, 0), 30.0f, 0.004f, core::vector3df(0.41f, 0.4f, 0.0f));
     light2->addAnimator(anim);
     anim->drop();
 
     // let the lights follow the model...
     anim = new CSceneNodeAnimatorFollowBoundingBox(modelNode, core::vector3df(0, -8, 0), 2000, 0);
     //light2->addAnimator(anim);
     anim->drop();
 
 
     // attach billboard to the light
     bill = smgr->addBillboardSceneNode(light2, core::dimension2d<f32>(10, 10));
     bill->setMaterialFlag(video::EMF_LIGHTING, false);
     bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
     bill->setMaterialTexture(0, driver->getTexture(mediaPath + "particlered.bmp"));
 
     // add light 3 (nearly blue)
     scene::ILightSceneNode* light3 =
         smgr->addLightSceneNode(0, core::vector3df(0, -1, 0),
             video::SColorf(0.f, 0.0f, 0.9f, 0.0f), 40.0f);
 
     // add fly circle animator to light 2
     anim = smgr->createFlyCircleAnimator(core::vector3df(0, 0, 0), 40.0f, 0.004f, core::vector3df(-0.41f, -0.4f, 0.0f));
     light3->addAnimator(anim);
     anim->drop();
 
     // let the lights follow the model...
     anim = new CSceneNodeAnimatorFollowBoundingBox(modelNode, core::vector3df(0, 8, 0), 8000, 0);
     //light3->addAnimator(anim);
     anim->drop();
 
     // attach billboard to the light
     bill = smgr->addBillboardSceneNode(light3, core::dimension2d<f32>(10, 10));
     if (bill)
     {
         bill->setMaterialFlag(video::EMF_LIGHTING, false);
         bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
         bill->setMaterialTexture(0, driver->getTexture(mediaPath + "portal1.bmp"));
     }
 #endif
 
     // create a fixed camera
     smgr->addCameraSceneNode(0, core::vector3df(45, 0, 0), core::vector3df(0, 0, 10));
 
     // irrlicht logo and background
     // add irrlicht logo
     bool oldMipMapState = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
     driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
 
     guienv->addImage(driver->getTexture(mediaPath + "irrlichtlogo2.png"),
         core::position2d<s32>(5, 5));
 
     video::ITexture* irrlichtBack = driver->getTexture(mediaPath + "demoback.jpg");
 
     driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, oldMipMapState);
 
     // query original skin color
     //getOriginalSkinColor();
     for (s32 i = 0; i < gui::EGDC_COUNT; ++i)
     {
         SkinColor[i] = skin->getColor((gui::EGUI_DEFAULT_COLOR)i);
     }
 
     // set transparency
     //setTransparency();
     for (u32 i = 0; i < gui::EGDC_COUNT; ++i)
     {
         video::SColor col = SkinColor[i];
 
         if (false == transparent)
             col.setAlpha(255);
 
         skin->setColor((gui::EGUI_DEFAULT_COLOR)i, col);
     }
 
 
     /*
         Mainloop. Applications usually never quit themself in Android. The OS is responsible for that.
     */
     CMainMenu_loop(device, irrlichtBack);
 
     /* Cleanup */
     device->setEventReceiver(0);
     device->closeDevice();
     device->drop();
 }

void android_main(android_app* state) {
    app_dummy();

    Engine engine{};

    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    engine.app = state;

    bool fullscreen = false;
    bool music = true;
    bool shadows = false;
    bool additive = false;
    bool vsync = false;
    bool aa = false;
    core::stringw playerName;
    bool isServer = false;
    video::E_DRIVER_TYPE driverType = video::EDT_OGLES2;

    /*
    CMainMenu menu;
    menu.state = state;

    if (menu.run(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer))
    {
        CDemo demo(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer);
        demo.run();
    }*/

    android_CMainMenu(&engine, state);

}

#else
#endif // __ANDROID__

