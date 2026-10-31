package com.opentail.android;

/** Measured microphone spectrum, not simulated effect/audio telemetry.
 * Hann-windowed radix-2 FFT with interpolated strongest non-DC bin.
 * Magnitude is peak PCM16 amplitude; device microphone gain is not CORE gain.
 */
final class MicSpectrum {
    static final int SIZE=1024,SAMPLE_RATE=22050;
    private final double[] real=new double[SIZE],imag=new double[SIZE];
    float[] analyze(short[] pcm) {
        double mean=0;for(short v:pcm)mean+=v;mean/=SIZE;
        for(int i=0;i<SIZE;i++){
            real[i]=(pcm[i]-mean)*(.5-.5*Math.cos(2*Math.PI*i/(SIZE-1)));
            imag[i]=0;
        }
        for(int i=1,j=0;i<SIZE;i++){
            int bit=SIZE>>1;for(; (j&bit)!=0;bit>>=1)j^=bit;j^=bit;
            if(i<j){double t=real[i];real[i]=real[j];real[j]=t;}
        }
        for(int length=2;length<=SIZE;length<<=1){
            double angle=-2*Math.PI/length,wr=Math.cos(angle),wi=Math.sin(angle);
            for(int start=0;start<SIZE;start+=length){
                double ur=1,ui=0;
                for(int j=0;j<length/2;j++){
                    int a=start+j,b=a+length/2;
                    double vr=real[b]*ur-imag[b]*ui,vi=real[b]*ui+imag[b]*ur;
                    real[b]=real[a]-vr;imag[b]=imag[a]-vi;real[a]+=vr;imag[a]+=vi;
                    double next=ur*wr-ui*wi;ui=ur*wi+ui*wr;ur=next;
                }
            }
        }
        int peak=1;
        for(int i=0;i<=SIZE/2;i++){
            real[i]=Math.hypot(real[i],imag[i]);
            if(i>0 && i<SIZE/2 && real[i]>real[peak])peak=i;
        }
        double a=real[peak-1],b=real[peak],c=real[peak+1];
        double divisor=a-2*b+c;
        double offset=divisor==0?0:.5*(a-c)/divisor;
        offset=Math.max(-.5,Math.min(.5,offset));
        return new float[]{(float)((peak+offset)*SAMPLE_RATE/SIZE),
            (float)(b*4/(SIZE-1))};
    }
}
