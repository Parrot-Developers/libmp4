/**
 * @file libmp4_jni.c
 * @brief JNI wrapper for retrieving MP4 metadata in Java
 */

#include <jni.h>
#include <android/log.h>
#include <string.h>
#include <libmp4.h>

#define LOG_TAG "libmp4_jni"

/** Log as verbose */
#define LOGV(_fmt, ...) \
	__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, _fmt, ##__VA_ARGS__)

/** Log as debug */
#define LOGD(_fmt, ...) \
	__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, _fmt, ##__VA_ARGS__)

/** Log as info */
#define LOGI(_fmt, ...) \
	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, _fmt, ##__VA_ARGS__)

/** Log as warning */
#define LOGW(_fmt, ...) \
	__android_log_print(ANDROID_LOG_WARN, LOG_TAG, _fmt, ##__VA_ARGS__)

/** Log as error */
#define LOGE(_fmt, ...) \
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, _fmt, ##__VA_ARGS__)

static struct {
   jclass hashMapClass;
   jmethodID hashMapConstructor;
   jmethodID hashMapPutMethod;
} globalIds;

JNIEXPORT jint JNICALL JNI_OnLoad(
	JavaVM *jvm,
	void *reserved)
{
	JNIEnv* env;

	if ((*(jvm))->GetEnv(jvm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		return -1;
	}
	globalIds.hashMapClass = (*env)->FindClass(env, "java/util/HashMap");
	if (globalIds.hashMapClass == NULL) {
		LOGE("could not retrieve class HashMap");
		return -1;
	}
	globalIds.hashMapClass =
		(jclass)(*env)->NewGlobalRef(env, globalIds.hashMapClass);
	if (globalIds.hashMapClass == NULL) {
		LOGE("could not create global ref for HashMap class");
		return -1;
	}
	globalIds.hashMapConstructor = (*env)->GetMethodID(env,
		globalIds.hashMapClass, "<init>", "()V");
	if (globalIds.hashMapConstructor == NULL) {
		LOGE("could not find HashMap constructor");
		return -1;
	}
	globalIds.hashMapPutMethod = (*env)->GetMethodID(env,
		globalIds.hashMapClass, "put",
		"(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
	if (globalIds.hashMapPutMethod == NULL) {
		LOGE("could not find HashMap.put method");
		return -1;
	}

	return JNI_VERSION_1_6;
}

JNIEXPORT jlong JNICALL
Java_com_parrot_libmp4_Libmp4_nativeOpen(
	JNIEnv *env,
	jobject thizz,
	jstring fileName)
{
	const char *name = (*env)->GetStringUTFChars(env, fileName, NULL);

	if (name == NULL) {
		return 0;
	}
	jlong ret = (jlong)mp4_demux_open(name);
	(*env)->ReleaseStringUTFChars(env, fileName, name);
	return ret;
}

JNIEXPORT jint JNICALL
Java_com_parrot_libmp4_Libmp4_nativeClose(
	JNIEnv *env,
	jobject thizz,
	jlong demux)
{
	return mp4_demux_close((struct mp4_demux*)demux);
}

JNIEXPORT jobject JNICALL
Java_com_parrot_libmp4_Libmp4_nativeGetMetadata(
	JNIEnv *env,
	jobject thizz,
	jlong demux)
{
	unsigned int count = 0;
	char **keys = NULL;
	char **values = NULL;

	int ret = mp4_demux_get_metadata_strings((struct mp4_demux*)demux, &count,
		&keys, &values);
	if ((ret == 0) && (count > 0)) {
		LOGV("Session metadata:");
		jobject map = (*env)->NewObject(env, globalIds.hashMapClass,
			globalIds.hashMapConstructor);
		if (map == NULL) {
			LOGE("NewObject failed");
			return NULL;
		}
		unsigned int i;
		for (i = 0; i < count; i++) {
			if ((keys[i]) && (values[i])) {
				LOGV("  %s: %s", keys[i], values[i]);
				int len = strlen(keys[i]);
				jbyteArray map_key = (*env)->NewByteArray(env, len);
				if (map_key == NULL) {
					LOGE("NewByteArray failed");
					return NULL;
				}
				(*env)->SetByteArrayRegion(env, map_key, 0, len,
					(const jbyte*)keys[i]);
				len = strlen(values[i]);
				jbyteArray map_val = (*env)->NewByteArray(env, len);
				if (map_val == NULL) {
					LOGE("NewByteArray failed");
					return NULL;
				}
				(*env)->SetByteArrayRegion(env, map_val, 0, len,
					(const jbyte*)values[i]);
				(*env)->CallObjectMethod(env, map, globalIds.hashMapPutMethod,
					map_key, map_val);
				if ((*env)->ExceptionOccurred(env)) {
					return NULL;
				}
			}
		}
		return map;
	} else {
		return NULL;
	}
}

