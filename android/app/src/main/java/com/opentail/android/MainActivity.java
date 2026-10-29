package com.opentail.android;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Random;
import java.util.Set;

/**
 * Genuine native OpenTail Studio Android. All six screens, dialogs, input
 * widgets, color editors, animation previews and navigation are Android Views.
 * No WebView, JavaScript bridge, HTML, dynamic web asset or firmware /studio.
 *
 * The Android and web apps can evolve independently while sharing a validated
 * OpenTail v4 JSON authoring project. Firmware /dev owns hardware diagnostics.
 * BLE is an EXPLICIT, authenticated controller action in Settings only.
 */
public final class MainActivity extends Activity {
    private static final int IMPORT_PROJECT=201, EXPORT_PROJECT=202;
    private static final String[] TABS={
        "Home","Show Modes","Layout","Triggers","Auto Events","Settings"
    };
    private static final String[] DEFAULT_EVENTS={"Startup","Shutdown"};
    private final Random random=new Random();
    private final Set<String> selectedTriggers=new LinkedHashSet<>();
    private final Set<Integer> supportedBleEffects=new HashSet<>();

    private NativeProject project;
    private SharedPreferences settings;
    private HapticController haptics;
    private NativeUi ui;
    private BleDiscovery ble;
    private NativeOutputManager outputManager;
    private String bleMessage="Not connected";
    private int tab=0,showTab=0,autoIndex=0;
    private int effectId=0, foreground=0xffffffff, background=0xff000000,accentColor=0xffffffff;
    private int speed=128,intensity=128,brightness=160;
    private boolean whiteMode=false,previewPlaying=false;
    private LinearLayout root,body,navContainer;
    private TextView bleStatus;
    private NativeLedPreview preview;
    private boolean receiverRegistered=false;

    private final BroadcastReceiver bluetoothState=new BroadcastReceiver(){
        @Override public void onReceive(Context context,Intent intent) {
            if(!BluetoothAdapter.ACTION_STATE_CHANGED.equals(intent.getAction()))return;
            int s=intent.getIntExtra(BluetoothAdapter.EXTRA_STATE,BluetoothAdapter.ERROR);
            if(ble!=null) {
                if(s==BluetoothAdapter.STATE_OFF || s==BluetoothAdapter.STATE_TURNING_OFF)
                    ble.onAdapterDisabled();
                else if(s==BluetoothAdapter.STATE_ON)ble.onAdapterEnabled();
            }
        }
    };

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        project=new NativeProject(this);
        settings=getSharedPreferences("opentail_native_ui_v1",MODE_PRIVATE);
        haptics=new HapticController(this);
        whiteMode=settings.getBoolean("whiteMode",false);
        effectId=settings.getInt("effectId",0);
        foreground=settings.getInt("foreground",0xffffffff);
        background=settings.getInt("background",0xff000000);
        accentColor=settings.getInt("accent",0xffffffff);
        speed=settings.getInt("speed",128);
        intensity=settings.getInt("intensity",128);
        brightness=settings.getInt("brightness",160);
        if(saved!=null){
            tab=Math.min(5,Math.max(0,saved.getInt("tab",0)));
            showTab=Math.min(2,Math.max(0,saved.getInt("showTab",0)));
        }
        ble=new BleDiscovery(this,message->{
            bleMessage=message;
            if(tab==5&&bleStatus!=null)bleStatus.setText("Bluetooth · "+message);
        });
        ble.setFxCapabilitiesListener(json->{
            supportedBleEffects.clear();
            try{JSONArray ids=new JSONArray(json);for(int i=0;i<ids.length();i++)
                supportedBleEffects.add(ids.getInt(i));}
            catch(JSONException ignored){}
            if(tab==5&&bleStatus!=null)bleStatus.setText(
                "Bluetooth · "+bleMessage+" · "+supportedBleEffects.size()+" verified effects");
        });
        ble.setPreferredListener(address->settings.edit().putString("preferred_ble_address",address).apply());
        ble.setConfigListener(result->{
            if(outputManager!=null)outputManager.onBleResult(result);
        });
        IntentFilter filter=new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
        if(Build.VERSION.SDK_INT>=33)registerReceiver(bluetoothState,filter,Context.RECEIVER_EXPORTED);
        else registerReceiver(bluetoothState,filter);
        receiverRegistered=true;
        render();
    }

    @Override protected void onSaveInstanceState(Bundle b){
        b.putInt("tab",tab);b.putInt("showTab",showTab);super.onSaveInstanceState(b);
    }
    @Override protected void onStop(){
        super.onStop();
        previewPlaying=false;
        if(preview!=null)preview.invalidate();
        if(ble!=null)ble.stopScan();
    }
    @Override protected void onDestroy(){
        if(receiverRegistered)unregisterReceiver(bluetoothState);
        if(ble!=null)ble.shutdown();
        super.onDestroy();
    }
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] grants){
        super.onRequestPermissionsResult(request,permissions,grants);
        if(ble!=null)ble.handlePermissions(request,grants);
    }

    private void tap(View v){haptics.tap(v,"tap");}
    private void toast(String text){Toast.makeText(this,text,Toast.LENGTH_SHORT).show();}
    private void saveDraft() {
        settings.edit().putInt("effectId",effectId)
                .putInt("foreground",foreground).putInt("background",background)
                .putInt("accent",accentColor).putInt("speed",speed)
                .putInt("intensity",intensity).putInt("brightness",brightness).apply();
    }
    private void setTab(int next){
        if(next==tab)return;
        previewPlaying=false;
        if(preview!=null)preview.invalidate();
        tab=next;render();
    }
    private void render() {
        ui=new NativeUi(this,whiteMode);
        getWindow().setStatusBarColor(ui.background);
        getWindow().setNavigationBarColor(ui.background);
        getWindow().getDecorView().setSystemUiVisibility(whiteMode?
                View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR|View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR:0);
        root=ui.vertical();root.setBackgroundColor(ui.background);
        setContentView(root);
        buildHeader();
        buildNavigation();
        ScrollView scroll=new ScrollView(this);
        scroll.setFillViewport(true);scroll.setClipToPadding(false);
        body=ui.vertical();body.setPadding(ui.dp(16),ui.dp(10),ui.dp(16),ui.dp(34));
        scroll.addView(body);
        root.addView(scroll,new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,0,1f));
        bleStatus=null;
        switch(tab){
            case 0:home();break;
            case 1:showModes();break;
            case 2:layout();break;
            case 3:triggers();break;
            case 4:autoEvents();break;
            default:settings();break;
        }
    }
    private void buildHeader(){
        LinearLayout header=ui.horizontal();
        header.setPadding(ui.dp(17),ui.dp(19),ui.dp(17),ui.dp(10));
        TextView brand=ui.text("✦  OpenTail",23,true);
        header.addView(brand,new LinearLayout.LayoutParams(0,ui.dp(44),1));
        TextView mode=ui.chip("DESIGN",true);
        header.addView(mode);
        TextView theme=ui.chip(whiteMode?"☀":"☾",false);
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,ViewGroup.LayoutParams.WRAP_CONTENT);
        p.leftMargin=ui.dp(9);header.addView(theme,p);
        theme.setContentDescription("Toggle white mode");
        theme.setOnClickListener(v->{tap(v);setWhiteMode(!whiteMode);});
        root.addView(header);
        TextView subtitle=ui.caption("Automotive lighting studio  ·  Native Android");
        subtitle.setPadding(ui.dp(18),0,ui.dp(18),ui.dp(6));root.addView(subtitle);
    }
    private void buildNavigation(){
        HorizontalScrollView scroller=new HorizontalScrollView(this);
        scroller.setHorizontalScrollBarEnabled(false);
        scroller.setFillViewport(false);
        navContainer=ui.horizontal();
        navContainer.setPadding(ui.dp(14),ui.dp(5),ui.dp(14),ui.dp(11));
        for(int i=0;i<TABS.length;i++){
            final int index=i;
            String[] icons={"⌂","✧","▦","ϟ","☼","⚙"};
            TextView b=ui.chip(icons[i]+"  "+TABS[i],i==tab);
            LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,ui.dp(43));
            lp.rightMargin=ui.dp(7);navContainer.addView(b,lp);
            b.setOnClickListener(v->{tap(v);setTab(index);});
        }
        scroller.addView(navContainer);root.addView(scroller);
    }
    private void pageTitle(String title,String subtitle){
        TextView h=ui.heading(title);body.addView(h,ui.lp(7,3));
        body.addView(ui.caption(subtitle),ui.lp(0,12));
    }
    private void addCard(LinearLayout card){
        body.addView(card,ui.lp(7,9));
    }
    private void rowButtons(LinearLayout card,String left,Runnable leftAction,
                            String right,Runnable rightAction){
        LinearLayout row=ui.horizontal();
        TextView one=ui.button(left,true),two=ui.button(right,false);
        LinearLayout.LayoutParams leftLp=ui.fillWeight();leftLp.rightMargin=ui.dp(9);
        row.addView(one,leftLp);row.addView(two,ui.fillWeight());
        one.setOnClickListener(v->{tap(v);leftAction.run();});
        two.setOnClickListener(v->{tap(v);rightAction.run();});
        card.addView(row,ui.lp(10,0));
    }
    private void action(LinearLayout parent,String title,boolean primary,Runnable runnable){
        TextView button=ui.button(title,primary);
        parent.addView(button,ui.lp(9,0));
        button.setOnClickListener(v->{tap(v);runnable.run();});
    }
    private void secondary(LinearLayout parent,String label){
        parent.addView(ui.caption(label),ui.lp(7,4));
    }
    private void setWhiteMode(boolean on){
        whiteMode=on;
        settings.edit().putBoolean("whiteMode",on).apply();
        render();
    }
    private void home(){
        pageTitle("Home","Your lighting design, at a glance.");
        LinearLayout c=ui.card();
        c.addView(ui.heading(project.name()));
        secondary(c,"Local project · No controller connection required");
        LinearLayout stats=ui.horizontal();
        TextView zones=ui.text(project.zoneCount()+" zones",15,true);
        TextView pixels=ui.text(project.pixelCount()+" pixels",15,true);
        TextView steps=ui.text(project.sequence().length()+" steps",15,true);
        stats.addView(zones,ui.fillWeight());
        stats.addView(pixels,ui.fillWeight());
        stats.addView(steps,ui.fillWeight());
        c.addView(stats,ui.lp(16,12));
        rowButtons(c,"Show Modes",()->setTab(1),"Edit Layout",()->setTab(2));
        addCard(c);
        previewCard(false);
        LinearLayout info=ui.card();info.addView(ui.text("What belongs where",16,true));
        secondary(info,"Studio designs effects, presets, triggers and sequences. CORE runs the actual LED renderer and owns hardware diagnostics. Local previews are illustrative.");
        addCard(info);
    }
    private void previewCard(boolean includeTransport){
        LinearLayout c=ui.card();
        LinearLayout title=ui.horizontal();
        TextView h=ui.text("Preview engine",16,true);
        title.addView(h,new LinearLayout.LayoutParams(0,ui.dp(30),1));
        title.addView(ui.chip("INDEXED",false));c.addView(title);
        preview=new NativeLedPreview(this,project);
        preview.configure(effectId,foreground,background,whiteMode);
        c.addView(preview,ui.lp(12,5));
        secondary(c,"Exact zero-based design pixel map (64 per page). Not a rendered FX frame; CORE BLE does not return RGB frames.");
        if(includeTransport)
            rowButtons(c,"Previous 64",()->preview.shift(-1),
                    "Next 64",()->preview.shift(1));
        addCard(c);
    }
    private void showModes(){
        pageTitle("Show Modes","Create effects, sequences and presets.");
        LinearLayout tabs=ui.horizontal();
        final String[] names={"Effects","Sequences","Presets"};
        for(int i=0;i<3;i++){
            final int at=i;
            TextView b=ui.chip(names[i],showTab==i);
            LinearLayout.LayoutParams l=ui.fillWeight();l.rightMargin=ui.dp(i<2?6:0);
            tabs.addView(b,l);
            b.setOnClickListener(v->{tap(v);showTab=at;previewPlaying=false;render();});
        }
        body.addView(tabs,ui.lp(1,9));
        previewCard(true);
        if(showTab==0)effectEditor();
        else if(showTab==1)sequenceEditor();
        else presetEditor();
    }
    private void effectEditor(){
        LinearLayout c=ui.card();
        c.addView(ui.text("Effects",17,true));
        secondary(c,"All 126 effects are metadata-only. Choose a design; the ESP32 renderer runs separately.");
        TextView selected=ui.text("Selected · "+NativeEffectCatalog.byId(effectId).name,15,true);
        c.addView(selected,ui.lp(9,6));
        action(c,"Browse effects",true,()->chooseEffect(effectId,id->{
            effectId=id;saveDraft();render();
        }));
        LinearLayout colors=ui.horizontal();
        colorTile(colors,"Foreground",foreground,color->{
            foreground=color;saveDraft();render();
        });
        colorTile(colors,"Background",background,color->{
            background=color;saveDraft();render();
        });
        colorTile(colors,"Accent",accentColor,color->{
            accentColor=color;saveDraft();render();
        });
        c.addView(colors,ui.lp(17,10));
        sliderRow(c,"Speed",speed,255,v->{speed=v;saveDraft();});
        sliderRow(c,"Intensity",intensity,255,v->{intensity=v;saveDraft();});
        sliderRow(c,"Brightness",brightness,255,v->{brightness=v;saveDraft();});
        secondary(c,"Design parameters are saved locally. Playback and live output settings are not sent automatically.");
        addCard(c);
    }
    private interface NumberAction{void change(int value);}
    private interface ColorAction{void select(int color);}
    private void sliderRow(LinearLayout c,String title,int value,int max,NumberAction callback){
        LinearLayout header=ui.horizontal();
        header.addView(ui.text(title,13,true),new LinearLayout.LayoutParams(0,ui.dp(26),1));
        TextView number=ui.caption(Integer.toString(value));header.addView(number);
        c.addView(header,ui.lp(10,0));
        SeekBar slider=ui.slider(max,value);
        c.addView(slider,new LinearLayout.LayoutParams(-1,ui.dp(43)));
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            @Override public void onProgressChanged(SeekBar sb,int progress,boolean fromUser){
                if(fromUser){number.setText(Integer.toString(progress));callback.change(progress);if(preview!=null)preview.invalidate();}
            }
            @Override public void onStartTrackingTouch(SeekBar sb){}
            @Override public void onStopTrackingTouch(SeekBar sb){haptics.tap(sb,"slider");}
        });
    }
    private void colorTile(LinearLayout parent,String name,int color,ColorAction onColor){
        LinearLayout col=ui.vertical();
        LinearLayout.LayoutParams lp=ui.fillWeight();lp.rightMargin=ui.dp(6);
        parent.addView(col,lp);
        View chip=new View(this);chip.setBackground(ui.edge(color,999,ui.divider));
        col.addView(chip,new LinearLayout.LayoutParams(-1,ui.dp(39)));
        TextView label=ui.caption(name);
        label.setGravity(Gravity.CENTER);col.addView(label,ui.lp(4,0));
        col.setOnClickListener(v->{tap(v);colorDialog(name,color,onColor);});
    }
    private void colorDialog(String title,int initial,ColorAction onColor){
        final int[] rgb={Color.red(initial),Color.green(initial),Color.blue(initial)};
        LinearLayout box=ui.vertical();box.setPadding(ui.dp(22),ui.dp(8),ui.dp(22),ui.dp(8));
        TextView sample=ui.text("",17,true);sample.setGravity(Gravity.CENTER);
        sample.setMinHeight(ui.dp(48));box.addView(sample);
        for(int i=0;i<3;i++){
            final int channel=i;String label=new String[]{"Red","Green","Blue"}[i];
            LinearLayout header=ui.horizontal();
            header.addView(ui.text(label,13,true),new LinearLayout.LayoutParams(0,ui.dp(27),1));
            TextView value=ui.caption(Integer.toString(rgb[i]));header.addView(value);
            box.addView(header);
            SeekBar bar=ui.slider(255,rgb[i]);box.addView(bar);
            bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
                @Override public void onProgressChanged(SeekBar sb,int p,boolean from){
                    if(!from)return;rgb[channel]=p;value.setText(Integer.toString(p));
                    sample.setBackground(ui.shape(Color.rgb(rgb[0],rgb[1],rgb[2]),999));
                }
                @Override public void onStartTrackingTouch(SeekBar b){}
                @Override public void onStopTrackingTouch(SeekBar b){}
            });
        }
        sample.setBackground(ui.shape(initial,999));
        new AlertDialog.Builder(this).setTitle(title+" color").setView(box)
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Save",(d,w)->onColor.select(Color.rgb(rgb[0],rgb[1],rgb[2]))).show();
    }
    private interface EffectChoice{void chosen(int effectId);}
    private void chooseEffect(int chosen,EffectChoice onChoose){
        LinearLayout container=ui.vertical();container.setPadding(ui.dp(13),ui.dp(5),ui.dp(13),ui.dp(5));
        EditText query=ui.field("","Search 126 effects…");
        container.addView(query,ui.lp(0,8));
        ScrollView scroll=new ScrollView(this);
        LinearLayout options=ui.vertical();scroll.addView(options);
        container.addView(scroll,new LinearLayout.LayoutParams(-1,ui.dp(390)));
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("Choose effect")
                .setView(container).setNegativeButton("Cancel",null).create();
        Runnable fill=()->{
            options.removeAllViews();
            String term=query.getText().toString().toLowerCase(java.util.Locale.ROOT).trim();
            for(NativeEffectCatalog.Effect effect:NativeEffectCatalog.all()){
                if(!effect.name.toLowerCase(java.util.Locale.ROOT).contains(term))continue;
                TextView line=ui.chip((effect.id==chosen?"✓  ":"")+effect.name,effect.id==chosen);
                options.addView(line,ui.lp(3,3));
                line.setOnClickListener(v->{tap(v);dialog.dismiss();onChoose.chosen(effect.id);});
            }
            if(options.getChildCount()==0)options.addView(ui.caption("No matching effects"));
        };
        query.addTextChangedListener(watcher(fill));
        fill.run();dialog.show();
    }
    private TextWatcher watcher(Runnable onChange){
        return new TextWatcher(){
            @Override public void beforeTextChanged(CharSequence s,int a,int c,int f){}
            @Override public void onTextChanged(CharSequence s,int start,int before,int count){onChange.run();}
            @Override public void afterTextChanged(Editable e){}
        };
    }
    private static void put(JSONObject object,String key,Object value){
        try{object.put(key,value);}catch(JSONException ignored){}
    }
    private static int bound(int value,int min,int max){return Math.max(min,Math.min(max,value));}
    private String hex(int color){return String.format(java.util.Locale.US,"#%06x",color&0xffffff);}
    private int parseHex(String value,int fallback){
        try{return Color.parseColor(value);}catch(Exception e){return fallback;}
    }
    private JSONObject currentEffectSettings(){
        JSONObject out=new JSONObject();
        put(out,"fx",effectId);put(out,"sx",speed);put(out,"ix",intensity);put(out,"bri",brightness);put(out,"pal",0);
        put(out,"col",new JSONArray().put(hex(foreground)).put(hex(background)).put(hex(accentColor)));
        return out;
    }
    private void applyEffectSettings(JSONObject x){
        if(x==null)return;
        effectId=x.optInt("fx",effectId);
        speed=bound(x.optInt("sx",speed),0,255);
        intensity=bound(x.optInt("ix",intensity),0,255);
        brightness=bound(x.optInt("bri",brightness),0,255);
        JSONArray colors=x.optJSONArray("col");
        if(colors!=null&&colors.length()>0){
            foreground=parseHex(colors.optString(0),foreground);
            background=parseHex(colors.optString(1),background);
            accentColor=parseHex(colors.optString(2),accentColor);
        }
        saveDraft();render();
    }

    private void sequenceEditor(){
        LinearLayout c=ui.card();
        c.addView(ui.text("Sequences",17,true));
        secondary(c,"Arrange, duplicate, edit and reorder steps. Playback here is simulated.");
        action(c,"+ Add step",true,()->{
            JSONArray old=project.sequence();
            if(old.length()>=NativeProject.MAX_SEQUENCE){toast("Maximum 50 steps");return;}
            old.put(new JSONObject());
            JSONObject last=old.optJSONObject(old.length()-1);
            put(last,"fx",NativeEffectCatalog.byId(effectId).name);
            put(last,"ms",3000);put(last,"settings",currentEffectSettings());
            project.save();render();
        });
        JSONArray sequence=project.sequence();
        for(int i=0;i<sequence.length();i++){
            final int at=i;
            JSONObject step=sequence.optJSONObject(i);
            if(step==null)continue;
            LinearLayout item=ui.card();
            item.setBackground(ui.shape(ui.raised,17));
            item.addView(ui.text((i+1)+".  "+step.optString("fx","Solid"),14,true));
            secondary(item,String.format(java.util.Locale.US,"%.1f seconds · %s",step.optInt("ms",3000)/1000f,
                    step.has("settings")?"Custom settings":"Default settings"));
            LinearLayout controls=ui.horizontal();
            for(String label:new String[]{"Edit","↑","↓","⧉","×"}){
                TextView b=ui.chip(label,false);
                LinearLayout.LayoutParams lp=ui.fillWeight();lp.rightMargin=ui.dp(4);
                controls.addView(b,lp);
                b.setOnClickListener(v->{
                    tap(v);
                    if("Edit".equals(label))editSequenceStep(at);
                    else if("↑".equals(label))moveStep(at,-1);
                    else if("↓".equals(label))moveStep(at,1);
                    else if("⧉".equals(label))duplicateStep(at);
                    else deleteStep(at);
                });
            }
            item.addView(controls,ui.lp(9,0));c.addView(item,ui.lp(9,0));
        }
        JSONObject opts=project.sequenceOptions();
        for(String[] option:new String[][]{
                {"Repeat","shuffleLoop"}, // existing web key: repeat state is separately editor-local
                {"Shuffle each loop","shuffleLoop"},
                {"Shuffle favorites only","favoritesOnly"},
                {"Shared palette","sharedPaletteEnabled"}}){
            String key=option[1];
            if("Repeat".equals(option[0]))key="repeat";
            final String storedKey=key;
            CheckBox cb=new CheckBox(this);cb.setText(option[0]);cb.setTextColor(ui.text);
            cb.setButtonTintList(android.content.res.ColorStateList.valueOf(ui.accent));
            cb.setChecked(opts.optBoolean(storedKey,false));
            cb.setOnCheckedChangeListener((button,checked)->{
                put(opts,storedKey,checked);project.save();
            });c.addView(cb,ui.lp(7,0));
        }
        action(c,"⤨ Shuffle effects",false,()->{
            JSONArray next=project.sequence();
            for(int j=0;j<next.length();j++){
                JSONObject step=next.optJSONObject(j);
                NativeEffectCatalog.Effect e=NativeEffectCatalog.all().get(random.nextInt(NativeEffectCatalog.all().size()));
                put(step,"fx",e.name);step.remove("settings");
            }
            project.save();render();
        });
        addCard(c);
    }
    private void editSequenceStep(int index){
        JSONObject item=project.sequence().optJSONObject(index);if(item==null)return;
        int original=NativeEffectCatalog.byName(item.optString("fx","Solid")).id;
        LinearLayout box=ui.vertical();box.setPadding(ui.dp(20),ui.dp(12),ui.dp(20),ui.dp(7));
        TextView effect=ui.button(NativeEffectCatalog.byId(original).name,false);
        box.addView(effect);
        final int[] selected={original};
        effect.setOnClickListener(v->chooseEffect(selected[0],id->{
            selected[0]=id;effect.setText(NativeEffectCatalog.byId(id).name);
        }));
        EditText seconds=ui.field(String.valueOf(item.optInt("ms",3000)/1000f),"Seconds");
        seconds.setInputType(InputType.TYPE_CLASS_NUMBER|InputType.TYPE_NUMBER_FLAG_DECIMAL);
        box.addView(ui.caption("Duration (1.5–300 seconds)"),ui.lp(10,4));box.addView(seconds);
        new AlertDialog.Builder(this).setTitle("Edit sequence step").setView(box)
                .setNegativeButton("Cancel",null).setPositiveButton("Save",(d,w)->{
                    float sec;
                    try{sec=Float.parseFloat(seconds.getText().toString());}catch(Exception ex){sec=3f;}
                    put(item,"fx",NativeEffectCatalog.byId(selected[0]).name);
                    put(item,"ms",bound(Math.round(sec*1000f),1500,300000));
                    put(item,"settings",currentEffectSettings());
                    project.save();render();
                }).show();
    }
    private void duplicateStep(int index){
        JSONArray seq=project.sequence();
        if(seq.length()>=NativeProject.MAX_SEQUENCE)return;
        JSONArray n=new JSONArray();
        for(int i=0;i<seq.length();i++){
            n.put(seq.optJSONObject(i));
            if(i==index)try{n.put(new JSONObject(seq.getJSONObject(i).toString()));}catch(Exception ignored){}
        }
        project.setSequence(n);render();
    }
    private void deleteStep(int index){
        JSONArray seq=project.sequence(),n=new JSONArray();
        for(int i=0;i<seq.length();i++)if(i!=index)n.put(seq.optJSONObject(i));
        project.setSequence(n);render();
    }
    private void moveStep(int index,int offset){
        int to=index+offset;JSONArray seq=project.sequence();
        if(to<0||to>=seq.length())return;
        JSONArray n=new JSONArray();
        for(int i=0;i<seq.length();i++)
            n.put(seq.optJSONObject(i==index?to:i==to?index:i));
        project.setSequence(n);render();
    }
    private void presetEditor(){
        LinearLayout c=ui.card();
        c.addView(ui.text("Presets",17,true));
        secondary(c,"Save and recall local designs. No controller upload occurs.");
        action(c,"+ Save current design",true,()->{
            JSONArray a=project.presets();
            if(a.length()>=NativeProject.MAX_PRESETS){toast("Maximum 40 presets");return;}
            JSONObject preset=new JSONObject();
            put(preset,"id","preset_"+java.util.UUID.randomUUID().toString().replace("-",""));
            put(preset,"name",NativeEffectCatalog.byId(effectId).name+" preset");
            JSONObject studio=new JSONObject();put(studio,"effect_id",effectId);
            put(studio,"settings",currentEffectSettings());
            put(preset,"studio",studio);a.put(preset);project.save();render();
        });
        JSONArray ps=project.presets();
        for(int i=0;i<ps.length();i++){
            JSONObject p=ps.optJSONObject(i);
            if(p==null)continue;
            final int index=i;
            LinearLayout item=ui.card();item.setBackground(ui.shape(ui.raised,17));
            item.addView(ui.text(p.optString("name","Preset"),14,true));
            secondary(item,p.optJSONObject("studio")!=null?"Native / web design preset":"Imported legacy preset");
            rowButtons(item,"Load",()->{
                JSONObject x=p.optJSONObject("studio");
                if(x==null){toast("Legacy device snapshot is not a design preset");return;}
                effectId=x.optInt("effect_id",effectId);
                applyEffectSettings(x.optJSONObject("settings"));
                showTab=0;render();
            },"Edit",()->editPreset(index));
            c.addView(item,ui.lp(8,0));
        }
        addCard(c);
    }
    private void editPreset(int index){
        JSONObject p=project.presets().optJSONObject(index);if(p==null)return;
        EditText field=ui.field(p.optString("name",""),"Preset name");
        LinearLayout content=ui.vertical();content.setPadding(ui.dp(20),ui.dp(10),ui.dp(20),ui.dp(5));
        content.addView(field);
        new AlertDialog.Builder(this).setTitle("Edit preset").setView(content)
                .setNegativeButton("Cancel",null)
                .setNeutralButton("Delete",(d,w)->{
                    JSONArray n=new JSONArray(),old=project.presets();
                    for(int i=0;i<old.length();i++)if(i!=index)n.put(old.optJSONObject(i));
                    project.setPresets(n);render();
                })
                .setPositiveButton("Save",(d,w)->{
                    put(p,"name",field.getText().toString().trim().substring(0,Math.min(48,field.length())));
                    project.save();render();
                }).show();
    }

    private void layout(){
        pageTitle("Layout","Universally configurable zones with zero-based pixel indexes.");
        LinearLayout c=ui.card();
        c.addView(ui.text("Project name",13,true));
        EditText name=ui.field(project.name(),"Project name");
        name.setImeOptions(EditorInfo.IME_ACTION_DONE);
        name.setOnEditorActionListener((v,id,event)->{
            project.name(name.getText().toString());toast("Project name saved");
            return false;
        });
        c.addView(name,ui.lp(8,6));
        action(c,"Save project name",false,()->{
            project.name(name.getText().toString());render();
        });
        TextView overview=ui.caption(project.zoneCount()+" zones · "+project.pixelCount()+" pixels assigned");
        c.addView(overview,ui.lp(14,2));
        if(project.zoneCount()==0)action(c,"+ Add zone",true,()->addZoneAfter(-1));
        addCard(c);
        for(int i=0;i<project.zoneCount();i++){
            JSONObject zone=project.zone(i);
            if(zone==null)continue;
            final int index=i;
            String id=zone.optString("id");
            int start=zone.optInt("start"),len=zone.optInt("length");
            LinearLayout z=ui.card();
            z.addView(ui.text(zone.optString("name","Zone "+(i+1)),17,true));
            secondary(z,len+" pixels  ·  Start "+start+"  ·  End "+(start+len-1));
            secondary(z,"Next zone starts at "+(start+len)+(zone.optBoolean("reverse")?"  ·  Reversed":""));
            action(z,"Edit zone",false,()->editZone(id));
            rowButtons(z,"+ Add next",()->addZoneAfter(index),"Remove",()->confirmRemoveZone(id));
            addCard(z);
        }
    }
    private void addZoneAfter(int index){
        if(!project.addZone("Zone "+(project.zoneCount()+1),30)){
            toast("Cannot add zone");return;
        }
        if(index>=0&&index<project.zoneCount()-2)
            project.moveZone(project.zoneCount()-1,index+1);
        render();
    }
    private void confirmRemoveZone(String id){
        JSONObject z=project.zone(project.indexOfZone(id));
        new AlertDialog.Builder(this).setTitle("Remove zone?")
                .setMessage(z==null?"Remove the selected zone?":
                        "Remove "+z.optString("name")+"? Trigger assignments to this zone will also be cleared.")
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Remove",(d,w)->{
                    project.removeZone(id);render();
                }).show();
    }
    private void editZone(String id){
        JSONObject z=project.zone(project.indexOfZone(id));
        if(z==null)return;
        LinearLayout panel=ui.vertical();panel.setPadding(ui.dp(20),ui.dp(8),ui.dp(20),ui.dp(4));
        EditText label=ui.field(z.optString("name"),"Zone name");
        panel.addView(ui.caption("Zone name"));panel.addView(label,ui.lp(6,8));
        EditText count=ui.field(String.valueOf(z.optInt("length",30)),"Exact pixel count");
        count.setInputType(InputType.TYPE_CLASS_NUMBER);
        panel.addView(ui.caption("Pixel count (1–10,000)"));panel.addView(count,ui.lp(6,7));
        TextView metrics=ui.caption(project.previewZoneOffsets(id,z.optInt("length",30)));
        panel.addView(metrics,ui.lp(5,3));
        SeekBar range=ui.slider(Math.max(250,Math.min(10000,z.optInt("length",30)*2+50)),
                z.optInt("length",30));
        panel.addView(range);
        range.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            @Override public void onProgressChanged(SeekBar bar,int n,boolean from){
                if(!from)return;
                count.setText(String.valueOf(Math.max(1,n)));
            }
            @Override public void onStartTrackingTouch(SeekBar bar){}
            @Override public void onStopTrackingTouch(SeekBar bar){haptics.tap(bar,"slider");}
        });
        // Editable exact count and contextual slider stay synchronized.
        // Downstream offsets update visually while dragging or typing, prior
        // to Save, with no BLE command and no change to the persisted layout.
        count.addTextChangedListener(new TextWatcher(){
            @Override public void beforeTextChanged(CharSequence v,int a,int n,int after){}
            @Override public void onTextChanged(CharSequence value,int a,int before,int n){
                int length=number(value.toString(),-1);
                metrics.setText(project.previewZoneOffsets(id,length));
                if(length>=1&&length<=10000){
                    if(length>range.getMax())
                        range.setMax(Math.min(10000,Math.max(length,length*2)));
                    if(range.getProgress()!=length)range.setProgress(length);
                }
            }
            @Override public void afterTextChanged(Editable value){}
        });
        CheckBox reverse=new CheckBox(this);reverse.setText("Reverse pixel direction");
        reverse.setTextColor(ui.text);reverse.setChecked(z.optBoolean("reverse"));
        reverse.setButtonTintList(android.content.res.ColorStateList.valueOf(ui.accent));
        panel.addView(reverse,ui.lp(6,6));
        secondary(panel,"Auto offsets follow the prior zone. This edits the design only, never live output hardware.");
        new AlertDialog.Builder(this).setTitle("Edit zone").setView(panel)
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Save",(d,w)->{
                    int length=number(count.getText().toString(),-1);
                    if(!project.editZone(id,label.getText().toString(),length,reverse.isChecked()))
                        toast("Name or pixel length invalid (1–10,000)");
                    render();
                }).show();
    }
    private int number(String n,int fallback){
        try{return Integer.parseInt(n);}catch(Exception e){return fallback;}
    }

    private void triggers(){
        pageTitle("Triggers","Design trigger assignments and animations offline.");
        LinearLayout c=ui.card();
        c.addView(ui.text("Triggers",17,true));
        secondary(c,"Select one or more triggers to edit their assigned zones.");
        JSONObject profiles=project.profiles();
        JSONArray names=profiles.names();
        LinkedHashSet<String> ordered=new LinkedHashSet<>(Arrays.asList(NativeProject.MAIN_TRIGGERS));
        if(names!=null)for(int i=0;i<names.length();i++){
            String n=names.optString(i);
            if(!"Startup".equals(n)&&!"Shutdown".equals(n)&&!"Show".equals(n))ordered.add(n);
        }
        for(String name:ordered){
            TextView chip=ui.chip((selectedTriggers.contains(name)?"✓  ":"○  ")+name,
                    selectedTriggers.contains(name));
            c.addView(chip,ui.lp(4,4));
            chip.setOnClickListener(v->{
                tap(v);
                if(selectedTriggers.contains(name))selectedTriggers.remove(name);
                else selectedTriggers.add(name);
                render();
            });
        }
        rowButtons(c,"Edit "+selectedTriggers.size()+" selected",()->editSelectedTriggers(),
                "Clear",()->{selectedTriggers.clear();render();});
        action(c,"+ Add custom trigger",false,()->{
            EditText name=ui.field("","Trigger name");
            LinearLayout box=ui.vertical();box.setPadding(ui.dp(18),ui.dp(8),ui.dp(18),ui.dp(6));box.addView(name);
            new AlertDialog.Builder(this).setTitle("Add software trigger").setView(box)
                    .setNegativeButton("Cancel",null).setPositiveButton("Create",(d,w)->{
                        if(!project.customTrigger(name.getText().toString()))toast("Invalid or duplicate trigger name");
                        render();
                    }).show();
        });
        addCard(c);
        LinearLayout example=ui.card();
        example.addView(ui.text("Trigger preview",15,true));
        secondary(example,"Editing assignments does not simulate actual vehicle inputs. CORE /dev handles physical testing.");
        action(example,"Preview selected animation",false,()->{
            if(selectedTriggers.isEmpty()){toast("Select a trigger first");return;}
            String name=selectedTriggers.iterator().next();
            JSONObject p=project.trigger(name);
            foreground=parseHex(p.optString("color","#ff0000"),foreground);
            effectId=NativeEffectCatalog.byName(p.optString("pattern","Solid")).id;
            previewPlaying=true;saveDraft();tab=1;showTab=0;render();
        });
        addCard(example);
    }
    private void editSelectedTriggers(){
        if(selectedTriggers.isEmpty()){toast("Select at least one trigger");return;}
        LinearLayout panel=ui.vertical();panel.setPadding(ui.dp(18),ui.dp(8),ui.dp(18),ui.dp(8));
        panel.addView(ui.text(selectedTriggers.size()+" trigger(s)",15,true));
        secondary(panel,"Assign design zones. This does not modify CORE input pin mapping.");
        final Set<String> assignment=new HashSet<>();
        for(int i=0;i<project.zoneCount();i++){
            JSONObject z=project.zone(i);String id=z.optString("id");
            boolean all=true;
            for(String n:selectedTriggers){
                JSONArray arr=project.trigger(n).optJSONArray("zones");
                boolean exists=false;
                if(arr!=null)for(int j=0;j<arr.length();j++)if(id.equals(arr.optString(j)))exists=true;
                all &=exists;
            }
            if(all)assignment.add(id);
            CheckBox box=new CheckBox(this);box.setText(z.optString("name"));box.setTextColor(ui.text);
            box.setChecked(all);
            box.setButtonTintList(android.content.res.ColorStateList.valueOf(ui.accent));
            box.setOnCheckedChangeListener((button,checked)->{
                if(checked)assignment.add(id);else assignment.remove(id);
            });
            panel.addView(box);
        }
        int[] choice={effectId};
        TextView fx=ui.button("Effect · "+NativeEffectCatalog.byId(effectId).name,false);
        panel.addView(fx,ui.lp(10,5));fx.setOnClickListener(v->chooseEffect(choice[0],id->{
            choice[0]=id;fx.setText("Effect · "+NativeEffectCatalog.byId(id).name);
        }));
        new AlertDialog.Builder(this).setTitle("Edit selected triggers").setView(panel)
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Save",(d,w)->{
                    for(String name:selectedTriggers){
                        JSONObject profile=project.trigger(name);
                        for(int i=0;i<project.zoneCount();i++){
                            String id=project.zone(i).optString("id");
                            project.assign(name,id,assignment.contains(id));
                            if(assignment.contains(id)){
                                JSONObject animations=profile.optJSONObject("sectionAnimations");
                                if(animations==null){animations=new JSONObject();put(profile,"sectionAnimations",animations);}
                                JSONObject payload=currentEffectSettings();
                                put(payload,"pattern",NativeEffectCatalog.byId(choice[0]).name);
                                put(animations,id,payload);
                            }
                        }
                        put(profile,"pattern",NativeEffectCatalog.byId(choice[0]).name);
                    }
                    project.save();render();
                }).show();
    }

    private void autoEvents(){
        pageTitle("Auto Events","Optional startup and shutdown animation designs.");
        LinearLayout tabs=ui.horizontal();
        for(int i=0;i<2;i++){
            final int at=i;
            TextView b=ui.chip(DEFAULT_EVENTS[i],autoIndex==i);
            LinearLayout.LayoutParams p=ui.fillWeight();p.rightMargin=ui.dp(i==0?8:0);
            tabs.addView(b,p);
            b.setOnClickListener(v->{tap(v);autoIndex=at;render();});
        }
        body.addView(tabs,ui.lp(0,9));
        String name=DEFAULT_EVENTS[autoIndex];
        JSONObject p=project.trigger(name);
        LinearLayout c=ui.card();
        c.addView(ui.text(name+" animation",17,true));
        secondary(c,"Configuration only · Not linked to ignition detection or automatic CORE playback.");
        CheckBox enabled=new CheckBox(this);
        enabled.setText("Enable "+name);
        enabled.setTextColor(ui.text);
        enabled.setButtonTintList(android.content.res.ColorStateList.valueOf(ui.accent));
        enabled.setChecked(p.optBoolean("eventUiEnabled",false));
        enabled.setOnCheckedChangeListener((button,on)->{
            put(p,"eventUiEnabled",on);project.save();
        });
        c.addView(enabled,ui.lp(7,10));
        TextView fx=ui.button("Animation · "+p.optString("eventFx","Solid"),false);
        c.addView(fx);
        fx.setOnClickListener(v->chooseEffect(
                NativeEffectCatalog.byName(p.optString("eventFx","Solid")).id,id->{
            put(p,"eventFx",NativeEffectCatalog.byId(id).name);project.save();render();
        }));
        int fade=bound(p.optInt("eventUiFadeMs",0),0,5000);
        sliderRow(c,"Fade transition ("+fade+" ms)",fade,5000,v->{
            put(p,"eventUiFadeMs",v);project.save();
        });
        int duration=bound(Math.round((float)p.optDouble("eventDuration",2)*10),2,300);
        sliderRow(c,"Duration ("+(duration/10f)+" sec)",duration,300,v->{
            put(p,"eventDuration",v/10f);project.save();
        });
        action(c,"▷ Preview offline",true,()->{
            effectId=NativeEffectCatalog.byName(p.optString("eventFx","Solid")).id;
            previewPlaying=true;saveDraft();tab=1;showTab=0;render();
        });
        addCard(c);
    }

    private void settings(){
        pageTitle("Settings","Project, appearance and optional controller tools.");
        LinearLayout appearance=ui.card();
        appearance.addView(ui.text("Appearance",17,true));
        secondary(appearance,"The same approved OLED black and white modes as web Studio.");
        rowButtons(appearance,"OLED black",()->setWhiteMode(false),
                "White mode",()->setWhiteMode(true));
        addCard(appearance);
        LinearLayout data=ui.card();
        data.addView(ui.text("Project backup",17,true));
        secondary(data,"Use the shared OpenTail v4 JSON format. Import/export does not deploy to a controller.");
        rowButtons(data,"Export JSON",()->exportDocument(),
                "Import JSON",()->importDocument());
        addCard(data);
        LinearLayout b=ui.card();
        b.addView(ui.text("Controller · Optional",17,true));
        secondary(b,"CORE Bluetooth testing and authorization are separate from design. The native app never connects or runs a show automatically.");
        bleStatus=ui.caption("Bluetooth · "+bleMessage);b.addView(bleStatus,ui.lp(8,9));
        rowButtons(b,"Find BLE",()->ble.scan(),"Authorize",()->ble.authorizeStored());
        rowButtons(b,"Pair with BOOT",()->ble.pairWithBoot(),
                "Read capabilities",()->ble.readFxCapabilities());
        action(b,"CORE Output Manager · Advanced",false,()->{
            if(outputManager==null)outputManager=new NativeOutputManager(this,ble,ui);
            outputManager.open(ui);
        });
        action(b,"Test selected effect on CORE · 15 seconds",false,()->confirmBleTest());
        action(b,"Stop CORE show",false,()->{
            if(!ble.canControl()){toast("Authorize CORE first");return;}
            ble.stopShow();toast("Stop requested; check physical output");
        });
        secondary(b,"Hardware input simulation, output mapping and diagnostics belong in firmware /dev. BLE test success must be confirmed on the actual LEDs.");
        addCard(b);
        LinearLayout build=ui.card();
        build.addView(ui.caption("OpenTail Android v0.2.0-alpha1 · Native Android Views"));
        secondary(build,"No WebView or bundled HTML. Preview and authoring never send device commands.");
        // Legacy Java Views is now internal-only for debug comparison.
        // Compose is the sole alpha launcher; never add a second entry.
        if((getApplicationInfo().flags & android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE)!=0){
            action(build,"Return to Kotlin + Compose",false,()->
                    startActivity(new Intent(this,ComposeMainActivity.class)));
            secondary(build,"Legacy Java Views is retained only for debug comparison. Compose is the alpha launcher.");
        }
        addCard(build);
    }
    private void confirmBleTest(){
        if(!ble.canControl()){toast("Connect and authorize CORE in Settings first");return;}
        if(!supportedBleEffects.contains(effectId)){
            toast("This effect ID is not in CORE's verified BLE capabilities");return;
        }
        new AlertDialog.Builder(this).setTitle("Bench test on CORE?")
                .setMessage("This will temporarily run "+NativeEffectCatalog.byId(effectId).name+
                        " for 15 seconds at capped brightness. Vehicle inputs retain priority. This is not a firmware regression test.")
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Test",(d,w)->{
                    int safe=Math.min(32,Math.max(1,Math.round(brightness*32f/255)));
                    ble.showFx(effectId,3,255,Color.red(foreground),Color.green(foreground),Color.blue(foreground),
                            Color.red(background),Color.green(background),Color.blue(background),
                            safe,speed,intensity,0,15);
                    toast("Test requested; verify physical LEDs");
                }).show();
    }

    private void exportDocument(){
        Intent intent=new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/json");
        intent.putExtra(Intent.EXTRA_TITLE,"opentail-project-v4.json");
        startActivityForResult(intent,EXPORT_PROJECT);
    }
    private void importDocument(){
        Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/json");
        startActivityForResult(intent,IMPORT_PROJECT);
    }
    @Override protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);
        if(result!=RESULT_OK||data==null||data.getData()==null)return;
        Uri uri=data.getData();
        try{
            if(request==EXPORT_PROJECT){
                try(OutputStream out=getContentResolver().openOutputStream(uri,"w")){
                    if(out==null)throw new IllegalStateException("File cannot be opened");
                    out.write(project.exportJson().getBytes(StandardCharsets.UTF_8));
                }
                toast("Project exported");return;
            }
            if(request==IMPORT_PROJECT){
                byte[] bytes;
                try(InputStream in=getContentResolver().openInputStream(uri);
                        ByteArrayOutputStream output=new ByteArrayOutputStream()){
                    if(in==null)throw new IllegalStateException("File cannot be opened");
                    byte[] chunk=new byte[8192];int n;
                    while((n=in.read(chunk))!=-1){
                        if(output.size()+n>2000000)throw new IllegalArgumentException("Project exceeds 2 MB");
                        output.write(chunk,0,n);
                    }
                    bytes=output.toByteArray();
                }
                if(!project.importJson(new String(bytes,StandardCharsets.UTF_8))){
                    toast("Invalid project; original project preserved");return;
                }
                selectedTriggers.clear();previewPlaying=false;tab=0;render();
                toast("Project imported");
            }
        }catch(Exception e){toast("Document operation failed: "+e.getMessage());}
    }
}
