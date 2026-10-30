package com.opentail.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Color;
import android.text.InputType;
import android.text.TextWatcher;
import android.text.Editable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.AdapterView;
import org.json.JSONArray;
import org.json.JSONObject;

/**
 * Native Android BLE output configuration editor, separated from Studio's
 * offline design layout. Hardware-read drafts remain per physical output.
 * Saves are explicit, firmware-bounded, and read back by BleDiscovery before
 * the UI reports success. Never assumes a local draft controls physical LEDs.
 */
final class NativeOutputManager {
    private final Activity activity;
    private final BleDiscovery ble;
    private NativeUi theme;
    private AlertDialog dialog;
    private LinearLayout page;
    private final JSONObject[] drafts = new JSONObject[2];
    private int selected = 0;
    private String message = "Load an output from CORE before editing.";
    NativeOutputManager(Activity activity, BleDiscovery ble, NativeUi theme) {
        this.activity=activity;this.ble=ble;this.theme=theme;
    }
    void open(NativeUi current) {
        theme=current;
        if(!ble.canConfigure()){
            new AlertDialog.Builder(activity)
                    .setTitle("Authorize CORE first")
                    .setMessage("OpenTail CORE must be connected and authorized over Bluetooth. " +
                            "Use Find BLE / Pair with BOOT / Authorize in Settings.")
                    .setPositiveButton("OK",null).show();
            return;
        }
        ScrollView scroll=new ScrollView(activity);
        page=theme.vertical();
        page.setPadding(theme.dp(16),theme.dp(12),theme.dp(16),theme.dp(20));
        scroll.addView(page);
        dialog=new AlertDialog.Builder(activity).setTitle("CORE Outputs · BLE")
                .setView(scroll).setNegativeButton("Close",null).create();
        dialog.setOnDismissListener(d->page=null);
        dialog.show();
        android.view.Window w=dialog.getWindow();
        if(w!=null)w.setLayout((int)(activity.getResources().getDisplayMetrics().widthPixels*.94),
                (int)(activity.getResources().getDisplayMetrics().heightPixels*.85));
        render();
    }
    void onBleResult(String json) {
        try {
            JSONObject result=new JSONObject(json);
            int index=result.optInt("index",-1);
            boolean ok=result.optBoolean("ok",false);
            message=result.optString("message","Unknown controller response");
            if(ok&&index>=0&&index<2&&result.optJSONObject("output")!=null)
                drafts[index]=new JSONObject(result.getJSONObject("output").toString());
        } catch(Exception e) { message="Invalid CORE configuration response"; }
        activity.runOnUiThread(this::render);
    }
    private void notifyUser(String title,String body){
        new AlertDialog.Builder(activity).setTitle(title).setMessage(body)
                .setPositiveButton("OK",null).show();
    }
    private void action(LinearLayout panel,String label,Runnable callback) {
        TextView b=theme.button(label,false);
        panel.addView(b,theme.lp(7,2));
        b.setOnClickListener(v->callback.run());
    }
    private TextWatcher edit(java.util.function.Consumer<String> save) {
        return new TextWatcher(){
            @Override public void beforeTextChanged(CharSequence s,int a,int c,int f){}
            @Override public void afterTextChanged(Editable e){save.accept(e.toString());}
            @Override public void onTextChanged(CharSequence s,int a,int before,int count){}
        };
    }
    private void field(LinearLayout p,String label,String value,boolean numeric,
                       java.util.function.Consumer<String> onEdit){
        p.addView(theme.caption(label),theme.lp(8,3));
        EditText e=theme.field(value,label);
        if(numeric)e.setInputType(InputType.TYPE_CLASS_NUMBER);
        e.addTextChangedListener(edit(onEdit));p.addView(e,theme.lp(2,4));
    }
    private Spinner spinner(LinearLayout p,String label,String[] choices,String current,
                            java.util.function.Consumer<String> onChoose){
        p.addView(theme.caption(label),theme.lp(9,4));
        Spinner s=new Spinner(activity);
        ArrayAdapter<String> adapter=new ArrayAdapter<>(activity,android.R.layout.simple_spinner_dropdown_item,choices);
        s.setAdapter(adapter);
        int selected=0;for(int i=0;i<choices.length;i++)if(current.equals(choices[i]))selected=i;
        s.setSelection(selected);
        s.setBackground(theme.shape(theme.raised,16));s.setPadding(theme.dp(12),0,theme.dp(12),0);
        s.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){
            @Override public void onItemSelected(AdapterView<?> parent,View v,int index,long id){
                onChoose.accept(choices[index]);
                // Platform drop-down text adapts to both dark and light themes.
                if(v instanceof TextView)((TextView)v).setTextColor(theme.text);
            }
            @Override public void onNothingSelected(AdapterView<?> parent){}
        });
        p.addView(s,new LinearLayout.LayoutParams(-1,theme.dp(46)));
        return s;
    }
    private static void put(JSONObject j,String key,Object val) {
        try{j.put(key,val);}catch(Exception ignored){}
    }
    private static int getInt(String input,int fallback){
        try{return Integer.parseInt(input);}catch(Exception e){return fallback;}
    }
    private void render() {
        if(page==null)return;
        page.removeAllViews();
        page.addView(theme.text("Readback-controlled hardware editor",16,true));
        page.addView(theme.caption("Current firmware: two physical outputs, up to 623 LEDs / output (1,246 total), " +
                "eight zones per output. These limits are firmware protocol bounds, not Studio design defaults."),
                theme.lp(5,8));
        LinearLayout chooser=theme.horizontal();
        for(int i=0;i<2;i++) {
            final int index=i;
            TextView button=theme.chip("Output "+(i+1),selected==i);
            LinearLayout.LayoutParams p=theme.fillWeight();p.rightMargin=theme.dp(7);
            chooser.addView(button,p);
            button.setOnClickListener(v->{selected=index;render();});
        }
        page.addView(chooser,theme.lp(6,9));
        page.addView(theme.caption(message),theme.lp(6,3));
        action(page,"Reload output "+(selected+1)+" from CORE",()->ble.readOutput(selected));
        JSONObject draft=drafts[selected];
        if(draft==null) {
            page.addView(theme.caption("No saved output readback yet. Authorize Bluetooth, then tap Reload."),
                    theme.lp(15,3));
            return;
        }
        LinearLayout fields=theme.card();
        fields.addView(theme.text("Output "+(selected+1),16,true));
        field(fields,"Output name",draft.optString("name"),false,value->put(draft,"name",value));
        field(fields,"Active pixels (1–623)",String.valueOf(draft.optInt("pixels",0)),true,
                value->put(draft,"pixels",getInt(value,-1)));
        spinner(fields,"Color order",BleConfigCodec.ORDERS,
                draft.optString("color_order","RGB"),value->put(draft,"color_order",value));
        page.addView(fields,theme.lp(10,9));

        JSONArray zones=draft.optJSONArray("zones");
        if(zones==null){zones=new JSONArray();put(draft,"zones",zones);}
        int total=0;
        for(int i=0;i<zones.length();i++){
            JSONObject zone=zones.optJSONObject(i);
            if(zone==null)continue;
            total+=zone.optInt("pixels",0);
            final int index=i;
            LinearLayout card=theme.card();
            card.addView(theme.text("Zone "+(i+1),15,true));
            field(card,"Zone name",zone.optString("name"),false,value->put(zone,"name",value));
            field(card,"Pixels",String.valueOf(zone.optInt("pixels",0)),true,
                    value->put(zone,"pixels",getInt(value,-1)));
            spinner(card,"Lighting role",BleConfigCodec.ROLES,zone.optString("role","off"),
                    value->put(zone,"role",value));
            final JSONArray zoneArr=zones;
            action(card,"Remove zone",()->{
                if(zoneArr.length()<2) {
                    notifyUser("At least one zone required","The current CORE BLE protocol requires 1–8 zones.");return;
                }
                JSONArray next=new JSONArray();
                for(int j=0;j<zoneArr.length();j++)
                    if(j!=index)next.put(zoneArr.optJSONObject(j));
                put(draft,"zones",next);render();
            });
            page.addView(card,theme.lp(7,6));
        }
        page.addView(theme.caption("Assigned "+total+" / "+draft.optInt("pixels")+" pixels. " +
                (total==draft.optInt("pixels")?"Counts match.":"Fix counts before saving.")),
                theme.lp(9,4));
        if(zones.length()<8)action(page,"+ Add zone",()->{
            JSONArray list=draft.optJSONArray("zones");
            JSONObject next=new JSONObject();
            put(next,"name","Zone "+(list.length()+1));put(next,"pixels",0);
            put(next,"role","off");list.put(next);render();
        });
        final int index=selected;
        action(page,"Save to CORE · requires readback",()->confirmSave(index));
    }
    private void confirmSave(int index){
        if(!ble.canConfigure()){
            notifyUser("Not authorized","Reconnect and authorize CORE before saving.");return;
        }
        JSONObject draft=drafts[index];if(draft==null)return;
        try{BleConfigCodec.encode(draft);}catch(Exception e){
            notifyUser("Cannot save output","Fix the draft first: "+e.getMessage());return;
        }
        new AlertDialog.Builder(activity).setTitle("Save physical output?")
                .setMessage("Stop all shows and release vehicle inputs before saving. " +
                        "CORE will verify the flash configuration by reading it back. " +
                        "This changes output "+(index+1)+", not the Studio design layout.")
                .setNegativeButton("Cancel",null)
                .setPositiveButton("Save",(d,w)->{
                    message="Save requested · waiting for CORE response/readback…";
                    render();
                    ble.saveOutput(index,draft.toString());
                }).show();
    }
}
