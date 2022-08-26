// dllmain.cpp : ?? DLL ?????????
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

char* randstr(char* str, const int len)
{
    srand(time(NULL));
    int i;
    for (i = 0; i < len; ++i)
    {
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

VOID OutputLastError(DWORD errorCode)
{
    CHAR errorString[256] = { 0 };
    sprintf_s(errorString, "Last error code: %lu", errorCode);
    //MessageBox(NULL, errorString, randstr(name, 8), MB_OK | MB_ICONEXCLAMATION);
}

//DWORD WINAPI MainThread(CONST LPVOID lpParam)
DWORD WINAPI MainThread(JNIEnv* env)
{
    //MessageBoxA(NULL, "debug staet", "debug", MB_OK | MB_ICONERROR);
    std::cout << "Starting injecting" << std::endl;
    char name[20];
    //MessageBoxA(NULL, "Starting injecting", randstr(name, 8), MB_OK | MB_ICONINFORMATION);
    HMODULE jvmDll = GetModuleHandleA("jvm.dll");
    if (!jvmDll)
    {
        DWORD lastError = GetLastError();
        MessageBoxA(NULL, "Can't find jvm.dll module handle", randstr(name, 8), MB_OK | MB_ICONERROR);
        OutputLastError(lastError);
        ExitThread(0);
    }
    FARPROC getJvmsVoidPtr = GetProcAddress(jvmDll, "JNI_GetCreatedJavaVMs");

    typedef jclass(*FindClassFromCaller)(JNIEnv* env, const char* name, jboolean init, jobject loader, jclass caller);
    FindClassFromCaller findClassFromCaller = (FindClassFromCaller)GetProcAddress(jvmDll, "JVM_FindClassFromCaller");

    typedef jint(JNICALL* GetCreatedJavaVMs)(JavaVM**, jsize, jsize*);
    GetCreatedJavaVMs jni_GetCreatedJavaVMs = (GetCreatedJavaVMs)getJvmsVoidPtr;
    jsize nVMs;
    jni_GetCreatedJavaVMs(NULL, 0, &nVMs);
    JavaVM** buffer = new JavaVM * [nVMs];
    jni_GetCreatedJavaVMs(buffer, nVMs, &nVMs);
    //MessageBoxA(NULL, "debug 1staet", "debug", MB_OK | MB_ICONERROR);
    if (nVMs == 0)
    {
        MessageBoxA(NULL, "JVM not found!", randstr(name, 8), MB_OK | MB_ICONERROR);
        ExitThread(0);
    }
    if (nVMs > 0)
    {
        for (jsize i = 0; i < nVMs; i++)
        {

            JavaVM* jvm = buffer[i];
            jvmtiEnv* jvmTiEnv;
;
            jvm->GetEnv((void**)(&jvmTiEnv), JVMTI_VERSION);
            if (!jvmTiEnv)
            {
                MessageBoxA(NULL, "Can't attach to JVMTI", "ELoader", MB_OK | MB_ICONERROR);
                //jvm->DetachCurrentThread();
                break;
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
            jmethodID defineClass = NULL;


            //寻找类
            for (jint i = 0; i < loadedClassesCount; i++)
            {
                char* signature;
                jvmTiEnv->GetClassSignature(loadedClasses[i], &signature, NULL);

                if (!strcmp(signature, "Ljava/lang/Thread;")) {
                    thread = loadedClasses[i];
                    std::cout << "find Thread" << std::endl;
                }

                if (!strcmp(signature, "Ljava/util/Set;")) {
                    set = loadedClasses[i];
                    std::cout << "find Set" << std::endl;
                }

                if (!strcmp(signature, "Ljava/util/Map;")) {
                    map = loadedClasses[i];
                    std::cout << "find Map" << std::endl;
                }

                if (!strcmp(signature, "Ljava/lang/ClassLoader;")) {
                    java_lang_ClassLoader = loadedClasses[i];
                    std::cout << "find java/lang/ClassLoader" << std::endl;
                }

            }

            //寻找方法
            getAllStackTraces = env->GetStaticMethodID(thread, "getAllStackTraces", "()Ljava/util/Map;");
            getThreadName = env->GetMethodID(thread, "getName", "()Ljava/lang/String;");
            getContextClassLoader = env->GetMethodID(thread, "getContextClassLoader", "()Ljava/lang/ClassLoader;");

            KeySet = env->GetMethodID(map, "keySet", "()Ljava/util/Set;");
            toArray = env->GetMethodID(set, "toArray", "()[Ljava/lang/Object;");
            defineClass = env->GetMethodID(java_lang_ClassLoader, "defineClass", "([BII)Ljava/lang/Class;");

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
                    std::cout << "find Client Thread" << std::endl;
                    break;
                }
            }

            //拿到类加载器
            CtxClassLoader = env->CallObjectMethod(clientThread, getContextClassLoader);
            

            
            java_lang_ClassLoader = env->FindClass("java/lang/ClassLoader");
            if (!CtxClassLoader) {
                MessageBoxA(NULL, "Minecraft Not Found", randstr(name, 8), MB_OK | MB_ICONERROR);
                break;
            }
            jclass loadClass = NULL;
            jclass loaderClass = NULL;
            jsize tempClassIndex = 0;
            jsize lastClassIndex = 0;
            for (jsize j = 0; j != classCount; j++) {
                char* lastClass = new char[classSizes[j] + 1];
                for (jsize classIndex = 0; classIndex != classSizes[j]; classIndex++) {
                    tempClassIndex++;;
                    lastClass[classIndex] = classes[lastClassIndex + classIndex];
                }
                loadClass = env->DefineClass(NULL, CtxClassLoader, (jbyte*)lastClass, classSizes[j]);

                char* signature;
                jvmTiEnv->GetClassSignature(loadClass, &signature, NULL);
                printf("[Loader] Defining class %s +\n", signature);
                if (!strcmp(signature, "LLoader;"))
                {
                    loaderClass = loadClass;
                }
                if (!loadClass) {

                }
                delete[]lastClass;
                lastClassIndex = tempClassIndex;
            }

            if (!loaderClass) {
                printf("[Loader] Loader class not found");
                break;
            }
            jmethodID loaderid = NULL;
            loaderid = env->GetMethodID(loaderClass, "<init>", "()V");

            if (!loaderid) {
                printf("[Loader] Loader method not found");
            }

            jobject LoadClent = env->NewObject(loaderClass, loaderid);
            if (!LoadClent) {
                printf("[Loader] new Load Client Error");
            }
        }
    }
    //FreeConsole();
    //ExitThread(0);
    
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
        //  MessageBoxA(NULL, "ATTACHED", "NHC", MB_OK | MB_ICONINFORMATION);
        AllocConsole();
        HANDLE hdlWrite = GetStdHandle(STD_OUTPUT_HANDLE);
        FILE* stream;
        freopen_s(&stream, "CONOUT$", "w+t", stdout);
        freopen_s(&stream, "CONIN$", "r+t", stdin);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)hook, NULL, 0, NULL);
        break;
    }

    return TRUE;
}
