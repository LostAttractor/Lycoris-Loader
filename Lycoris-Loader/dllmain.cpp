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
    GetCreatedJavaVMs jni_GetCreatedJavaVMs = (GetCreatedJavaVMs)GetProcAddress(jvmDll, "JNI_GetCreatedJavaVMs");

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

    //客户端入口类
    jclass loaderClass = NULL;

    //是否已经加载 (Re-Transform)
    bool isLoaded = false;

    //寻找类
    for (jint i = 0; i < loadedClassesCount; i++)
    {
        char* signature;
        jvmTiEnv->GetClassSignature(loadedClasses[i], &signature, NULL);

        // 通过判断入口类是否定义来判断是否二次注入
        if (!strcmp(signature, "LLoader;")) {
            isLoaded = true;
            loaderClass = loadedClasses[i];
            std::cout << "Found Defined Client Loaded" << std::endl;
        }
    }

    //寻找类
    jclass java_lang_ClassLoader = env->FindClass("java/lang/ClassLoader");
    jclass java_lang_Thread = env->FindClass("java/lang/Thread");
    jclass java_util_Set = env->FindClass("java/util/Set");
    jclass java_util_Map = env->FindClass("java/util/Map");
    //寻找方法
    //获取KeySet
    jmethodID KeySet = env->GetMethodID(java_util_Map, "keySet", "()Ljava/util/Set;");
    //转数组
    jmethodID toArray = env->GetMethodID(java_util_Set, "toArray", "()[Ljava/lang/Object;");
    //获取所有的栈追踪
    jmethodID getAllStackTraces = env->GetStaticMethodID(java_lang_Thread, "getAllStackTraces", "()Ljava/util/Map;");
    //获取名字
    jmethodID getThreadName = env->GetMethodID(java_lang_Thread, "getName", "()Ljava/lang/String;");
    //拿上下文类加载器
    jmethodID getContextClassLoader = env->GetMethodID(java_lang_Thread, "getContextClassLoader", "()Ljava/lang/ClassLoader;");

    //得到所有线程
    jobjectArray allThread = (jobjectArray)env->CallObjectMethod(env->CallObjectMethod(env->CallStaticObjectMethod(java_lang_Thread, getAllStackTraces), KeySet), toArray);

    //查找客户端线程
    jthread clientThread = NULL;
    jsize atLength = env->GetArrayLength(allThread);
    for (jsize i = 0; i < atLength; i++) {
        jobject currectThread = env->GetObjectArrayElement(allThread, i);
        jstring ctName = (jstring)env->CallObjectMethod(currectThread, getThreadName);
        const char* ctCname = env->GetStringUTFChars(ctName, 0);
        std::cout << ctCname << std::endl;
        if (!strcmp(ctCname, "Client thread")) {
            clientThread = currectThread;
            std::cout << "Found Client Thread" << std::endl;
            break;
        }
    }

    //拿到类加载器
    jobject ContextClassLoader = env->CallObjectMethod(clientThread, getContextClassLoader);

    if (!ContextClassLoader) {
        printf("[Loader] ContextClassLoader Not Found");
        MessageBoxA(NULL, "ContextClassLoader Not Found", randstr(8), MB_OK | MB_ICONERROR);
        return NULL;
    }

    // 定义所以类
    if (!isLoaded) {
        for (jsize i = 0, currectOffset = 0; i < classCount; i++) {
            unsigned int currectSize = classSizes[i];
            char* currectClass = new char[currectSize]; //从堆内存申请数组
            std::copy(classes + currectOffset, classes + currectOffset + currectSize, currectClass);
            jclass loadingClass = loadingClass = env->DefineClass(NULL, ContextClassLoader, (jbyte*)currectClass, currectSize);

            char* signature;
            jvmTiEnv->GetClassSignature(loadingClass, &signature, NULL);

            printf("[Loader] Defining class %s +\n", signature);

            if (!strcmp(signature, "LLoader;")) {
                loaderClass = loadingClass;
            }
            delete[]currectClass; //释放内存
            currectOffset += currectSize;
        }
    }

    // 初始化入口类
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

typedef void(*Java_org_lwjgl_opengl_GL11_nglFlush)(JNIEnv* env, jclass clazz, jlong lVar);

Java_org_lwjgl_opengl_GL11_nglFlush nglFlush = nullptr;

void nglFlush_Hook(JNIEnv* env, jclass clazz, jlong lVar) {
    while (nglFlush == nullptr); //等待赋值执行完毕
    nglFlush(env, clazz, lVar); //运行原始方法
    unhook(nglFlush); //取消hook
    MainThread(env);
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)unload, NULL, 0, NULL);
    return;
}


PVOID WINAPI lwjgl_hook(PVOID arg) {
    HMODULE jvm = GetModuleHandlePeb(L"lwjgl64.dll");

    Java_org_lwjgl_opengl_GL11_nglFlush nglFlush_address = (Java_org_lwjgl_opengl_GL11_nglFlush)GetProcAddressPeb(jvm, "Java_org_lwjgl_opengl_GL11_nglFlush"); // 获得入口方法
    nglFlush = (Java_org_lwjgl_opengl_GL11_nglFlush) hook(nglFlush_address, nglFlush_Hook); //替换nglFlush方法为Hook方法并保存原始方法函数指针

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
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)lwjgl_hook, NULL, 0, NULL);
        break;
    }

    return TRUE;
}
