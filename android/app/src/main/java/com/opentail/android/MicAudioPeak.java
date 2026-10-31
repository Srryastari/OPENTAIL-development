package com.opentail.android;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import java.util.concurrent.atomic.AtomicInteger;

/** Foreground/local Rocktaves only. PCM stays in memory and is never stored,
 * uploaded or sent over BLE. Acquisition and FFT never run on the UI thread.
 */
final class MicAudioPeak {
    private final AtomicInteger generation=new AtomicInteger();
    private volatile float[] latest;
    float[] latest(){return latest;}
    @SuppressWarnings("MissingPermission")
    void start(){
        final int lease=generation.incrementAndGet();latest=null;
        Thread worker=new Thread(()->{
            AudioRecord record=null;
            try{
                int minimum=AudioRecord.getMinBufferSize(MicSpectrum.SAMPLE_RATE,
                    AudioFormat.CHANNEL_IN_MONO,AudioFormat.ENCODING_PCM_16BIT);
                if(minimum<=0)return;
                record=new AudioRecord(MediaRecorder.AudioSource.MIC,MicSpectrum.SAMPLE_RATE,
                    AudioFormat.CHANNEL_IN_MONO,AudioFormat.ENCODING_PCM_16BIT,
                    Math.max(minimum,MicSpectrum.SIZE*4));
                if(record.getState()!=AudioRecord.STATE_INITIALIZED)return;
                MicSpectrum spectrum=new MicSpectrum();short[] pcm=new short[MicSpectrum.SIZE];
                record.startRecording();
                while(generation.get()==lease){
                    int offset=0;
                    while(offset<pcm.length && generation.get()==lease){
                        int read=record.read(pcm,offset,pcm.length-offset,AudioRecord.READ_BLOCKING);
                        if(read<=0)return;
                        offset+=read;
                    }
                    if(generation.get()==lease)latest=spectrum.analyze(pcm);
                }
            }catch(SecurityException|IllegalArgumentException|IllegalStateException ignored){
                // No permission/device/recording: no invented audio frames.
            }finally{
                if(record!=null){try{record.stop();}catch(IllegalStateException ignored){}
                    record.release();}
                if(generation.get()==lease)latest=null;
            }
        },"opentail-local-mic");
        worker.setDaemon(true);worker.start();
    }
    // Do not join a blocking microphone read on Android's command/UI thread.
    void stop(){generation.incrementAndGet();latest=null;}
}
