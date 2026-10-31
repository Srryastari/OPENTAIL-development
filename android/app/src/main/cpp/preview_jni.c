#include <jni.h>
#include <stdint.h>
#include <stdlib.h>
#include "preview.h"
#include "core/wled_fx_compat.h"

/* Read-only fixed palette source lookup; no effect state or BLE calls.
 * Dynamic/custom palette IDs require their actual resolved frame snapshot. */
JNIEXPORT jintArray JNICALL Java_com_opentail_android_OfflineCoreRenderer_fixedPalette(
    JNIEnv *env,jclass cls,jint id,jint foreground,jint background,jint accent) {
    (void)cls;
    if(id<0||id>72||id==1)return NULL;
    const ot_wled_rgb_t fg={(uint8_t)(foreground>>16),
        (uint8_t)(foreground>>8),(uint8_t)foreground};
    const ot_wled_rgb_t bg={(uint8_t)(background>>16),
        (uint8_t)(background>>8),(uint8_t)background};
    const ot_wled_rgb_t third={(uint8_t)(accent>>16),
        (uint8_t)(accent>>8),(uint8_t)accent};
    ot_wled_palette16_t pal={0};
    if(!ot_wled_palette_build((uint8_t)id,fg,bg,third,&pal))return NULL;
    jint values[16];
    for(int i=0;i<16;i++)
        values[i]=(jint)(0xff000000u|((uint32_t)pal.entries[i].r<<16)|
            ((uint32_t)pal.entries[i].g<<8)|pal.entries[i].b);
    jintArray result=(*env)->NewIntArray(env,16);
    if(!result)return NULL;
    (*env)->SetIntArrayRegion(env,result,0,16,values);
    if((*env)->ExceptionCheck(env))return NULL;
    return result;
}
// Handle and buffers belong to one Java instance, accessed under its monitor.
typedef struct {ot_preview *preview;jsize count;uint32_t colors[];} binding;
JNIEXPORT jlong JNICALL Java_com_opentail_android_OfflineCoreRenderer_create(
    JNIEnv *env,jclass cls,jint effect,jint count) {
    (void)env;(void)cls;
    if(effect<0||effect>65535||count<1||count>65535)return 0;
    binding *b=calloc(1,sizeof(*b)+(size_t)count*sizeof(uint32_t));
    if(!b)return 0;
    b->preview=ot_preview_create((uint16_t)effect,(size_t)count);b->count=count;
    if(!b->preview){free(b);return 0;}
    return (jlong)(intptr_t)b;
}
JNIEXPORT void JNICALL Java_com_opentail_android_OfflineCoreRenderer_destroy(
    JNIEnv *env,jclass cls,jlong handle) {
    (void)env;(void)cls;binding *b=(binding *)(intptr_t)handle;
    if(b){ot_preview_free(b->preview);free(b);}
}
JNIEXPORT jboolean JNICALL Java_com_opentail_android_OfflineCoreRenderer_frame(
    JNIEnv *env,jclass cls,jlong handle,jlong ms,jint frame_ms,jint speed,
    jint intensity,jint brightness,jint palette,jint fg,jint bg,
    jbyteArray custom,jfloat hz,jfloat magnitude,jboolean audio,jintArray out) {
    (void)cls;binding *b=(binding *)(intptr_t)handle;
    if(!b||!out||ms<0||frame_ms<1||frame_ms>65535||
       speed<0||speed>255||intensity<0||intensity>255||brightness<0||brightness>255||
       palette<0||palette>255||(*env)->GetArrayLength(env,out)!=b->count)return JNI_FALSE;
    uint8_t stops[18*4];jsize bytes=custom?(*env)->GetArrayLength(env,custom):0;
    if(bytes && (bytes<8||bytes>(jsize)sizeof(stops)||bytes%4))return JNI_FALSE;
    if(bytes)(*env)->GetByteArrayRegion(env,custom,0,bytes,(jbyte *)stops);
    if((*env)->ExceptionCheck(env))return JNI_FALSE;
    ot_wled_audio_peak_t peak={hz,magnitude};
    if(!ot_preview_render(b->preview,(uint32_t)ms,(uint16_t)frame_ms,
        (uint8_t)speed,(uint8_t)intensity,(uint8_t)brightness,(uint8_t)palette,
        (uint32_t)fg,(uint32_t)bg,bytes?stops:NULL,(size_t)bytes/4,
        audio?&peak:NULL,b->colors))return JNI_FALSE;
    (*env)->SetIntArrayRegion(env,out,0,b->count,(jint *)b->colors);
    return (*env)->ExceptionCheck(env)?JNI_FALSE:JNI_TRUE;
}
