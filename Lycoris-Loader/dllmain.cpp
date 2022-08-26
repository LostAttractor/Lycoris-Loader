#include "pch.h"
#include "dllmain.h"
#include "jni.h"
#include "jvmti.h"
#include "classes.h"
#include "windows.h"
#include <time.h>
#include <iostream>
#include "stdio.h"
#include "HookLib.h"
#include "MargeleAntiCheatHitboxDisabler.h"
#include "utils.h"
#include "loader.h"

#pragma comment(lib, "HookLib.lib")
#pragma comment(lib, "Zydis.lib")

char* randstr(const int len) {
    char str[20];
    int i;
    srand(time(NULL));
    for (i = 0; i < len; ++i) {
        switch ((rand() % 3))
        {
        case 1:
            str[i] = 'A' + rand() % 26;
            break;
        case 2:
            str[i] = 'a' + rand() % 26;
            break;
        default:
            str[i] = '0' + rand() % 10;
            break;
        }
    }
    str[++i] = '\0';
    return str;
}

VOID OutputLastError(DWORD errorCode) {
    CHAR errorString[256] = { 0 };
    sprintf_s(errorString, "Last error code: %lu", errorCode);
    MessageBoxA(NULL, errorString, randstr(8), MB_OK | MB_ICONEXCLAMATION);
}

//DWORD WINAPI MainThread(CONST LPVOID lpParam)
DWORD WINAPI MainThread(JNIEnv* env) {
    std::cout << "Starting Injecting!" << std::endl;
    HMODULE jvmDll = GetModuleHandleA("jvm.dll");
    if (!jvmDll)
    {
        DWORD lastError = GetLastError();
        //MessageBoxA(NULL, "Can't Handle \"jvm.dll\"", randstr(8), MB_OK | MB_ICONERROR);
        OutputLastError(lastError);
    }

    typedef jint(JNICALL* GetCreatedJavaVMs)(JavaVM**, jsize, jsize*);
    GetCreatedJavaVMs jni_GetCreatedJavaVMs = (GetCreatedJavaVMs)GetProcAddress(jvmDll, "JNI_GetCreatedJavaVMs");;

    typedef jclass(*FindClassFromCaller)(JNIEnv* env, const char* name, jboolean init, jobject loader, jclass caller);
    FindClassFromCaller findClassFromCaller = (FindClassFromCaller)GetProcAddress(jvmDll, "JVM_FindClassFromCaller");

    jsize count;
    JavaVM* jvm;
    if (jni_GetCreatedJavaVMs((JavaVM**)&jvm, 1, &count) != JNI_OK || count == 0) { //获取JVM
        //MessageBoxA(nullptr, "Error: 0x00000003", "LycorisAgent", MB_OK | MB_ICONERROR); //无法获取JVM
        return NULL;
    }
    jvmtiEnv* jvmTiEnv;

    jvm->GetEnv((void**)(&jvmTiEnv), JVMTI_VERSION);
    if (!jvmTiEnv)
    {
        MessageBoxA(NULL, "Can't attach to JVMTI", "ELoader", MB_OK | MB_ICONERROR);
        //jvm->DetachCurrentThread();
        return NULL;
    }

    jclass* loadedClasses;
    jint loadedClassesCount = 0;
    jvmTiEnv->GetLoadedClasses(&loadedClassesCount, &loadedClasses);
    jclass launchHandlerClass = NULL;
    jclass launchClassLoaderClass = NULL;
    jclass enityRenderClass = NULL;
    jclass LaunchWrapper = NULL;
    jclass java_lang_ClassLoader = NULL;
    jobject CtxClassLoader = NULL;

    /////让我们通过这↑里↓ 来获取所有的线程 从而得到ClientThread 并拿到 CtxCL/////
    //线程类
    jclass thread = NULL;
    //获取所有的栈追踪
    jmethodID getAllStackTraces = NULL;
    //获取名字
    jmethodID getThreadName = NULL;
    //拿上下文类加载器
    jmethodID getContextClassLoader = NULL;
    //Map类
    jclass map = NULL;
    //获取KeySet
    jmethodID KeySet = NULL;
    //Set类
    jclass set = NULL;
    //转数组
    jmethodID toArray = NULL;
    //客户端线程
    jthread clientThread = NULL;

    //定义类
    //jmethodID defineClass = NULL;

    //客户端入口类
    jclass loaderClass = NULL;

    //是否已经加载
    bool isLoaded = false;

    //寻找类
    for (jint i = 0; i < loadedClassesCount; i++)
    {
        char* signature;
        jvmTiEnv->GetClassSignature(loadedClasses[i], &signature, NULL);

        if (!strcmp(signature, "Ljava/lang/Thread;")) {
            thread = loadedClasses[i];
            //std::cout << "Found java.lang.Thread" << std::endl;
        }

        if (!strcmp(signature, "Ljava/util/Set;")) {
            set = loadedClasses[i];
            //std::cout << "Found java.util.Set" << std::endl;
        }

        if (!strcmp(signature, "Ljava/util/Map;")) {
            map = loadedClasses[i];
            //std::cout << "Found java.util.Map" << std::endl;
        }

        if (!strcmp(signature, "Ljava/lang/ClassLoader;")) {
            java_lang_ClassLoader = loadedClasses[i];
            //std::cout << "Found java.lang.ClassLoader" << std::endl;
        }
        if (!strcmp(signature, "LLoader;")) {
            isLoaded = true;
            loaderClass = loadedClasses[i];
            std::cout << "Found Defined Client Loaded" << std::endl;
        }
    }

    //寻找方法
    getAllStackTraces = env->GetStaticMethodID(thread, "getAllStackTraces", "()Ljava/util/Map;");
    getThreadName = env->GetMethodID(thread, "getName", "()Ljava/lang/String;");
    getContextClassLoader = env->GetMethodID(thread, "getContextClassLoader", "()Ljava/lang/ClassLoader;");

    KeySet = env->GetMethodID(map, "keySet", "()Ljava/util/Set;");
    toArray = env->GetMethodID(set, "toArray", "()[Ljava/lang/Object;");
    //defineClass = env->GetMethodID(java_lang_ClassLoader, "defineClass", "([BII)Ljava/lang/Class;");

    //得到所有线程
    jobjectArray allThread = (jobjectArray)env->CallObjectMethod(env->CallObjectMethod(env->CallStaticObjectMethod(thread, getAllStackTraces), KeySet), toArray);

    jsize atLength = env->GetArrayLength(allThread);

    for (jsize i = 0; i < atLength; i++) {
        jobject curThread = env->GetObjectArrayElement(allThread, i);
        jstring ctName = (jstring)env->CallObjectMethod(curThread, getThreadName);
        const char* ctCname = env->GetStringUTFChars(ctName, 0);
        std::cout << ctCname << std::endl;
        if (!strcmp(ctCname, "Client thread")) {
            clientThread = curThread;
            std::cout << "Found Client Thread" << std::endl;
            break;
        }
    }

    //拿到类加载器
    CtxClassLoader = env->CallObjectMethod(clientThread, getContextClassLoader);
            
    java_lang_ClassLoader = env->FindClass("java/lang/ClassLoader");
    if (!CtxClassLoader) {
        printf("[Loader] Minecraft Not Found");
        MessageBoxA(NULL, "Minecraft Not Found", randstr(8), MB_OK | MB_ICONERROR);
        return NULL;
    }
    jclass loadingClass = NULL;
    jsize tempClassIndex = 0;
    jsize lastClassIndex = 0;
    for (jsize j = 0; j != classCount; j++) {
        char* lastClass = new char[classSizes[j] + 1];
        for (jsize classIndex = 0; classIndex != classSizes[j]; classIndex++) {
            tempClassIndex++;;
            lastClass[classIndex] = classes[lastClassIndex + classIndex];
        }
        if (!isLoaded) {
            loadingClass = env->DefineClass(NULL, CtxClassLoader, (jbyte*)lastClass, classSizes[j]);

            char* signature;
            jvmTiEnv->GetClassSignature(loadingClass, &signature, NULL);

            printf("[Loader] Defining class %s +\n", signature);

            if (!strcmp(signature, "LLoader;")) {
                loaderClass = loadingClass;
            }
            if (!loadingClass) {

            }
            delete[]lastClass;
            lastClassIndex = tempClassIndex;
        }
    }

    if (!loaderClass) {
        std::cout << "[Loader] Loader Class Not Found" << std::endl;
        MessageBoxA(NULL, "Loader Class Not Found", randstr(8), MB_OK | MB_ICONEXCLAMATION);
        return NULL;
    }
    jmethodID loaderid = NULL;
    loaderid = env->GetMethodID(loaderClass, "<init>", "()V");

    if (!loaderid) {
        std::cout << "[Loader] Loader Method Not Found" << std::endl;
        MessageBoxA(NULL, "Loader Method Not Found", randstr(8), MB_OK | MB_ICONEXCLAMATION);
    }

    jobject LoadClent = env->NewObject(loaderClass, loaderid);
    if (!LoadClent) {
        std::cout << "[Loader] Client Loader Constructor Error" << std::endl;
    }
    std::cout << "[Loader] Client Loaded" << std::endl;
    //FreeConsole();
}


PVOID unload(PVOID arg) {
    HMODULE hm = NULL;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPWSTR)&unload, &hm);
    FreeLibraryAndExitThread(hm, 0);
}


//====================================================================================================

typedef void(*Java_org_lwjgl_opengl_GL11_nglFlush)(JNIEnv* env, jclass clazz, jlong lVar);

Java_org_lwjgl_opengl_GL11_nglFlush nglFlush = NULL;

void nglFlush_Hook(JNIEnv* env, jclass clazz, jlong lVar) {
    nglFlush(env, clazz, lVar);
    RemoveHook(nglFlush);
    MainThread(env);
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)unload, NULL, 0, NULL);
    return;
}

//====================================================================================================


PVOID WINAPI hook(PVOID arg) {
    HMODULE jvm = GetModuleHandlePeb(L"lwjgl64.dll");

    Java_org_lwjgl_opengl_GL11_nglFlush nglFlush_address = (Java_org_lwjgl_opengl_GL11_nglFlush)GetProcAddressPeb(jvm, "Java_org_lwjgl_opengl_GL11_nglFlush");
    SetHook(nglFlush_address, nglFlush_Hook, reinterpret_cast<PVOID*>(&nglFlush));

    return NULL;
}

extern "C" __declspec(dllexport) BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    DisableThreadLibraryCalls(hinstDLL);

    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        AllocConsole();
        HANDLE hdlWrite = GetStdHandle(STD_OUTPUT_HANDLE);
        FILE* stream;
        freopen_s(&stream, "CONOUT$", "w+t", stdout);
        freopen_s(&stream, "CONIN$", "r+t", stdin);
        //  MessageBoxA(NULL, "ATTACHED", "NHC", MB_OK | MB_ICONINFORMATION);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)hook, NULL, 0, NULL);
        break;
    }

    return TRUE;
}
