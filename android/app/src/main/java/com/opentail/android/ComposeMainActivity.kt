package com.opentail.android

import android.bluetooth.BluetoothAdapter
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.json.JSONArray
import org.json.JSONObject

/**
 * Kotlin + Jetpack Compose migration candidate. The Java v4 document model,
 * BLE GATT transport and codec stay available without conversion or WebView.
 * Strictly local authoring; only explicit, confirmed Settings controls send BLE.
 *
 * This class is intentionally NOT the launcher until all functional parity
 * checks and on-phone visual acceptance pass. Old MainActivity stays runnable.
 */
class ComposeMainActivity : ComponentActivity() {
    private val pages = listOf("Home", "Show Modes", "Layout", "Triggers", "Auto Events", "Settings")
    private lateinit var project: NativeProject
    private lateinit var ble: BleDiscovery
    private val revision = mutableIntStateOf(0)
    private val bleMessage = mutableStateOf("Disconnected")
    private val supportedIds = mutableStateOf(setOf<Int>())
    private val outputs = mutableStateMapOf<Int, JSONObject>()
    private val outputMessage = mutableStateOf("No device output read yet")
    private val prefs by lazy { getSharedPreferences("opentail_native_ui_v1", Context.MODE_PRIVATE) }

    private val importer = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            try {
                val data = contentResolver.openInputStream(uri)?.bufferedReader()?.use { it.readText() } ?: ""
                outputMessage.value = if (project.importJson(data)) "Design imported" else "Invalid v4 project"
                refresh()
            } catch (e: Exception) { outputMessage.value = "Import failed: " + (e.message ?: "invalid file") }
        }
    }
    private val exporter = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        if (uri != null) {
            try {
                contentResolver.openOutputStream(uri)?.bufferedWriter()?.use { it.write(project.exportJson()) }
                outputMessage.value = "Design exported"
            } catch (e: Exception) { outputMessage.value = "Export failed: " + (e.message ?: "storage error") }
        }
    }
    private val adapterReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (intent.action != BluetoothAdapter.ACTION_STATE_CHANGED) return
            val state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)
            if (state == BluetoothAdapter.STATE_OFF || state == BluetoothAdapter.STATE_TURNING_OFF)
                ble.onAdapterDisabled()
            else if (state == BluetoothAdapter.STATE_ON) ble.onAdapterEnabled()
        }
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        project = NativeProject(this)
        ble = BleDiscovery(this) { status -> runOnUiThread { bleMessage.value = status } }
        ble.setFxCapabilitiesListener { data ->
            try {
                val list = JSONArray(data)
                supportedIds.value = (0 until list.length()).map { list.optInt(it) }.toSet()
            } catch (_: Exception) { supportedIds.value = emptySet() }
        }
        ble.setConfigListener { raw ->
            try {
                val response = JSONObject(raw)
                outputMessage.value = response.optString("message", "CORE output result")
                val index = response.optInt("index", -1)
                if (response.optBoolean("ok") && index in 0..1)
                    response.optJSONObject("output")?.let { outputs[index] = JSONObject(it.toString()) }
            } catch (_: Exception) { outputMessage.value = "Invalid output reply" }
        }
        val filter = IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED)
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(adapterReceiver, filter, Context.RECEIVER_EXPORTED)
        else registerReceiver(adapterReceiver, filter)
        setContent { OpenTailShell() }
    }
    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        ble.handlePermissions(requestCode, grantResults)
    }
    override fun onStop() { super.onStop(); ble.stopScan() }
    override fun onDestroy() {
        unregisterReceiver(adapterReceiver)
        ble.shutdown()
        super.onDestroy()
    }
    private fun refresh() { revision.intValue++ }

    private data class Tone(val bg: Color, val surface: Color, val raised: Color,
                            val fg: Color, val muted: Color, val accent: Color, val red: Color)
    private fun tone(light: Boolean): Tone = if (light)
        Tone(Color.White, Color(0xFFF4F4F4), Color(0xFFE6E6E6),
             Color(0xFF181818), Color(0xFF606060), Color(0xFF226E45), Color(0xFFBD3842))
    else Tone(Color.Black, Color(0xFF101010), Color(0xFF1B1B1B),
              Color(0xFFF5F5F5), Color(0xFFA7A7A7), Color(0xFF79D99B), Color(0xFFED6472))

    @Composable
    private fun OpenTailShell() {
        val changed = revision.intValue // observe authoritative model revisions
        var page by rememberSaveable { mutableIntStateOf(0) }
        var light by remember { mutableStateOf(prefs.getBoolean("whiteMode", false)) }
        val t = remember(light) { tone(light) }
        Column(Modifier.fillMaxSize().background(t.bg).padding(top = 14.dp)) {
            Row(Modifier.fillMaxWidth().padding(horizontal = 18.dp), verticalAlignment = Alignment.CenterVertically) {
                Text("OpenTail", color = t.fg, fontSize = 23.sp, fontWeight = FontWeight.Bold,
                     modifier = Modifier.weight(1f))
                Pill(if (light) "OLED" else "White", t, false) {
                    light = !light
                    prefs.edit().putBoolean("whiteMode", light).apply()
                }
            }
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(12.dp),
                horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                pages.forEachIndexed { i, title ->
                    Pill(title, t, page == i) { page = i }
                }
            }
            Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())
                .padding(horizontal = 16.dp, vertical = 8.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp)) {
                when (page) {
                    0 -> HomePage(t, { page = it })
                    1 -> ShowModesPage(t)
                    2 -> LayoutPage(t)
                    3 -> TriggersPage(t)
                    4 -> AutoEventsPage(t)
                    5 -> SettingsPage(t)
                }
                Spacer(Modifier.height(24.dp))
            }
        }
    }
    @Composable
    private fun Pill(label: String, t: Tone, selected: Boolean = false, action: () -> Unit) {
        Box(Modifier.clip(RoundedCornerShape(99.dp))
                .background(if (selected) t.accent else t.raised)
                .clickable(onClick = action).padding(horizontal = 15.dp, vertical = 11.dp),
            contentAlignment = Alignment.Center) {
            Text(label, fontSize = 13.sp, fontWeight = FontWeight.SemiBold,
                 color = if (selected) t.bg else t.fg, maxLines = 1)
        }
    }
    @Composable
    private fun Panel(t: Tone, content: @Composable ColumnScope.() -> Unit) {
        Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp))
            .background(t.surface).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(9.dp), content = content)
    }
    @Composable
    private fun Caption(text: String, t: Tone) {
        Text(text, color = t.muted, fontSize = 12.sp, lineHeight = 17.sp)
    }
    @Composable
    private fun Title(text: String, t: Tone) {
        Text(text, color = t.fg, fontSize = 21.sp, fontWeight = FontWeight.Bold)
    }
    @Composable
    private fun Field(value: String, hint: String, t: Tone, onChange: (String) -> Unit) {
        BasicTextField(
            value = value, onValueChange = onChange, singleLine = true,
            cursorBrush = SolidColor(t.accent),
            textStyle = TextStyle(color = t.fg, fontSize = 14.sp),
            modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(99.dp))
                .background(t.raised).padding(horizontal = 15.dp, vertical = 13.dp),
            decorationBox = { inner ->
                Box {
                    if (value.isEmpty()) Text(hint, fontSize = 14.sp, color = t.muted)
                    inner()
                }
            }
        )
    }
    @Composable
    private fun ValueSlider(title: String, value: Int, max: Int, t: Tone, change: (Int) -> Unit) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(title, Modifier.weight(1f), color = t.fg, fontSize = 13.sp)
            Text(value.toString(), color = t.accent, fontSize = 13.sp)
        }
        Slider(value = value.coerceIn(0, max).toFloat(), onValueChange = { change(it.toInt()) },
               valueRange = 0f..max.toFloat(), colors = SliderDefaults.colors(
                   thumbColor = t.accent, activeTrackColor = t.accent,
                   inactiveTrackColor = t.raised))
    }
    @Composable
    private fun PaletteEditor(label: String, color: Int, t: Tone, change: (Int) -> Unit) {
        var hex by remember(color) {
            mutableStateOf(String.format("#%06X", color and 0xffffff))
        }
        Caption(label + " · " + hex, t)
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Box(Modifier.size(32.dp).clip(RoundedCornerShape(12.dp))
                .background(Color(color)))
            Field(hex, "#RRGGBB", t) { entered ->
                hex = entered.take(7)
                if (Regex("^#[0-9A-Fa-f]{6}$").matches(hex))
                    change(android.graphics.Color.parseColor(hex))
            }
        }
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(7.dp)) {
            listOf("White" to 0xffffff, "Red" to 0xff0000,
                "Amber" to 0xffbf00, "Blue" to 0x0088ff,
                "Green" to 0x00ff77, "Black" to 0x000000).forEach { (name, value) ->
                val chosen = 0xff000000.toInt() or value
                Pill(name, t, (color and 0xffffff) == value) { change(chosen) }
            }
        }
    }
    @Composable
    private fun HomePage(t: Tone, changePage: (Int) -> Unit) {
        Title("Home", t)
        Panel(t) {
            Text(project.name(), color = t.fg, fontSize = 18.sp, fontWeight = FontWeight.Bold)
            Caption(project.zoneCount().toString() + " zones · " +
                    project.pixelCount().toString() + " design pixels", t)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Pill("Show Modes", t) { changePage(1) }
                Pill("Edit Layout", t) { changePage(2) }
            }
        }
        Panel(t) {
            Caption("Web Studio and Android author offline. CORE firmware renders animations, " +
                    "owns trigger arbitration and drives the physical outputs.", t)
            Caption("Design edits never change vehicle lighting unless explicitly sent from Settings.", t)
        }
    }
    @Composable
    private fun ShowModesPage(t: Tone) {
        Title("Show Modes", t)
        var view by rememberSaveable { mutableStateOf("Effects") }
        var query by rememberSaveable { mutableStateOf("") }
        var effect by rememberSaveable { mutableIntStateOf(prefs.getInt("effectId", 0)) }
        var speed by rememberSaveable { mutableIntStateOf(prefs.getInt("speed", 128)) }
        var intensity by rememberSaveable { mutableIntStateOf(prefs.getInt("intensity", 128)) }
        var brightness by rememberSaveable { mutableIntStateOf(prefs.getInt("brightness", 160)) }
        var foreground by rememberSaveable { mutableIntStateOf(prefs.getInt("foreground", -1)) }
        var background by rememberSaveable { mutableIntStateOf(prefs.getInt("background", 0xff000000.toInt())) }
        var accent by rememberSaveable { mutableIntStateOf(prefs.getInt("accent", -1)) }
        Panel(t) {
            Caption("Native offline preview — CORE is the authoritative physical renderer", t)
            Canvas(Modifier.fillMaxWidth().height(64.dp)
                .clip(RoundedCornerShape(14.dp)).background(t.raised)) {
                val count = 28
                for (i in 0 until count) {
                    val color = if (effect == 0 || i % 5 < 3)
                        Color(foreground) else Color(background)
                    drawLine(color, Offset(i * size.width / count, size.height * 0.2f),
                             Offset(i * size.width / count, size.height * 0.8f),
                             strokeWidth = size.width / count * 0.65f)
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf("Effects", "Sequences", "Presets").forEach {
                    Pill(it, t, view == it) { view = it }
                }
            }
        }
        when (view) {
            "Effects" -> {
                Panel(t) {
                    Field(query, "Search effects", t) { query = it }
                    Caption("Selected: " + NativeEffectCatalog.byId(effect).name, t)
                    ValueSlider("Speed", speed, 255, t) { speed = it; prefs.edit().putInt("speed", it).apply() }
                    ValueSlider("Intensity", intensity, 255, t) { intensity = it; prefs.edit().putInt("intensity", it).apply() }
                    ValueSlider("Brightness", brightness, 255, t) { brightness = it; prefs.edit().putInt("brightness", it).apply() }
                    PaletteEditor("Foreground", foreground, t) {
                        foreground = it; prefs.edit().putInt("foreground", it).apply()
                    }
                    PaletteEditor("Background", background, t) {
                        background = it; prefs.edit().putInt("background", it).apply()
                    }
                    PaletteEditor("Accent", accent, t) {
                        accent = it; prefs.edit().putInt("accent", it).apply()
                    }
                    Caption("Offline illustration only. Exact timing and palettes are rendered by CORE.", t)
                }
                NativeEffectCatalog.all().filter { it.name.contains(query, ignoreCase = true) }
                    .forEach { item ->
                        Panel(t) {
                            Pill(item.name, t, item.id == effect) {
                                effect = item.id
                                prefs.edit().putInt("effectId", effect).apply()
                            }
                        }
                    }
            }
            "Sequences" -> SequencePage(t, effect)
            else -> PresetsPage(t)
        }
    }
    @Composable
    private fun SequencePage(t: Tone, selectedEffect: Int) {
        val steps = project.sequence()
        val opts = project.sequenceOptions()
        var editingStep by remember { mutableIntStateOf(-1) }
        var editedDuration by remember { mutableStateOf("") }
        Panel(t) {
            Caption("Playback settings are saved in the v4 design; they never command CORE.", t)
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                listOf("Shuffle" to "shuffleLoop", "Repeat" to "repeat",
                    "Shared Palette" to "sharedPaletteEnabled").forEach { (name, key) ->
                    val enabled = opts.optBoolean(key, false)
                    Pill(name + (if (enabled) " ✓" else ""), t, enabled) {
                        opts.put(key, !enabled)
                        project.save()
                        refresh()
                    }
                }
            }
            if (opts.optBoolean("sharedPaletteEnabled", false)) {
                ValueSlider("Shared palette", opts.optInt("sharedPaletteId", 0), 255, t) { n ->
                    opts.put("sharedPaletteId", n)
                    project.save()
                    refresh()
                }
            }
        }
        Panel(t) {
            Title("Sequence designer", t)
            Caption("Locally saved. No BLE commands are sent.", t)
            Pill("+ Add selected effect", t, true) {
                if (steps.length() < NativeProject.MAX_SEQUENCE) {
                    steps.put(JSONObject().put("fx", NativeEffectCatalog.byId(selectedEffect).name)
                        .put("ms", 1500))
                    project.setSequence(steps); refresh()
                }
            }
        }
        for (i in 0 until steps.length()) {
            val item = steps.optJSONObject(i) ?: continue
            Panel(t) {
                Text(item.optString("fx") + " · " + item.optInt("ms", 1500) + " ms", color = t.fg)
                Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                    Pill("↑", t) { moveSequence(i, -1) }
                    Pill("↓", t) { moveSequence(i, 1) }
                    Pill("Duplicate", t) { duplicateSequence(i) }
                    Pill("Edit time", t) {
                        editingStep = i
                        editedDuration = item.optInt("ms", 1500).toString()
                    }
                    Pill("Remove", t) { removeSequence(i) }
                }
            }
        }
        if (editingStep in 0 until steps.length()) {
            AlertDialog(onDismissRequest = { editingStep = -1 },
                title = { Text("Edit sequence duration") },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Caption("50–600,000 milliseconds. Editable before saving to the design.", t)
                        Field(editedDuration, "Duration in milliseconds", t) {
                            editedDuration = it.filter(Char::isDigit).take(6)
                        }
                    }
                },
                confirmButton = { Pill("Save", t, true) {
                    val ms = editedDuration.toIntOrNull()
                    if (ms != null && ms in 50..600000) {
                        val updated = sequenceList()
                        updated[editingStep].put("ms", ms)
                        updateSequence(updated)
                        editingStep = -1
                    }
                } },
                dismissButton = { Pill("Cancel", t) { editingStep = -1 } })
        }
    }
    private fun sequenceList(): MutableList<JSONObject> =
        (0 until project.sequence().length()).mapNotNull { project.sequence().optJSONObject(it) }.toMutableList()
    private fun updateSequence(list: List<JSONObject>) {
        val arr = JSONArray(); list.forEach { arr.put(it) }
        project.setSequence(arr); refresh()
    }
    private fun moveSequence(i: Int, delta: Int) {
        val list = sequenceList()
        val destination = i + delta
        if (i in list.indices && destination in list.indices) {
            val step = list.removeAt(i); list.add(destination, step); updateSequence(list)
        }
    }
    private fun duplicateSequence(i: Int) {
        val list = sequenceList()
        if (i in list.indices && list.size < NativeProject.MAX_SEQUENCE) {
            list.add(i + 1, JSONObject(list[i].toString())); updateSequence(list)
        }
    }
    private fun removeSequence(i: Int) {
        val list = sequenceList()
        if (i in list.indices) { list.removeAt(i); updateSequence(list) }
    }
    @Composable
    private fun PresetsPage(t: Tone) {
        val presets = project.presets()
        Panel(t) {
            Title("Presets", t)
            Caption("These save a local design snapshot, not CORE firmware.", t)
            Pill("+ Save current design", t, true) {
                if (presets.length() < NativeProject.MAX_PRESETS) {
                    presets.put(JSONObject().put("name", "Preset " + (presets.length() + 1))
                        .put("studio", project.exportJson()))
                    project.setPresets(presets);refresh()
                }
            }
        }
        for (i in 0 until presets.length()) {
            val p = presets.optJSONObject(i) ?: continue
            Panel(t) {
                Text(p.optString("name", "Preset"), color = t.fg)
                Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                    Pill("Load design", t) {
                        if (project.importJson(p.optString("studio"))) refresh()
                    }
                    Pill("Remove", t) {
                        val next = JSONArray()
                        for (j in 0 until presets.length()) if (j != i) next.put(presets.opt(j))
                        project.setPresets(next);refresh()
                    }
                }
            }
        }
    }
    @Composable
    private fun LayoutPage(t: Tone) {
        Title("Layout", t)
        var name by remember { mutableStateOf(project.name()) }
        var editingId by remember { mutableStateOf<String?>(null) }
        var deleteId by remember { mutableStateOf<String?>(null) }
        Panel(t) {
            Field(name, "Project name", t) { name = it }
            Pill("Save name", t) { project.name(name);refresh() }
            Caption(project.zoneCount().toString() + " zones · " + project.pixelCount() + " design pixels", t)
            if (project.zoneCount() == 0) Pill("+ Add zone", t, true) { addZoneAfter(-1) }
        }
        for (i in 0 until project.zoneCount()) {
            val z = project.zone(i) ?: continue
            val id = z.optString("id")
            val start = z.optInt("start")
            val length = z.optInt("length")
            Panel(t) {
                Text(z.optString("name"), color = t.fg, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                Caption("Pixels " + length + " · start " + start + " · end " + (start + length - 1), t)
                Caption("Next zone starts at " + (start + length), t)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Pill("Edit", t) { editingId = id }
                    Pill("+ Add next", t) { addZoneAfter(i) }
                    Pill("Remove", t) { deleteId = id }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Pill("Move ↑", t) { if (project.moveZone(i, i - 1)) refresh() }
                    Pill("Move ↓", t) { if (project.moveZone(i, i + 1)) refresh() }
                }
            }
        }
        editingId?.let { id ->
            val index = project.indexOfZone(id)
            val zone = if (index >= 0) project.zone(index) else null
            if (zone != null) {
                var label by remember(id) { mutableStateOf(zone.optString("name")) }
                var count by remember(id) { mutableStateOf(zone.optInt("length").toString()) }
                var reverse by remember(id) { mutableStateOf(zone.optBoolean("reverse")) }
                AlertDialog(onDismissRequest = { editingId = null },
                    title = { Text("Edit zone") },
                    text = {
                        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                            Field(label, "Zone name", t) { label = it }
                            Field(count, "Pixel count", t) { count = it.filter(Char::isDigit).take(5) }
                            val n = count.toIntOrNull() ?: 0
                            Slider(value = n.coerceIn(1, 10000).toFloat(),
                                   onValueChange = { count = it.toInt().coerceIn(1, 10000).toString() },
                                   valueRange = 1f..10000f)
                            Caption(project.previewZoneOffsets(id, n), t)
                            Pill(if (reverse) "Reverse pixels ✓" else "Reverse pixels", t, reverse) {
                                reverse = !reverse
                            }
                        }
                    },
                    confirmButton = { Pill("Save", t, true) {
                        if (project.editZone(id, label, count.toIntOrNull() ?: 0, reverse)) {
                            editingId = null;refresh()
                        }
                    } },
                    dismissButton = { Pill("Cancel", t) { editingId = null } })
            }
        }
        deleteId?.let { id ->
            AlertDialog(onDismissRequest = { deleteId = null }, title = { Text("Remove this zone?") },
                text = { Text("Only assignments to this zone will be removed.") },
                confirmButton = { Pill("Remove", t, true) {
                    project.removeZone(id);deleteId = null;refresh()
                } }, dismissButton = { Pill("Cancel", t) { deleteId = null } })
        }
    }
    private fun addZoneAfter(index: Int) {
        if (project.addZone("Zone " + (project.zoneCount() + 1), 30)) {
            if (index >= 0 && index < project.zoneCount() - 2)
                project.moveZone(project.zoneCount() - 1, index + 1)
            refresh()
        }
    }
    @Composable
    private fun TriggersPage(t: Tone) {
        Title("Triggers", t)
        Caption("Universal offline trigger assignments. Physical inputs and arbitration are authoritative in CORE.", t)
        var selections by remember { mutableStateOf(setOf<String>()) }
        var testing by remember { mutableStateOf(setOf<String>()) }
        var editing by remember { mutableStateOf(false) }
        var newTrigger by remember { mutableStateOf("") }
        var searchFx by remember { mutableStateOf("") }
        var chosenFx by remember { mutableStateOf("Solid") }
        var chosenColor by remember { mutableStateOf("#ff0000") }
        val profiles = project.profiles()
        val names = profiles.names()
        val keys = (0 until (names?.length() ?: 0))
            .mapNotNull { names?.optString(it) }
            .filterNot { it == "Startup" || it == "Shutdown" || it == "Show" }
        Panel(t) {
            Caption("Multiple triggers can be selected and edited together. Nothing on this page sends BLE commands.", t)
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Field(newTrigger, "Custom trigger name", t) { newTrigger = it.take(32) }
            }
            Pill("+ Add trigger", t) {
                if (project.customTrigger(newTrigger)) {
                    selections = setOf(newTrigger)
                    newTrigger = ""
                    refresh()
                }
            }
            Pill("Edit selected · " + selections.size, t, selections.isNotEmpty()) {
                if (selections.isNotEmpty()) {
                    val p = project.trigger(selections.first())
                    chosenFx = p.optString("pattern", "Solid")
                    chosenColor = p.optString("color", "#ff0000")
                    searchFx = ""
                    editing = true
                }
            }
        }
        keys.forEach { name ->
            val profile = project.trigger(name)
            val selected = name in selections
            val active = name in testing
            Panel(t) {
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Text(name, modifier = Modifier.weight(1f),
                        color = t.fg, fontWeight = FontWeight.SemiBold)
                    Pill(if (selected) "Selected ✓" else "Select", t, selected) {
                        selections = if (selected) selections - name else selections + name
                    }
                }
                Caption("Effect: " + profile.optString("pattern", "Solid") +
                    " · zones: " + (profile.optJSONArray("zones")?.length() ?: 0), t)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Pill(if (active) "Stop" else "Start", t, active) {
                        testing = if (active) testing - name else testing + name
                    }
                    if (profile.optString("kind") == "trigger-custom")
                        Pill("Remove", t) {
                            if (project.deleteTrigger(name)) {
                                selections = selections - name
                                testing = testing - name
                                refresh()
                            }
                        }
                }
                if (active) Caption("Local trigger preview active. CORE output unchanged.", t)
            }
        }
        if (editing) {
            AlertDialog(onDismissRequest = { editing = false },
                title = { Text("Edit " + selections.size + " triggers") },
                text = {
                    Column(Modifier.heightIn(max = 400.dp).verticalScroll(rememberScrollState()),
                        verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Caption("Effect (applies to every selected trigger)", t)
                        Field(searchFx, "Search effects", t) { searchFx = it }
                        Caption("Selected: " + chosenFx, t)
                        NativeEffectCatalog.all()
                            .filter { it.name.contains(searchFx, ignoreCase = true) }
                            .take(12).forEach { fx ->
                                Pill(fx.name, t, chosenFx == fx.name) { chosenFx = fx.name }
                            }
                        Caption("Trigger color (hex #RRGGBB)", t)
                        Field(chosenColor, "#ff0000", t) { chosenColor = it.take(7) }
                        Caption("Zone assignment (select to apply to all)", t)
                        for (i in 0 until project.zoneCount()) {
                            val z = project.zone(i) ?: continue
                            val zoneId = z.optString("id")
                            val checked = selections.all { name ->
                                val assigned = project.trigger(name).optJSONArray("zones")
                                (0 until (assigned?.length() ?: 0))
                                    .any { assigned?.optString(it) == zoneId }
                            }
                            Pill(z.optString("name") + (if (checked) " ✓" else ""), t, checked) {
                                selections.forEach { project.assign(it, zoneId, !checked) }
                                refresh()
                            }
                        }
                    }
                },
                confirmButton = { Pill("Save", t, true) {
                    val validColor = Regex("^#[0-9A-Fa-f]{6}$").matches(chosenColor)
                    if (validColor && NativeEffectCatalog.all().any { it.name == chosenFx }) {
                        selections.forEach { name ->
                            project.trigger(name).put("pattern", chosenFx)
                                .put("color", chosenColor)
                        }
                        project.save()
                        editing = false
                        refresh()
                    }
                } },
                dismissButton = { Pill("Cancel", t) { editing = false } })
        }
    }
    @Composable
    private fun AutoEventsPage(t: Tone) {
        Title("Auto Events", t)
        Caption("Optional startup/shutdown events; preview and configuration are local.", t)
        listOf("Startup", "Shutdown").forEach { key ->
            val trigger = project.trigger(key)
            Panel(t) {
                Text(key, color = t.fg, fontWeight = FontWeight.Bold)
                val enabled = trigger.optBoolean("enabled", false)
                Pill(if (enabled) "Enabled ✓" else "Disabled", t, enabled) {
                    trigger.put("enabled", !enabled);project.save();refresh()
                }
                val currentSpeed = trigger.optInt("speed", 60)
                ValueSlider("Transition speed", currentSpeed, 255, t) {
                    trigger.put("speed", it);project.save();refresh()
                }
                Caption("Current animation: " + trigger.optString("pattern", "Solid"), t)
                Pill("Materialize", t) {
                    trigger.put("pattern", "Materialize Sparks");project.save();refresh()
                }
                Pill("Smooth Fade", t) {
                    trigger.put("pattern", "Smooth Fade");project.save();refresh()
                }
            }
        }
    }
    @Composable
    private fun SettingsPage(t: Tone) {
        Title("Settings", t)
        Panel(t) {
            Text("Bluetooth · " + bleMessage.value, color = t.fg)
            Caption("Authorize only on a physically present CORE bench controller.", t)
            Pill("Find CORE Bluetooth", t) { ble.scan() }
            Pill("Pair / BOOT", t) { ble.pairWithBoot() }
            Pill("Authorize saved key", t) { ble.authorizeStored() }
            Pill("Read supported effects", t) { ble.readFxCapabilities() }
            Caption(supportedIds.value.size.toString() + " effects reported by connected CORE", t)
        }
        var confirm by remember { mutableStateOf(false) }
        Panel(t) {
            Caption("Explicit short bench effect demo. Never sent during authoring.", t)
            Pill("Test a supported effect", t) { confirm = true }
            Pill("Stop CORE show", t) { ble.stopShow() }
        }
        if (confirm) AlertDialog(onDismissRequest = { confirm = false },
            title = { Text("Test CORE LEDs on isolated bench?") },
            text = { Text("Requires BLE authorization and reported effect support. 15-second TTL, brightness 24/255. Vehicle inputs retain priority.") },
            confirmButton = { Pill("Start bench test", t, true) {
                val id = prefs.getInt("effectId", 0)
                if (ble.canControl() && id in supportedIds.value) {
                    ble.showFx(id, 3, 255, 255, 0, 0, 0, 0, 0, 24, 128, 128, 0, 15)
                } else bleMessage.value = "Not authorized or effect unsupported by installed firmware"
                confirm = false
            } }, dismissButton = { Pill("Cancel", t) { confirm = false } })
        Panel(t) {
            Title("Projects", t)
            Pill("Export design JSON", t) { exporter.launch("opentail-project.json") }
            Pill("Import design JSON", t) { importer.launch(arrayOf("application/json", "text/plain")) }
            Caption(outputMessage.value, t)
        }
        HardwareOutputEditor(t)
    }
    @Composable
    private fun HardwareOutputEditor(t: Tone) {
        var index by rememberSaveable { mutableIntStateOf(0) }
        // String snapshots make edits observable without mutating the last
        // confirmed hardware readback. A failed save cannot masquerade as NVS.
        var draftText by remember(index, outputs[index]) {
            mutableStateOf(outputs[index]?.toString().orEmpty())
        }
        var confirm by remember { mutableStateOf(false) }
        val draft = runCatching { JSONObject(draftText) }.getOrNull()
        fun edit(change: (JSONObject) -> Unit) {
            runCatching {
                val updated = JSONObject(draftText)
                change(updated)
                draftText = updated.toString()
            }.onFailure { outputMessage.value = "Invalid draft: " + (it.message ?: "input") }
        }

        Panel(t) {
            Title("CORE Output Manager", t)
            Caption("Hardware settings are separate from your Studio design. Reload each output over authorized BLE before editing.", t)
            Caption("Current firmware allocation: up to 623 pixels per output; older installed CORE versions may allow fewer.", t)
            Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                Pill("Output 1", t, index == 0) { index = 0 }
                Pill("Output 2", t, index == 1) { index = 1 }
            }
            Pill("Reload output " + (index + 1) + " from CORE", t) {
                if (ble.canConfigure()) ble.readOutput(index)
                else outputMessage.value = "Connect and authorize BLE before loading hardware"
            }
            Caption(outputMessage.value, t)
            if (draft == null) Caption("Nothing has been read from this output. No settings will be sent until you explicitly confirm Save.", t)
        }
        if (draft == null) return
        val zones = draft.optJSONArray("zones") ?: JSONArray()
        val pixelCount = draft.optInt("pixels", 0)
        val assigned = (0 until zones.length()).sumOf { zones.optJSONObject(it)?.optInt("pixels", 0) ?: 0 }
        Panel(t) {
            Text("Output " + (index + 1), color = t.fg, fontWeight = FontWeight.Bold)
            Field(draft.optString("name"), "Output name", t) { value ->
                edit { it.put("name", value) }
            }
            Caption("Pixel allocation (1–623; temporary firmware limit)", t)
            Field(pixelCount.toString(), "Pixel count", t) { value ->
                edit { it.put("pixels", value.toIntOrNull() ?: -1) }
            }
            ValueSlider("Pixels", pixelCount.coerceIn(1, 623), 623, t) { n ->
                edit { it.put("pixels", n.coerceAtLeast(1)) }
            }
            Caption("Color order · set this for the actual connected LED strips", t)
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                BleConfigCodec.ORDERS.forEach { order ->
                    Pill(order, t, order == draft.optString("color_order")) {
                        edit { it.put("color_order", order) }
                    }
                }
            }
        }
        var cursor = 0
        for (i in 0 until zones.length()) {
            val zone = zones.optJSONObject(i) ?: continue
            val from = cursor
            val count = zone.optInt("pixels", 0)
            cursor += count
            Panel(t) {
                Text("Zone " + (i + 1), color = t.fg, fontWeight = FontWeight.SemiBold)
                Caption("Start " + from + " · end " + (if (count > 0) cursor - 1 else "empty") +
                    " · next " + cursor, t)
                Field(zone.optString("name"), "Zone name", t) { value ->
                    edit { it.getJSONArray("zones").getJSONObject(i).put("name", value) }
                }
                Field(count.toString(), "Zone pixel count", t) { value ->
                    edit { it.getJSONArray("zones").getJSONObject(i)
                        .put("pixels", value.toIntOrNull() ?: -1) }
                }
                Caption("Lighting role (only CORE assigns trigger priority)", t)
                Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    BleConfigCodec.ROLES.forEach { role ->
                        Pill(role.replace('_', ' '), t, zone.optString("role") == role) {
                            edit { it.getJSONArray("zones").getJSONObject(i).put("role", role) }
                        }
                    }
                }
                if (zones.length() > 1) Pill("Remove zone", t) {
                    edit {
                        val next = JSONArray()
                        val original = it.getJSONArray("zones")
                        for (j in 0 until original.length()) if (j != i) next.put(original.get(j))
                        it.put("zones", next)
                    }
                }
            }
        }
        Panel(t) {
            Caption("Assigned " + assigned + " of " + pixelCount + " pixels · " +
                (pixelCount - assigned) + " remaining. Zone offsets are zero-based and automatically contiguous.", t)
            if (zones.length() < 8) Pill("+ Add zone", t) {
                edit {
                    it.getJSONArray("zones").put(JSONObject()
                        .put("name", "Zone " + (zones.length() + 1))
                        .put("pixels", 0).put("role", "off"))
                }
            }
            Pill("Fit last zone to remaining pixels", t) {
                edit {
                    val group = it.getJSONArray("zones")
                    if (group.length() > 0) {
                        val other = (0 until group.length() - 1)
                            .sumOf { j -> group.getJSONObject(j).getInt("pixels") }
                        val remainder = it.getInt("pixels") - other
                        if (remainder in 0..623)
                            group.getJSONObject(group.length() - 1).put("pixels", remainder)
                    }
                }
            }
            val calibrated = draft.optJSONObject("calibration")
            val gamma = calibrated?.optBoolean("gamma_enabled", false) ?: false
            Pill(if (gamma) "Gamma enabled ✓" else "Gamma disabled", t, gamma) {
                edit {
                    val cal = it.optJSONObject("calibration") ?: JSONObject()
                    cal.put("gamma_enabled", !gamma)
                    it.put("calibration", cal)
                }
            }
            var validationError = ""
            val valid = try {
                BleConfigCodec.encode(draft)
                true
            } catch (ex: Exception) {
                validationError = ex.message ?: "Invalid output mapping"
                false
            }
            Caption(if (valid) "Ready to save; no changes sent yet." else
                "Cannot save: " + validationError, t)
            if (valid) Pill("Save to CORE · verify readback", t, true) { confirm = true }
        }
        if (confirm) AlertDialog(
            onDismissRequest = { confirm = false },
            title = { Text("Apply saved hardware output settings?") },
            text = { Text("Stop all shows and release physical inputs before applying. Changes affect only output " +
                (index + 1) + ". CORE must acknowledge flash persistence and return identical BLE readback.") },
            confirmButton = { Pill("Save", t, true) {
                try {
                    BleConfigCodec.encode(JSONObject(draftText))
                    if (ble.canConfigure()) {
                        ble.saveOutput(index, draftText)
                        outputMessage.value = "Save requested · verifying hardware readback"
                    } else outputMessage.value = "Not authorized — no settings sent"
                } catch (ex: Exception) {
                    outputMessage.value = "Save rejected: " + (ex.message ?: "invalid draft")
                }
                confirm = false
            } },
            dismissButton = { Pill("Cancel", t) { confirm = false } }
        )
    }
}
