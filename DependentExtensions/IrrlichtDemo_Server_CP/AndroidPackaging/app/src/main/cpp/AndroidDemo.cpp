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

//Hardware Timestamp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <unistd.h>

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

CDemo* demo = nullptr;

/* Raknet stuff */
#include "RakNetStuff.h"

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

// 하드웨어 타임스탬프 지원 확인 함수
int IsHardwareTimestampSupported(const char* ifname) {
    int sockfd;
    struct ifreq ifr;
    struct ethtool_ts_info ts_info;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 0;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    ts_info.cmd = ETHTOOL_GET_TS_INFO;
    ifr.ifr_data = (caddr_t)&ts_info;

    if (ioctl(sockfd, SIOCETHTOOL, &ifr) == -1) {
        perror("ioctl");
        close(sockfd);
        return 0;
    }

    close(sockfd);

    // tx_type와 rx_type이 0이 아니면 지원한다고 판단
    if (ts_info.tx_types != 0 && ts_info.rx_filters != 0)
        return 1;

    return 0;
}


void android_main(android_app* state) {
    app_dummy();

    Engine engine{};

    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    engine.app = state;

    ////
    //struct ifaddrs* ifaddr;
    //if (getifaddrs(&ifaddr) == -1) {
    //    perror("getifaddrs");
    //}

    //printf("네트워크 인터페이스별 하드웨어 타임스탬프 지원 여부:\n");

    //for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    //    if (ifa->ifa_name == NULL)
    //        continue;

    //    // 중복 이름 출력 방지용: 인터페이스 이름이 바뀔 때만 출력
    //    static char prev_ifname[IFNAMSIZ] = { 0 };
    //    if (strcmp(prev_ifname, ifa->ifa_name) == 0)
    //        continue;
    //    strncpy(prev_ifname, ifa->ifa_name, IFNAMSIZ);

    //    int supported = IsHardwareTimestampSupported(ifa->ifa_name);
    //    printf("  %s: %s\n", ifa->ifa_name, supported ? "지원함" : "미지원");
    //}

    //freeifaddrs(ifaddr);
    ////

    bool fullscreen = false;
    bool music = true;
    bool shadows = false;
    bool additive = false;
    bool vsync = true;
    bool aa = false;
    core::stringw playerName;
    bool isServer = false;
    video::E_DRIVER_TYPE driverType = video::EDT_OGLES2;
	GamePlatform platform = GamePlatform::Holder;
    bool isLogged = false;
    bool isBot = false;
    int logCount = 1;
    int winScore = 10;
    
    const char* baseDir = "/storage/emulated/0/Download";
   
    demo = new CDemo(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer, platform, isLogged, logCount, baseDir, isBot, winScore);
    demo->state = state;
    demo->run();
    delete demo;
    demo = nullptr;
}

#else
#endif // __ANDROID__

