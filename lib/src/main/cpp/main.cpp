#include "main.h"
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>
#include <cstring>
#include <thread>
#include "zygisk.hpp"
// ImGui
#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"
// EGL
#include <EGL/egl.h>
#include <GLES3/gl3.h>
// Dobby
#include "dobby.h"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "MyModule", __VA_ARGS__)

static int width, height;

// -- ZYGISK

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        // Use JNI to fetch our process name
        const char *processName = env->GetStringUTFChars(args->nice_name, nullptr);
        const char *appDataDir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        preSpecialize(processName, appDataDir);
        env->ReleaseStringUTFChars(args->nice_name, processName);
        env->ReleaseStringUTFChars(args->app_data_dir, appDataDir);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        if (createThread) {
            std::thread hackThread(inject, gameDataDir);
            LOGD("Thread created");
        }
        
    }

private:
    Api *api;
    JNIEnv *env;
    bool createThread = false;
    char *gameDataDir;

    void preSpecialize(const char *processName, const char *appDataDir) {
        
        // Check is the current process is match with targeted process
        if (strcmp(processName, targetProcessName) == 0) {
            LOGD("Success, setup a thread");
            createThread = true;
            gameDataDir = new char[strlen(appDataDir) + 1];
            strcpy(gameDataDir, appDataDir);

        } else {
            LOGD("Skip, process unknown");
        }
    }

};

// Register our module class
REGISTER_ZYGISK_MODULE(MyModule)

// -- end ZYGISK

// -- HOOK IMGUI

// INPUT HANDLER
/* android::InputConsumer::initializeMotionEvent(android::MotionEvent*, android::InputMessage const*)
 * void InputConsumer::initializeMotionEvent(MotionEvent* event, const InputMessage* msg)
 */
void (*inputOrig)(void *thiz, void* event, void* msg);
void inputHook(void *thiz, void *event, void *msg) {
    inputOrig(thiz, event, msg);
    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *), thiz);
} 

// EGLSWAPBUFFER HANDLER
EGLBoolean (*eglSwapBufferOrig)(EGLDisplay eglDpy, EGLSurface eglSrf);
EGLBoolean eglSwapBufferHook(EGLDisplay eglDpy, EGLSurface eglSrf) {
    EGLint(width, height);
    eglQuerySurface(eglDpy, eglSrf, EGL_WIDTH, &width);
    eglQuerySurface(eglDpy, eglSrf, EGL_HEIGHT, &height);

    return eglSwapBufferOrig(eglDpy, eglSrf);
}
// INJECT OUR MENU
void inject(const char *targetProcessName) {
    // HOOK INPUT SYMBOL
    DobbySymbolResolver("libinput.so", "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE");

    // HOOK EGLSWAPBUFFER
    DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    
}

// -- END HOOK IMGUI