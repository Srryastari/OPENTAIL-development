package com.opentail.android;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.View;

/**
 * Native 1D pixel-address inspector for the Java fallback launcher.
 * Exactly one drawn segment per configured LED index. Not an effect renderer.
 * CORE BLE does not supply frames, so never invent effect motion.
 */
final class NativeLedPreview extends View {
    private static final int PAGE = 64;
    private final Paint paint=new Paint();
    private final NativeProject project;
    private ExactLedFrame frame=ExactLedFrame.fromProject(new int[0]);
    private boolean light=false;
    private int first=0;

    NativeLedPreview(Context context,NativeProject project){
        super(context);
        this.project=project;
        paint.setAntiAlias(false);
    }

    void configure(int effectId,int fg,int bg,boolean lightTheme){
        light=lightTheme;
        try {
            frame=ExactLedFrame.fromProjectModel(project);
        } catch(IllegalArgumentException e) {
            frame=ExactLedFrame.fromProject(new int[0]);
            setContentDescription("Pixel map unavailable: invalid or oversized layout");
        }
        first=Math.min(first,Math.max(0,frame.size()-PAGE));
        setContentDescription("Exact local LED index map, "+frame.size()+
                " pixels. Not an effect animation.");
        invalidate();
    }

    void shift(int direction){
        int limit=Math.max(0,frame.size()-PAGE);
        first=Math.max(0,Math.min(limit,first+direction*PAGE));
        invalidate();
    }

    @Override protected void onMeasure(int w,int h){
        setMeasuredDimension(MeasureSpec.getSize(w),
                Math.round(96*getResources().getDisplayMetrics().density));
    }

    @Override protected void onDraw(Canvas canvas){
        super.onDraw(canvas);
        final float width=getWidth(),height=getHeight();
        paint.setColor(light?0xffe7e7e7:0xff0b0b0b);
        canvas.drawRoundRect(0,0,width,height,18,18,paint);
        int visible=Math.min(PAGE,Math.max(0,frame.size()-first));
        if(visible==0)return;
        float pitch=width/visible;
        for(int i=0;i<visible;i++){
            // Do not downsample, interpolate, fake a color or wrap zone edges.
            paint.setColor(frame.colorAt(first+i));
            float left=i*pitch;
            canvas.drawRect(left,height*.2f,
                    Math.min(width,left+Math.max(.5f,pitch-1f)),
                    height*.8f,paint);
        }
    }
}
