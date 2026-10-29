package com.opentail.android;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.View;

/**
 * Canvas-only OFFLINE design illustration. Not a copy of firmware's renderer
 * and never wired to real LED outputs or BLE commands.
 */
final class NativeLedPreview extends View {
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final int count=32;
    private int effectId=0;
    private int foreground=0xfff7f7f7,background=0xff000000;
    private boolean playing=false;
    private long started=0;
    private boolean light=false;
    NativeLedPreview(Context context){super(context);setLayerType(View.LAYER_TYPE_SOFTWARE,null);}
    void configure(int id,int fg,int bg,boolean lightTheme){
        effectId=id;foreground=fg;background=bg;light=lightTheme;invalidate();
    }
    void setPlaying(boolean active) {
        playing=active;
        started=android.os.SystemClock.uptimeMillis();
        invalidate();
    }
    boolean isPlaying(){return playing;}
    @Override protected void onMeasure(int w,int h) {
        setMeasuredDimension(MeasureSpec.getSize(w),Math.round(96*getResources().getDisplayMetrics().density));
    }
    private static int lerp(int a,int b,float f) {
        int r=(int)(Color.red(a)*(1f-f)+Color.red(b)*f);
        int g=(int)(Color.green(a)*(1f-f)+Color.green(b)*f);
        int bb=(int)(Color.blue(a)*(1f-f)+Color.blue(b)*f);
        return Color.rgb(r,g,bb);
    }
    private static int rainbow(float v) {
        return Color.HSVToColor(new float[]{(v*360f)%360f,1f,1f});
    }
    @Override protected void onDraw(Canvas canvas){
        super.onDraw(canvas);
        float w=getWidth(),h=getHeight();
        paint.setColor(light?0xffe7e7e7:0xff0b0b0b);
        canvas.drawRoundRect(0,0,w,h,18,18,paint);
        float gap=Math.max(4f,w/(count/2+3f));
        float radius=Math.max(3f,Math.min(8f,(w-42f)/(count/2*3f)));
        float first=22f,step=(w-44f)/16f;
        float t=playing?(android.os.SystemClock.uptimeMillis()-started)/1000f:0f;
        for(int i=0;i<count;i++){
            int row=i/16,col=i%16;
            float x=first+(col+.5f)*step,y=h*(row==0?.32f:.68f);
            float index=(float)i/count;
            int color=foreground;
            String label=NativeEffectCatalog.byId(effectId).name.toLowerCase(java.util.Locale.ROOT);
            if(label.contains("rainbow")||label.contains("plasma"))color=rainbow(index+t*.12f);
            else if(label.contains("wipe")||label.contains("sweep")||label.contains("materialize"))
                color=index<((t*.3f)%1f)?foreground:background;
            else if(label.contains("chase")||label.contains("running"))
                color=((i+(int)(t*9))%8<3)?foreground:background;
            else if(label.contains("blink")||label.contains("strobe"))
                color=((int)(t*4)%2==0)?foreground:background;
            else if(label.contains("fade")||label.contains("breathe"))
                color=lerp(background,foreground,(float)(.5+.5*Math.sin(t*2)));
            else color=foreground;
            if(!playing)color=lerp(background,color,.65f);
            paint.setColor(color);
            paint.setShadowLayer(playing?Math.max(3f,radius*.9f):0,0,0,color);
            canvas.drawCircle(x,y,radius,paint);
            paint.clearShadowLayer();
        }
        if(playing)postInvalidateDelayed(33);
    }
}
