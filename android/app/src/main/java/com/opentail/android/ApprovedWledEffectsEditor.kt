package com.opentail.android

import android.content.SharedPreferences
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import kotlin.math.atan2
import kotlin.math.hypot
import kotlin.math.PI

/**
 * Native Compose implementation of the OWNER-approved WLED+ design handoff,
 * approved-prototype.fragment.html SHA256 37DB62A26E1622A7058B503EEB3A10B731C327269FB755E6289802AACE273D9D.
 * Limited to Show Modes / Effects. Never creates physical effect frames or
 * assumes a missing firmware capability is present.
 *
 * Palette gradients only use supplied authoritative NativePalette stops.
 * Source metadata below is the 16-effect review sample, with CORE #49 flags
 * as fallback for other available catalog effects. Range is 0..255 for CORE.
 */
internal object ApprovedEditorMetadata {
    data class Param(val position: Int, val label: String, val isSwitch: Boolean)
    data class Role(val slot: Int, val label: String)
    data class Definition(val params: List<Param>, val roles: List<Role>, val palette: Boolean)

    private val sample = mapOf(
        "Blink" to "!,Duty cycle;!,!;!",
        "Blink Rainbow" to "Frequency,Blink duration;!,!;!",
        "Twinkle" to "!,!;!,!;!",
        "Twinklefox" to "!,Twinkle rate,,,,Cool;!,!;!",
        "Twinklecat" to "!,Twinkle rate,,,,Cool,Reverse;!,!;!",
        "Twinkleup" to "!,Intensity;!,!;!",
        "Glitter" to "!,!,,,,,Overlay;,,Glitter color;!",
        "Solid Glitter" to ",!;Bg,,Glitter color;",
        "Scanner Dual" to "!,Trail,Delay,,,Dual,Bi-delay;!,!,!;!",
        "Sinelon" to "!,Trail;!,!,!;!",
        "Sinelon Dual" to "!,Trail;!,!,!;!",
        "Two Dots" to "!,Dot size,,,,,Overlay;1,2,Bg;!",
        "Bouncing Balls" to "Gravity,# of balls,,,,,Overlay;!,!,!;!",
        "Solid Pattern" to "Fg size,Bg size;Fg,!;!",
        "Ripple Peak" to "Fade rate,Max # of ripples,Select bin,Volume (min);!,!;!",
        "Solid" to ";Fx;"
    )
    private val defaults = listOf("Speed", "Intensity", "Custom 1", "Custom 2",
        "Custom 3", "Option 1", "Option 2", "Option 3")
    fun definition(name: String, flags: EffectParameters?): Definition {
        val raw = sample[name]
        if (raw == null) return Definition(
            buildList {
                if(flags?.speed==true)add(Param(0,"Speed",false))
                if(flags?.intensity==true)add(Param(1,"Intensity",false))
            },
            buildList {
                if(flags?.foreground==true)add(Role(0,"Fx"))
                if(flags?.background==true)add(Role(1,"Bg"))
            },flags?.palette==true
        )
        val sections=raw.split(';')
        val params=sections.getOrElse(0){""}.split(',').mapIndexedNotNull { i, label ->
            if(label.isEmpty())null else Param(i,if(label=="!") defaults.getOrElse(i){"Parameter ${i+1}"} else label,i>=5)
        }
        val roles=sections.getOrElse(1){""}.split(',').mapIndexedNotNull { i,label ->
            if(label.isEmpty())null else Role(i,when(label) {
                "!" -> listOf("Fx","Bg","Cs").getOrElse(i){"C${i+1}"}
                "Glitter color" -> "Gl"
                else -> label
            })
        }
        return Definition(params,roles,sections.getOrElse(2){""}.isNotEmpty())
    }
    fun parameterKey(position:Int):String=when(position){0->"speed";1->"intensity";else->"param${position}"}
}

private val panel=Color(0xFF111111)
private val surface=Color(0xFF191919)
private val subtle=Color(0xFF999999)
private val ink=Color(0xFFEDEDED)

@Composable
internal fun ApprovedWledEffectsEditor(
    selectedId: Int?,
    effectOptions: List<NativeEffectCatalog.Effect>,
    segments: Int,
    liveEnabled: Boolean,
    prefs: SharedPreferences,
    paletteFor: (Int)->NativePalette,
    onSelected: (Int?)->Unit,
    onLiveToggle: ()->Unit,
    onParam: (Int,String,Int)->Unit,
    onColor: (Int,Int,Int)->Unit,
    onPalette: (Int,Int)->Unit
) {
    val effect=selectedId?.let { NativeEffectCatalog.byId(it) }
    val def=effect?.let { ApprovedEditorMetadata.definition(it.name,NativeEffectParameters.forId(it.id)) }
    var sheet by remember { mutableStateOf("") }
    var role by remember { mutableIntStateOf(0) }
    var paletteSearch by rememberSaveable { mutableStateOf("") }
    var effectSearch by rememberSaveable { mutableStateOf("") }
    var stateRevision by remember { mutableIntStateOf(0) }
    val currentId=selectedId ?: 0
    val paletteId=prefs.getInt("wled.${currentId}.palette",prefs.getInt("paletteId",0))
    val stops=paletteFor(paletteId).stops
    fun intSetting(key:String,default:Int):Int {
        @Suppress("UNUSED_VARIABLE") val observe = stateRevision
        return prefs.getInt("wled.${currentId}.${key}",prefs.getInt(key,default))
    }
    fun colorSetting(slot:Int):Int {
        @Suppress("UNUSED_VARIABLE") val observe = stateRevision
        return prefs.getInt("wled.${currentId}.color${slot}", when(slot){
            0->prefs.getInt("foreground",0xffd735f4.toInt())
            1->prefs.getInt("background",0xff000000.toInt())
            else->prefs.getInt("accent",0xffffffff.toInt())
        })
    }
    fun changeParam(key:String,value:Int) {
        onParam(currentId,key,value)
        stateRevision++
    }
    fun changeColor(slot:Int,rgb:Int) {
        onColor(currentId,slot,rgb)
        stateRevision++
    }
    fun changePalette(id:Int) {
        onPalette(currentId,id)
        stateRevision++
        sheet=""
    }
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp))
        .background(panel).padding(12.dp).testTag("approved-effects-main"),
        verticalArrangement=Arrangement.spacedBy(7.dp)) {
        Row(Modifier.fillMaxWidth().height(40.dp), verticalAlignment=Alignment.CenterVertically) {
            Box(Modifier.size(24.dp).clip(RoundedCornerShape(50)).background(Color(0xFFDDDDDD)),
                contentAlignment=Alignment.Center){Text("A",fontSize=15.sp,color=Color(0xFF222222))}
            Spacer(Modifier.width(10.dp))
            Column(Modifier.weight(1f)){
                Text("All Segments",fontSize=13.sp,fontWeight=FontWeight.Medium,color=ink)
                Text("${segments} Segments",fontSize=11.sp,color=subtle)
            }
            Text(if(liveEnabled)"Ⅱ" else "▶",fontSize=20.sp,color=subtle)
            Spacer(Modifier.width(10.dp))
            Switch(checked=liveEnabled,onCheckedChange={onLiveToggle()},
                modifier=Modifier.height(24.dp).testTag("effects-live-toggle"),
                colors=SwitchDefaults.colors(checkedThumbColor=Color(0xFF181818),
                    checkedTrackColor=Color(0xFFD6D6D6),
                    uncheckedThumbColor=Color(0xFFAAAAAA),
                    uncheckedTrackColor=Color(0xFF333333)))
        }
        Row(Modifier.fillMaxWidth().height(58.dp).clip(RoundedCornerShape(10.dp))
            .background(surface).clickable{sheet="effects"}.padding(start=12.dp,end=8.dp),
            verticalAlignment=Alignment.CenterVertically){
            Column(Modifier.weight(1f)){
                Text(effect?.name ?: "Select effect",fontSize=13.sp,color=ink,
                    maxLines=1,overflow=TextOverflow.Ellipsis)
                Text("Effect",fontSize=11.sp,color=subtle)
            }
            EditorChevron()
        }
        Box(Modifier.fillMaxWidth().height(58.dp).clip(RoundedCornerShape(10.dp))
            .background(surface)) {
            Row(Modifier.fillMaxSize().padding(start=12.dp,end=8.dp,bottom=4.dp),
                verticalAlignment=Alignment.CenterVertically) {
                Column(Modifier.weight(1f).clickable { if(def?.palette==true)sheet="palettes" }) {
                    Text(if(def?.palette==true) paletteFor(paletteId).name else "Colors",
                        fontSize=13.sp,color=ink,maxLines=1,overflow=TextOverflow.Ellipsis)
                    Text(if(def?.palette==true)"Palette" else "Effect colors",
                        fontSize=11.sp,color=subtle)
                }
                def?.roles?.forEach { r ->
                    ColorChip(r.label,colorSetting(r.slot)) {
                        role=r.slot
                        sheet="colors"
                    }
                    Spacer(Modifier.width(4.dp))
                }
                if(def?.palette==true)
                    Box(Modifier.clickable{sheet="palettes"}){EditorChevron()}
                else Spacer(Modifier.width(24.dp))
            }
            if(def?.palette==true && stops != null)
                Canvas(Modifier.align(Alignment.BottomCenter).fillMaxWidth().height(4.dp)){
                    drawRect(brush=Brush.horizontalGradient(
                        colorStops=stops.map{it.first to it.second}.toTypedArray()))
                }
        }
        if(effect != null) {
            EditorNumeric("Brightness",intSetting("brightness",160),255,
                {changeParam("brightness",it)})
            def?.params?.forEach { p ->
                val key=ApprovedEditorMetadata.parameterKey(p.position)
                if(p.isSwitch) {
                    val enabled=intSetting(key,0)!=0
                    Row(Modifier.fillMaxWidth().height(50.dp),
                        verticalAlignment=Alignment.CenterVertically) {
                        Text(p.label,Modifier.weight(1f),color=ink,fontSize=12.sp)
                        Switch(checked=enabled,onCheckedChange={changeParam(key,if(it)1 else 0)})
                    }
                } else EditorNumeric(p.label,intSetting(key,if(p.position==0)128 else 128),255,
                    {changeParam(key,it)})
            }
        }
    }
    when(sheet){
        "effects" -> EditorSheet(title="Select effect",onClose={sheet=""}){
            BasicTextField(value=effectSearch,onValueChange={effectSearch=it},
                singleLine=true,modifier=Modifier.fillMaxWidth()
                    .clip(RoundedCornerShape(99.dp)).background(surface).padding(12.dp),
                textStyle=TextStyle(fontSize=13.sp,color=ink),
                decorationBox={inner->Box {
                    if(effectSearch.isBlank()) Text("Search effects",fontSize=13.sp,color=subtle)
                    inner()
                }})
            Spacer(Modifier.height(8.dp))
            LazyColumn(Modifier.fillMaxWidth().heightIn(max=360.dp)){
                items(effectOptions.filter{it.name.contains(effectSearch.trim(),true)},key={it.id}){item->
                    val picked=item.id==selectedId
                    Row(Modifier.fillMaxWidth().padding(vertical=3.dp).height(44.dp)
                        .clip(RoundedCornerShape(13.dp))
                        .background(if(picked)Color(0xFF303030) else surface)
                        .clickable {
                            onSelected(if(picked)null else item.id)
                            sheet=""
                        }.padding(horizontal=12.dp),
                        verticalAlignment=Alignment.CenterVertically){
                        Text(item.name,Modifier.weight(1f),color=ink,fontSize=13.sp)
                        if(picked)Text("✓",color=ink)
                    }
                }
            }
        }
        "palettes" -> EditorSheet(title="Select palette",onClose={sheet=""}){
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.End){
                def?.roles?.forEach { r ->
                    ColorChip(r.label,colorSetting(r.slot)){role=r.slot;sheet="colors"}
                    Spacer(Modifier.width(4.dp))
                }
            }
            LazyColumn(Modifier.fillMaxWidth().heightIn(max=360.dp)){
                items((0..NativePaletteEngine.MAX_ID).filter {
                    paletteFor(it).name.contains(paletteSearch.trim(),true) ||
                        it.toString()==paletteSearch.trim()
                },key={it}){id->
                    val palette=paletteFor(id)
                    Column(Modifier.fillMaxWidth().padding(vertical=3.dp).height(43.dp)
                        .clip(RoundedCornerShape(13.dp))
                        .background(surface).clickable{changePalette(id)}) {
                        Row(Modifier.fillMaxWidth().weight(1f).padding(horizontal=10.dp),
                            verticalAlignment=Alignment.CenterVertically) {
                            Text(palette.name,Modifier.weight(1f),color=ink,fontSize=12.sp)
                            if(id==paletteId)Text("Selected",fontSize=10.sp,color=ink)
                        }
                        if(palette.stops!=null)
                            Canvas(Modifier.fillMaxWidth().height(5.dp)){
                                drawRect(brush=Brush.horizontalGradient(
                                    colorStops=palette.stops.map{it.first to it.second}.toTypedArray()))
                            }
                    }
                }
            }
            Spacer(Modifier.height(8.dp))
            BasicTextField(value=paletteSearch,onValueChange={paletteSearch=it},
                singleLine=true,modifier=Modifier.fillMaxWidth().height(43.dp)
                    .clip(RoundedCornerShape(12.dp)).background(surface)
                    .padding(horizontal=12.dp,vertical=10.dp),
                textStyle=TextStyle(color=ink,fontSize=13.sp),
                decorationBox={inner->Box{
                    if(paletteSearch.isEmpty())Text("Search palettes",color=subtle,fontSize=13.sp)
                    inner()
                }})
        }
        "colors" -> EditorSheet(title="Select colors",onClose={sheet=""}){
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.End){
                def?.roles?.forEach { r ->
                    ColorChip(r.label,colorSetting(r.slot)){role=r.slot}
                    Spacer(Modifier.width(4.dp))
                }
            }
            val current=colorSetting(role)
            val hsv=remember(current) { FloatArray(3).also {
                android.graphics.Color.colorToHSV(current,it)
            }}
            Row(Modifier.fillMaxWidth().padding(top=12.dp),
                horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                ColorWheel(Modifier.weight(2f),current) {hue,saturation->
                    changeColor(role,android.graphics.Color.HSVToColor(
                        floatArrayOf(hue,saturation,hsv[2])))
                }
                val swatches=listOf(0xFFDD35F4,0xFFB735FA,0xFF932BE5,0xFF4A25D8,
                    0xFF1267DD,0xFF12A0F0,0xFF009E7B,0xFF20CC6E,0xFF9EDC2A,
                    0xFFE2CB0B,0xFFFF961E,0xFFFF552E,0xFF0A0A0A,0xFFFFFFFF,0xFF999999)
                Column(Modifier.weight(1f).clip(RoundedCornerShape(13.dp))
                    .background(surface).padding(6.dp),
                    verticalArrangement=Arrangement.spacedBy(5.dp)){
                    swatches.chunked(3).forEach { row ->
                        Row(horizontalArrangement=Arrangement.spacedBy(5.dp)){
                            row.forEach { c ->
                                Box(Modifier.weight(1f).aspectRatio(1f)
                                    .clip(RoundedCornerShape(50))
                                    .background(Color(c.toInt())).clickable {
                                        changeColor(role,c.toInt())
                                    })
                            }
                        }
                    }
                }
            }
            val brightness=(hsv[2]*255).toInt()
            EditorNumeric("Value/Brightness",brightness,255,{v->
                changeColor(role,android.graphics.Color.HSVToColor(
                    floatArrayOf(hsv[0],hsv[1],v/255f)))
            })
            var temperature by rememberSaveable(currentId,role) {
                mutableIntStateOf(prefs.getInt("wled.${currentId}.temp${role}",72))
            }
            EditorNumeric("Kelvin/Temperature",temperature,100,{v->
                // Relative position only: no unverified Kelvin->RGB or CORE write.
                temperature=v
                prefs.edit().putInt("wled.${currentId}.temp${role}",v).apply()
            })
        }
    }
}

@Composable
private fun EditorChevron(){
    Canvas(Modifier.size(width=24.dp,height=32.dp)){
        val mid=size.width/2f
        val y=size.height/2f
        val color=Color(0xFF999999)
        drawLine(color,Offset(mid-6.dp.toPx(),y-3.dp.toPx()),
            Offset(mid,y+3.dp.toPx()),2.dp.toPx(),cap=StrokeCap.Round)
        drawLine(color,Offset(mid,y+3.dp.toPx()),
            Offset(mid+6.dp.toPx(),y-3.dp.toPx()),2.dp.toPx(),cap=StrokeCap.Round)
    }
}
@Composable
private fun ColorChip(label:String,rgb:Int,onClick:()->Unit){
    val c=Color(rgb or 0xff000000.toInt())
    val luminance=(c.red*.299f+c.green*.587f+c.blue*.114f)
    Box(Modifier.size(32.dp).clip(RoundedCornerShape(50)).background(c)
        .border(1.dp,Color(0xFF666666),RoundedCornerShape(50)).clickable(onClick=onClick),
        contentAlignment=Alignment.Center){
        Text(label,color=if(luminance>.59f)Color.Black else Color.White,
            fontSize=11.sp,maxLines=1)
    }
}
@Composable
private fun EditorNumeric(label:String,value:Int,max:Int,onChange:(Int)->Unit){
    Column(Modifier.fillMaxWidth().height(75.dp).padding(top=8.dp),
        verticalArrangement=Arrangement.spacedBy(2.dp)){
        Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
            Text(label,Modifier.weight(1f),color=ink,fontSize=12.sp)
            Text("${value}",color=ink,fontSize=12.sp)
        }
        Slider(value=value.coerceIn(0,max).toFloat(),
            onValueChange={onChange(it.toInt().coerceIn(0,max))},
            valueRange=0f..max.toFloat(),
            colors=SliderDefaults.colors(
                thumbColor=Color.Black,activeTrackColor=ink,
                inactiveTrackColor=Color(0xFF1B1B1B)),
            modifier=Modifier.fillMaxWidth().height(36.dp))
    }
}
@Composable
private fun EditorSheet(title:String,onClose:()->Unit,content:@Composable ColumnScope.()->Unit){
    Dialog(onDismissRequest=onClose,
        properties=DialogProperties(usePlatformDefaultWidth=false,dismissOnBackPress=true,
            dismissOnClickOutside=true)){
        Column(Modifier.fillMaxWidth(.88f).widthIn(max=360.dp)
            .clip(RoundedCornerShape(28.dp)).background(panel)
            .padding(18.dp).testTag("approved-wled-popup"),
            verticalArrangement=Arrangement.spacedBy(4.dp)){
            Row(Modifier.fillMaxWidth().height(42.dp),
                verticalAlignment=Alignment.CenterVertically){
                Text("×",Modifier.width(28.dp).clickable(onClick=onClose),
                    fontSize=25.sp,color=subtle)
                Text(title,color=ink,fontSize=15.sp)
            }
            content()
        }
    }
}
@Composable
private fun ColorWheel(modifier:Modifier,rgb:Int,onPick:(Float,Float)->Unit){
    val hsv=remember(rgb){FloatArray(3).also{android.graphics.Color.colorToHSV(rgb,it)}}
    fun touch(pos:Offset,w:Float,h:Float){
        val dx=pos.x-w/2f
        val dy=pos.y-h/2f
        val hue=((atan2(dy,dx)*180.0/PI+360)%360).toFloat()
        val sat=(hypot(dx,dy)/(minOf(w,h)/2f)).coerceIn(0f,1f)
        onPick(hue,sat)
    }
    var dimensions by remember { mutableStateOf(Offset(170f,170f)) }
    Canvas(modifier.aspectRatio(1f).clip(RoundedCornerShape(13.dp))
        .background(surface).padding(10.dp)
        .pointerInput(Unit){detectTapGestures { touch(it,dimensions.x,dimensions.y) }}
        .pointerInput(Unit){detectDragGestures {change,_ ->
            touch(change.position,dimensions.x,dimensions.y)
        }}){
        dimensions=Offset(size.width,size.height)
        val radius=minOf(size.width,size.height)/2f
        val center=Offset(size.width/2f,size.height/2f)
        drawCircle(brush=Brush.sweepGradient(
            listOf(Color.Red,Color.Magenta,Color.Blue,Color.Cyan,
                Color.Green,Color.Yellow,Color.Red)),radius=radius,center=center)
        drawCircle(brush=Brush.radialGradient(
            listOf(Color.White,Color.White.copy(alpha=.75f),Color.Transparent),
            center=center,radius=radius),radius=radius,center=center)
        val theta=hsv[0]*PI/180.0
        val x=center.x+kotlin.math.cos(theta).toFloat()*hsv[1]*radius
        val y=center.y+kotlin.math.sin(theta).toFloat()*hsv[1]*radius
        drawCircle(Color.White,radius=6.dp.toPx(),center=Offset(x,y),style=Stroke(width=1.dp.toPx()))
    }
}
