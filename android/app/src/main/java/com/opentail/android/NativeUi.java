package com.opentail.android;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

/**
 * Shared native Android design tokens. Nothing in this file uses a WebView.
 * Match the approved Studio: true OLED black, monochrome elevated cards,
 * subtle glowing capsule controls, pill fields, and an invertible white theme.
 */
final class NativeUi {
    final Activity activity;
    final boolean light;
    final int background, surface, raised, text, secondary, accent, danger, divider;
    NativeUi(Activity activity, boolean light) {
        this.activity=activity;this.light=light;
        background=light?0xffffffff:0xff000000;
        surface=light?0xfff5f5f5:0xff101010;
        raised=light?0xffebebeb:0xff1c1c1c;
        text=light?0xff181818:0xfff5f5f5;
        secondary=light?0xff606060:0xffa7a7a7;
        accent=light?0xff181818:0xfff5f5f5;
        danger=light?0xffbd3842:0xffed6472;
        divider=light?0xffdedede:0xff252525;
    }
    int dp(float x) {return Math.round(x*activity.getResources().getDisplayMetrics().density);}
    GradientDrawable shape(int color,float radius) {
        GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;
    }
    GradientDrawable edge(int color,float radius,int stroke) {
        GradientDrawable d=shape(color,radius);d.setStroke(dp(1),stroke);return d;
    }
    LinearLayout vertical() {LinearLayout l=new LinearLayout(activity);l.setOrientation(LinearLayout.VERTICAL);return l;}
    LinearLayout horizontal() {LinearLayout l=new LinearLayout(activity);l.setOrientation(LinearLayout.HORIZONTAL);l.setGravity(Gravity.CENTER_VERTICAL);return l;}
    LinearLayout card() {
        LinearLayout l=vertical();l.setPadding(dp(17),dp(17),dp(17),dp(17));l.setBackground(shape(surface,21));return l;
    }
    TextView text(String value,int sp,boolean bold) {
        TextView v=new TextView(activity);v.setText(value);v.setTextColor(text);v.setTextSize(sp);
        if(bold)v.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        v.setGravity(Gravity.CENTER_VERTICAL);return v;
    }
    TextView caption(String value) {
        TextView v=text(value,12,false);v.setTextColor(secondary);v.setLineSpacing(dp(2),1f);return v;
    }
    TextView heading(String value) {return text(value,21,true);}
    TextView button(String value,boolean primary) {
        TextView b=text(value,13,true);b.setGravity(Gravity.CENTER);
        b.setMinHeight(dp(43));b.setPadding(dp(16),dp(9),dp(16),dp(9));
        b.setTextColor(primary?background:text);
        b.setBackground(shape(primary?accent:raised,999));b.setClickable(true);b.setFocusable(true);
        return b;
    }
    TextView chip(String value,boolean selected) {
        TextView t=button(value,false);
        t.setBackground(shape(selected?accent:surface,999));
        t.setTextColor(selected?background:secondary);
        t.setPadding(dp(13),dp(8),dp(13),dp(8));
        return t;
    }
    EditText field(String value,String hint) {
        EditText f=new EditText(activity);f.setSingleLine(true);f.setText(value);f.setHint(hint);
        f.setTextColor(text);f.setHintTextColor(secondary);
        f.setTextSize(14);f.setPadding(dp(16),dp(11),dp(16),dp(11));
        f.setBackground(shape(raised,999));f.setSelectAllOnFocus(false);return f;
    }
    SeekBar slider(int max,int progress) {
        SeekBar bar=new SeekBar(activity);bar.setMax(max);
        bar.setProgress(Math.max(0,Math.min(max,progress)));
        bar.setProgressTintList(ColorStateList.valueOf(accent));
        bar.setProgressBackgroundTintList(ColorStateList.valueOf(raised));
        bar.setThumbTintList(ColorStateList.valueOf(accent));
        bar.setPadding(dp(4),dp(9),dp(4),dp(9));
        bar.setMinimumHeight(dp(34));
        // Native Android progress drawable with softly rounded capsule track.
        GradientDrawable base=shape(raised,999),active=shape(accent,999);
        LayerDrawable track=new LayerDrawable(new android.graphics.drawable.Drawable[]{
            base,new ClipDrawable(active,Gravity.LEFT,ClipDrawable.HORIZONTAL)});
        track.setId(0,android.R.id.background);
        track.setId(1,android.R.id.progress);
        bar.setProgressDrawable(track);
        return bar;
    }
    LinearLayout.LayoutParams lp(int top,int bottom) {
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,ViewGroup.LayoutParams.WRAP_CONTENT);
        p.topMargin=dp(top);p.bottomMargin=dp(bottom);return p;
    }
    LinearLayout.LayoutParams fillWeight() {return new LinearLayout.LayoutParams(0,ViewGroup.LayoutParams.WRAP_CONTENT,1);}
    void gap(LinearLayout l,int dp) {
        View v=new View(activity);l.addView(v,new LinearLayout.LayoutParams(1,this.dp(dp)));
    }
    void divider(LinearLayout l) {
        View v=new View(activity);v.setBackgroundColor(divider);
        l.addView(v,new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,dp(1)));
    }
}
