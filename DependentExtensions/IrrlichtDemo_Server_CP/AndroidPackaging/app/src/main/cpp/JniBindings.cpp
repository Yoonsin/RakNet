#ifdef __ANDROID__
#include <jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_irrlichtdemoCP_MainActivity_stringFromJNI(
	JNIEnv* env,
	jobject thiz) {
	return env->NewStringUTF("Hello~");
}


#else 

#endif // 


