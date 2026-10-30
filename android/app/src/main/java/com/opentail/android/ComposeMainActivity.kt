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
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import kotlinx.coroutines.delay
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.cos
import kotlin.math.sin

/**
 * Kotlin + Jetpack Compose migration candidate. The Java v4 document model,
 * BLE GATT transport and codec stay available without conversion or WebView.
 * Strictly local authoring; only explicit, confirmed Settings controls send BLE.
 *
 * Sole Android launcher. On-phone visual and BLE acceptance are release gates,
 * not reasons to ship a second, divergent UI implementation.
 */
class ComposeMainActivity : ComponentActivity() {
    private val pages = listOf("Home", "Show Modes", "Layout", "Triggers", "Auto Events", "Settings")
    private lateinit var project: NativeProject
    private lateinit var ble: BleDiscovery
    private val revision = mutableIntStateOf(0)
    // Import/preset restoration replaces the local document; ordinary saves
    // must not reset in-progress TextField values (including typed spaces).
    private val projectLoadRevision = mutableIntStateOf(0)
    private val bleMessage = mutableStateOf("Disconnected")
    private val supportedIds = mutableStateOf(setOf<Int>())
    private val coreTriggerMapping = mutableStateOf<NativeTriggerMappingCodec.Snapshot?>(null)
    private val foundDevices = mutableStateMapOf<String, String>()
    private val outputs = mutableStateMapOf<Int, JSONObject>()
    private val outputMessage = mutableStateOf("No device output read yet")
    private val paletteTransferMessage = mutableStateOf("CORE palette v3 not negotiated")
    private val paletteV3Supported = mutableStateOf(false)
    private val paletteLoadedSlot = mutableIntStateOf(-1)
    private val paletteLoadedRevision = mutableLongStateOf(-1L)
    private val paletteLoadedStops = mutableStateOf("")
    private val paletteLoadedPresent = mutableStateOf(false)
    private val prefs by lazy { getSharedPreferences("opentail_native_ui_v1", Context.MODE_PRIVATE) }
    private lateinit var livePreview: LiveBlePreview
    private val liveArmed = mutableStateOf(false)
    private val offlinePreview = mutableStateOf(false)
    private val selectedEffect = mutableIntStateOf(0)
    private val liveChainMask = mutableIntStateOf(3)
    private val liveZoneMask = mutableIntStateOf(255)
    private val coreFrame = mutableStateOf<ExactLedFrame?>(null)
    private val captureMessage = mutableStateOf("CORE render snapshot not captured")
    private val sequenceMessage = mutableStateOf("")
    private val captureOutput = mutableIntStateOf(0)
    // A verified snapshot is valid only for the UI state and output at the
    // time its capture began. Old asynchronous GATT replies cannot resurrect
    // pixels after an edit, disconnect, or switch into offline preview.
    private var frameRevision = 0L
    private var pendingFrameRevision = -1L
    private var pendingFrameOutput = -1

    private fun invalidateCoreFrame() {
        frameRevision++
        pendingFrameRevision = -1L
        pendingFrameOutput = -1
        coreFrame.value = null
        captureMessage.value = "CORE frame not captured"
    }

    private fun enterOfflinePreview() {
        if (liveArmed.value) {
            livePreview.stop()
            liveArmed.value = false
        }
        offlinePreview.value = true
        invalidateCoreFrame()
    }

    // A single gesture updates the on-screen design immediately. When the
    // owner has explicitly armed a physical bench preview, the same gesture
    // also coalesces an ephemeral BLE FX update to the already-saved zones.
    private fun previewCurrentEffect(effectOverride: Int? = null) {
        invalidateCoreFrame() // any parameter change invalidates the frozen CORE frame
        if (offlinePreview.value) return // Settings Preview Offline cannot send BLE edits
        // Live editing is the DEFAULT. Offline rendering always updates;
        // if owner-authorized BLE can preview this effect, stream the gesture
        // automatically without an extra "Live Edit Mode" switch.
        val effectId = effectOverride ?: prefs.getInt("effectId", 0)
        if (!ble.canControl() || supportedIds.value.isEmpty()) return
        val paletteId = prefs.getInt("paletteId", 0)
        // Preflight BEFORE arming or coalescing a BLE packet. Otherwise
        // changing the effect, speed or color could silently replay palette
        // zero while the screen still shows an unsupported selected palette.
        if (effectId !in supportedIds.value ||
            !NativePaletteEngine.coreSupports(effectId, paletteId)) {
            if (liveArmed.value) {
                livePreview.stop()
                liveArmed.value = false
            }
            bleMessage.value = if (effectId !in supportedIds.value)
                "Selected effect is unavailable on this CORE; design stays local"
            else "Selected palette is unavailable for this CORE effect; design stays local"
            return
        }
        if (!livePreview.isArmed()) {
            if (!livePreview.arm()) return
            liveArmed.value = true
        }
        livePreview.update(LiveBlePreview.Frame(
            effectId = effectId,
            chainMask = liveChainMask.intValue,
            zoneMask = liveZoneMask.intValue,
            foreground = prefs.getInt("foreground", -1),
            background = prefs.getInt("background", 0xff000000.toInt()),
            brightness = prefs.getInt("brightness", 160),
            speed = prefs.getInt("speed", 128),
            intensity = prefs.getInt("intensity", 128),
            paletteId = NativePaletteEngine.corePreviewPalette(effectId, paletteId)
        ))
    }

    private val importer = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            try {
                val input = contentResolver.openInputStream(uri)
                    ?: throw java.io.IOException("Cannot open selected document")
                val data = input.bufferedReader().use { reader -> NativeProjectDocumentIo.readBounded(reader) }
                val imported = project.importJson(data)
                outputMessage.value = if (imported) "Design imported" else "Invalid v4 project"
                if (imported) refreshLoadedProject()
            } catch (e: Exception) { outputMessage.value = "Import failed: " + (e.message ?: "invalid file") }
        }
    }
    private val exporter = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        if (uri != null) {
            try {
                val output = contentResolver.openOutputStream(uri)
                    ?: throw java.io.IOException("Cannot open output document")
                output.bufferedWriter().use { it.write(project.exportJson()) }
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
        selectedEffect.intValue = prefs.getInt("effectId", 0)
        ble = BleDiscovery(this) { status -> runOnUiThread {
            bleMessage.value = status
            if (status.contains("OK AUTH") && ble.canControl())
                ble.readFxCapabilities()
            if (!ble.canControl()) {
                invalidateCoreFrame()
                supportedIds.value = emptySet() // never reuse another CORE's FX capability list
                coreTriggerMapping.value = null // never reuse another controller's physical input mapping
                outputs.clear() // readback from a disconnected/unauthorized controller is not current
                paletteV3Supported.value = false
                paletteLoadedSlot.intValue = -1
                paletteLoadedRevision.longValue = -1L
                paletteLoadedStops.value = ""
                paletteLoadedPresent.value = false
            }
            if (!ble.canControl() && liveArmed.value) {
                livePreview.stop()
                liveArmed.value = false
            }
        } }
        livePreview = LiveBlePreview(ble, { supportedIds.value }) { message ->
            runOnUiThread { bleMessage.value = message }
        }
        ble.setFrameListener { output, sequence, rgb, error ->
            runOnUiThread {
                if (pendingFrameRevision != frameRevision ||
                    pendingFrameOutput != output || offlinePreview.value ||
                    !ble.canCaptureFrame()) return@runOnUiThread
                pendingFrameRevision = -1L
                pendingFrameOutput = -1
                if (error != null || rgb == null) {
                    coreFrame.value = null
                    captureMessage.value = error ?: "No CORE frame returned"
                } else {
                    runCatching { ExactLedFrame.fromCoreRgb888(output, sequence, rgb) }
                        .onSuccess {
                            coreFrame.value = it
                            captureMessage.value = "Verified CORE RGB888 snapshot · output " +
                                (output + 1) + " · frame " + sequence + " · " + it.size() + " LEDs"
                        }
                        .onFailure {
                            coreFrame.value = null
                            captureMessage.value = "Invalid CORE RGB888 frame"
                        }
                }
            }
        }
        ble.setScanResultListener { address, label ->
            runOnUiThread { foundDevices[address] = label }
        }
        ble.setTriggerMapListener { mapping ->
            runOnUiThread { coreTriggerMapping.value = mapping }
        }
        ble.setFxCapabilitiesListener { data ->
            try {
                val list = JSONArray(data)
                supportedIds.value = (0 until list.length()).map { list.optInt(it) }.toSet()
            } catch (_: Exception) { supportedIds.value = emptySet() }
        }
        ble.setPaletteListener { result ->
            runOnUiThread {
                paletteTransferMessage.value = result.message
                paletteV3Supported.value = result.capable && ble.canControl()
                if (!result.ok) {
                    paletteLoadedSlot.intValue = -1 // failed/ambiguous save requires new QUERY
                    paletteLoadedRevision.longValue = -1L
                } else if (result.index in 0..127) {
                    paletteLoadedSlot.intValue = result.index
                    paletteLoadedRevision.longValue = result.revision
                    paletteLoadedPresent.value = result.present
                    paletteLoadedStops.value = if (result.stops != null)
                        NativePaletteV3.formatStops(result.stops) else ""
                }
            }
        }
        ble.setConfigListener { raw ->
            try {
                val response = JSONObject(raw)
                outputMessage.value = response.optString("message", "CORE output result")
                val index = response.optInt("index", -1)
                if (response.optBoolean("ok") && index in 0..1)
                    response.optJSONObject("output")?.let {
                        outputs[index] = JSONObject(it.toString())
                        invalidateCoreFrame() // the saved/readback output may have changed
                    }
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
    override fun onStop() {
        ble.cancelPaletteTransfer() // no further NVS mutation after backgrounding
        ble.cancelFrameCapture()
        invalidateCoreFrame()
        if (liveArmed.value) {
            livePreview.stop()
            liveArmed.value = false
        }
        ble.stopScan()
        super.onStop()
    }
    override fun onDestroy() {
        unregisterReceiver(adapterReceiver)
        if (liveArmed.value) livePreview.stop()
        ble.shutdown()
        super.onDestroy()
    }
    private fun refresh() {
        invalidateCoreFrame() // local design changes invalidate an older CORE snapshot
        revision.intValue++
    }
    private fun refreshLoadedProject() {
        projectLoadRevision.intValue++
        refresh()
    }

    private data class Tone(val bg: Color, val surface: Color, val raised: Color,
                            val fg: Color, val muted: Color, val accent: Color, val red: Color)
    private fun tone(light: Boolean): Tone = if (light)
        Tone(Color.White, Color(0xFFF4F4F4), Color(0xFFE6E6E6),
             Color(0xFF181818), Color(0xFF606060), Color(0xFF242424), Color(0xFFBD3842))
    else Tone(Color.Black, Color(0xFF101010), Color(0xFF1B1B1B),
              Color(0xFFF5F5F5), Color(0xFFA7A7A7), Color.White, Color(0xFFED6472))

    @Composable
    private fun OpenTailShell() {
        val changed = revision.intValue // observe authoritative model revisions
        var page by rememberSaveable { mutableIntStateOf(0) }
        val pageScroll = remember(page) { ScrollState(0) } // new destination always starts at its header
        var light by remember { mutableStateOf(prefs.getBoolean("whiteMode", false)) }
        val t = remember(light) { tone(light) }
        val scheme = remember(light) {
            if (light) lightColorScheme(
                primary = t.accent, onPrimary = t.bg, background = t.bg,
                onBackground = t.fg, surface = t.surface, onSurface = t.fg,
                surfaceVariant = t.raised, onSurfaceVariant = t.fg, error = t.red
            ) else darkColorScheme(
                primary = t.accent, onPrimary = t.bg, background = t.bg,
                onBackground = t.fg, surface = t.surface, onSurface = t.fg,
                surfaceVariant = t.raised, onSurfaceVariant = t.fg, error = t.red
            )
        }
        MaterialTheme(colorScheme = scheme) {
        Column(Modifier.fillMaxSize().background(t.bg)
            .windowInsetsPadding(WindowInsets.safeDrawing).padding(top = 14.dp)) {
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
                    NavigationPill(i, title, t, page == i) {
                        if (i != 1) offlinePreview.value = false
                        page = i
                    }
                }
            }
            Column(Modifier.fillMaxSize().verticalScroll(pageScroll)
                .padding(horizontal = 16.dp, vertical = 8.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp)) {
                when (page) {
                    0 -> HomePage(t, { page = it })
                    1 -> ShowModesPage(t)
                    2 -> LayoutPage(t)
                    3 -> TriggersPage(t)
                    4 -> AutoEventsPage(t) { effectId ->
                        selectedEffect.intValue = effectId
                        prefs.edit().putInt("effectId", effectId).apply()
                        enterOfflinePreview()
                        page = 1
                    }
                    5 -> SettingsPage(t) { page = it }
                }
                Spacer(Modifier.height(24.dp))
            }
        }
        }
    }
    @Composable
    private fun Pill(label: String, t: Tone, selected: Boolean = false, action: () -> Unit) {
        // Selected surfaces are neutral graphite, not colored brand accents.
        val fill = if (selected) (if (t.bg == Color.Black) Color(0xFF303030)
            else Color(0xFFDEDEDE)) else t.raised
        Box(Modifier.heightIn(min = 44.dp).clip(RoundedCornerShape(99.dp))
                .background(fill)
                .clickable(onClick = action).padding(horizontal = 15.dp, vertical = 10.dp),
            contentAlignment = Alignment.Center) {
            Text(label, fontSize = 13.sp, fontWeight = FontWeight.SemiBold,
                 color = t.fg, maxLines = 1)
        }
    }
    // Native vector glyphs preserve the approved icon meanings without
    // emoji, downloaded fonts, WebView or an additional icon dependency.
    @Composable
    private fun NavigationPill(index: Int, label: String, t: Tone,
                               selected: Boolean, action: () -> Unit) {
        val fg = t.fg
        val fill = if (selected) (if (t.bg == Color.Black) Color(0xFF303030)
            else Color(0xFFDEDEDE)) else t.bg
        Row(Modifier.heightIn(min = 46.dp).clip(RoundedCornerShape(99.dp))
                .background(fill)
                .border(1.dp, if(selected) t.muted.copy(alpha = 0.7f)
                        else t.muted.copy(alpha = 0.25f), RoundedCornerShape(99.dp))
                .clickable(onClick = action).padding(horizontal = 14.dp, vertical = 11.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.CenterVertically) {
            if (index in 1..4) NavigationGlyph(index, fg)
            Text(label, fontSize = 13.sp, fontWeight = FontWeight.SemiBold,
                color = fg, maxLines = 1)
        }
    }
    @Composable
    private fun NavigationGlyph(index: Int, color: Color) {
        Canvas(Modifier.size(17.dp)) {
            val u = size.minDimension / 24f
            when (index) {
                1 -> { // Show Modes: sparkle
                    val sparkle = Path().apply {
                        moveTo(12*u, 1*u); lineTo(15*u, 9*u)
                        lineTo(23*u, 12*u); lineTo(15*u, 15*u)
                        lineTo(12*u, 23*u); lineTo(9*u, 15*u)
                        lineTo(1*u, 12*u); lineTo(9*u, 9*u); close()
                    }
                    drawPath(sparkle, color)
                }
                2 -> { // Layout: four squares
                    for (row in 0..1) for (column in 0..1)
                        drawRect(color,
                            topLeft = Offset((2+column*11)*u, (2+row*11)*u),
                            size = androidx.compose.ui.geometry.Size(9*u, 9*u),
                            style = Stroke(2*u))
                }
                3 -> { // Triggers: lightning
                    val bolt = Path().apply {
                        moveTo(13*u, 1*u); lineTo(4*u, 13*u)
                        lineTo(11*u, 13*u); lineTo(9*u, 23*u)
                        lineTo(21*u, 9*u); lineTo(14*u, 9*u); close()
                    }
                    drawPath(bolt, color)
                }
                4 -> { // Auto Events: sun
                    drawCircle(color, 4.5f*u, Offset(12*u, 12*u),
                        style = Stroke(2*u))
                    for (ray in 0 until 8) {
                        val angle = ray * Math.PI / 4.0
                        val x = cos(angle).toFloat()
                        val y = sin(angle).toFloat()
                        drawLine(color, Offset((12+8*x)*u, (12+8*y)*u),
                            Offset((12+11*x)*u, (12+11*y)*u),
                            strokeWidth = 2*u)
                    }
                }
            }
        }
    }
    /** Preserve the single white Connect action from the approved UI. */
    @Composable
    private fun ConnectPill(onClick: () -> Unit) {
        Box(Modifier.heightIn(min = 48.dp).clip(RoundedCornerShape(99.dp))
            .background(Color.White)
            .clickable(onClick = onClick).padding(horizontal = 22.dp, vertical = 12.dp),
            contentAlignment = Alignment.Center) {
            Text("Connect", color = Color.Black, fontSize = 14.sp,
                fontWeight = FontWeight.SemiBold)
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
    private fun Field(value: String, hint: String, t: Tone,
                      modifier: Modifier = Modifier.fillMaxWidth(), onChange: (String) -> Unit) {
        BasicTextField(
            value = value, onValueChange = onChange, singleLine = true,
            cursorBrush = SolidColor(t.accent),
            textStyle = TextStyle(color = t.fg, fontSize = 14.sp),
            modifier = modifier.heightIn(min = 48.dp).clip(RoundedCornerShape(99.dp))
                .background(t.raised).padding(horizontal = 15.dp, vertical = 13.dp),
            decorationBox = { inner ->
                Box {
                    if (value.isEmpty()) Text(hint, fontSize = 14.sp, color = t.muted)
                    inner()
                }
            }
        )
    }
    // Shared soft-capsule slider for all editors. Material3 retains keyboard,
    // accessibility and touch gestures; only the thumb/track are styled.
    @OptIn(ExperimentalMaterial3Api::class)
    @Composable
    private fun CapsuleSlider(value: Int, min: Int, max: Int, t: Tone,
                              change: (Int) -> Unit, finished: (() -> Unit)? = null) {
        val upper = max.coerceAtLeast(min + 1)
        Slider(
            value = value.coerceIn(min, upper).toFloat(),
            onValueChange = { change(it.toInt().coerceIn(min, upper)) },
            onValueChangeFinished = finished,
            valueRange = min.toFloat()..upper.toFloat(),
            colors = SliderDefaults.colors(
                thumbColor = t.accent, activeTrackColor = t.accent,
                inactiveTrackColor = t.raised
            ),
            thumb = {
                Box(Modifier.size(22.dp)
                    .shadow(5.dp, RoundedCornerShape(99.dp),
                            ambientColor = t.accent.copy(alpha = 0.25f),
                            spotColor = t.accent.copy(alpha = 0.25f))
                    .clip(RoundedCornerShape(99.dp)).background(t.accent))
            },
            track = { state ->
                val progress = ((state.value - min) / (upper - min)).coerceIn(0f, 1f)
                Box(Modifier.fillMaxWidth().height(12.dp)
                    .clip(RoundedCornerShape(99.dp)).background(t.raised)) {
                    Box(Modifier.fillMaxHeight().fillMaxWidth(progress)
                        .clip(RoundedCornerShape(99.dp)).background(t.accent))
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
        CapsuleSlider(value, 0, max, t, change)
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
            Field(hex, "#RRGGBB", t, Modifier.weight(1f)) { entered ->
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
    // Pure native designer control. Uses the same neutral pills, search field,
    // and OLED/white colors as the locked interface. Never invent CORE pixels.
    @Composable
    private fun PalettePicker(label: String, value: Int, t: Tone, onSelect: (Int) -> Unit) {
        var browsing by rememberSaveable(label) { mutableStateOf(false) }
        var search by rememberSaveable(label) { mutableStateOf("") }
        val chosen = NativePaletteEngine.lookup(project.raw(), value)
        Caption(label + " · " + chosen.name + " (#" + chosen.id + ")", t)
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            PaletteSwatch(chosen, t)
            Pill(if (browsing) "Close palettes" else "Browse palettes", t) {
                browsing = !browsing
            }
        }
        if (browsing) {
            Field(search, "Search palette name or ID", t) { search = it.take(64) }
            val matching = remember(search, revision.intValue) {
                (0..NativePaletteEngine.MAX_ID).filter { id ->
                    val name = NativePaletteEngine.lookup(project.raw(), id).name
                    name.contains(search.trim(), ignoreCase = true) ||
                        id.toString() == search.trim()
                }
            }
            LazyColumn(Modifier.fillMaxWidth().height(228.dp),
                verticalArrangement = Arrangement.spacedBy(5.dp)) {
                items(matching, key = { it }) { id ->
                    val palette = NativePaletteEngine.lookup(project.raw(), id)
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically) {
                        Text(palette.name + " · " + id, color = t.fg, fontSize = 13.sp,
                            maxLines = 1, modifier = Modifier.weight(1f))
                        PaletteSwatch(palette, t)
                        Pill(if (value == id) "Selected" else "Select", t, value == id) {
                            onSelect(id)
                            browsing = false
                        }
                    }
                }
            }
        }
        Caption("Only palettes with supplied fixed RGB stops show a gradient. " +
            "Unknown or dynamic palettes show Preview unavailable; the LED strip " +
            "still requires an actual CORE render frame.", t)
    }
    @Composable
    private fun PaletteSwatch(palette: NativePalette, t: Tone) {
        val stops = palette.stops
        if (stops == null) {
            Text("Preview unavailable", color = t.muted, fontSize = 11.sp,
                maxLines = 1)
        } else {
            Canvas(Modifier.width(92.dp).height(27.dp)
                .clip(RoundedCornerShape(12.dp)).background(t.raised)) {
                drawRect(brush = Brush.horizontalGradient(*stops.toTypedArray()))
            }
        }
    }

    @Composable
    private fun PixelStrip(t: Tone, segments: List<Pair<Int, Color>>) {
        val length = segments.sumOf { it.first.coerceAtLeast(0) }.coerceAtLeast(1)
        Canvas(Modifier.fillMaxWidth().height(28.dp).clip(RoundedCornerShape(12.dp))
            .background(t.raised)) {
            var used = 0f
            segments.forEach { (pixels, color) ->
                val width = size.width * pixels.coerceAtLeast(0) / length
                if (width > 0f) {
                    drawLine(color,
                        Offset(used + 1f, size.height * 0.5f),
                        Offset((used + width - 1f).coerceAtLeast(used + 1f),
                            size.height * 0.5f),
                        strokeWidth = size.height * 0.75f)
                }
                used += width
            }
        }
    }
    @Composable
    private fun ZoneDotPreview(t: Tone, count: Int, reverse: Boolean) {
        // Neutral, indexed LED-address preview. Never invent effect animation.
        val dots = count.coerceIn(0, 30)
        Canvas(Modifier.fillMaxWidth().height(34.dp)
            .clip(RoundedCornerShape(99.dp)).background(t.raised)) {
            val pitch = size.width / 31f
            for(i in 0 until dots){
                val x = (if (reverse) 30-i else i) * pitch + pitch/2f
                val center = Offset(x,size.height/2f)
                drawCircle(t.fg.copy(alpha = 0.10f),radius=8f,center=center)
                drawCircle(t.fg.copy(alpha = 0.66f),radius=4.2f,center=center)
            }
        }
    }

    private fun rolePixelColor(role: String): Color = when(role) {
        "park_brake", "parking", "brake" -> Color(0xFFEE3939)
        "left_turn", "right_turn" -> Color(0xFFFFB300)
        "reverse" -> Color.White
        "aux1", "aux2", "aux3" -> Color(0xFFB083F5)
        "decorative" -> Color(0xFF9D9D9D)
        else -> Color(0xFF333333)
    }

    @Composable
    private fun HomePage(t: Tone, changePage: (Int) -> Unit) {
        Title("Home", t)
        Panel(t) {
            Box(Modifier.clip(RoundedCornerShape(99.dp)).background(t.raised)
                .padding(horizontal = 14.dp, vertical = 10.dp)) {
                Text("Bluetooth · " + ble.connectionPhase(), color = t.fg, fontSize = 13.sp)
            }
            Caption(bleMessage.value, t)
            Pill("Authorize / Pair with BOOT", t) { ble.authorizeOrPair() }
            Caption("Connect in Settings first. BOOT is required only for first-time enrollment.", t)
        }
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
            Caption("All editors update the local preview instantly. Owner-authorized BLE also previews supported effects transiently; physical geometry still requires explicit controller save.", t)
        }
    }
    @Composable
    private fun EffectsPreview(t: Tone, effect: Int) {
        // Every segment represents exactly one real zero-based LED index.
        // No fabricated effect motion, fixed 28-dot count, or frame sampling.
        val revisionKey = revision.intValue
        val localFrame = remember(revisionKey) {
            runCatching { ExactLedFrame.fromProjectModel(project) }.getOrNull()
        }
        val frame = coreFrame.value ?: localFrame
        val isCore = frame?.origin == ExactLedFrame.Origin.CORE
        val total = frame?.size() ?: 0
        var visiblePixels by rememberSaveable { mutableIntStateOf(32) }
        val window = total.coerceAtMost(visiblePixels)
        var requestedFirst by rememberSaveable { mutableIntStateOf(0) }
        val first = requestedFirst.coerceIn(0, (total - window).coerceAtLeast(0))
        Caption((if (isCore) "Verified CORE-rendered RGB888 frame · output " +
            (frame!!.output + 1) + " · sequence " + frame.frameNumber
        else "Exact local pixel map · " + total + " indexed design LEDs"), t)
        if (frame == null) {
            Caption("Pixel map exceeds preview capacity; check manual zone offsets.", t)
        } else if (total == 0) {
            Caption("Add zones in Layout to inspect individual pixel addresses.", t)
        } else {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp),
                verticalAlignment = Alignment.CenterVertically) {
                Text("Zoom", color = t.muted, fontSize = 12.sp)
                listOf(8, 16, 32, 64).forEach { pixels ->
                    Pill(pixels.toString(), t, visiblePixels == pixels) { visiblePixels = pixels }
                }
            }
            Canvas(Modifier.fillMaxWidth().height(64.dp)
                .clip(RoundedCornerShape(14.dp)).background(t.raised)) {
                val pitch = size.width / window.toFloat()
                for (i in 0 until window) {
                    val x = i * pitch
                    drawRect(Color(frame.colorAt(first + i)), topLeft = Offset(x, 9f),
                        size = androidx.compose.ui.geometry.Size(
                            (pitch - 1f).coerceAtLeast(0.5f),
                            (size.height - 18f).coerceAtLeast(1f)))
                }
            }
            Caption("LED " + first + "–" + (first + window - 1) +
                " · one display segment per real LED index · black = unpainted", t)
            if (total > window)
                ValueSlider("First visible pixel", first, total - window, t) {
                    requestedFirst = it
                }
        }
        Caption(if (isCore) "Real CORE post-render logical RGB888 for ONE captured frame. " +
            "This does not certify continuous animation timing or optical LED output."
        else "Effect " + NativeEffectCatalog.byId(effect).name +
            " · live animation unavailable without CORE frame capture. " +
            "The strip above shows the pixel-address map, NOT a simulated effect.", t)
    }

    @Composable
    private fun ShowModesPage(t: Tone) {
        Title("Show Modes", t)
        var view by rememberSaveable { mutableStateOf("Effects") }
        var query by rememberSaveable { mutableStateOf("") }
        val effect = selectedEffect.intValue
        var speed by rememberSaveable { mutableIntStateOf(prefs.getInt("speed", 128)) }
        var intensity by rememberSaveable { mutableIntStateOf(prefs.getInt("intensity", 128)) }
        var brightness by rememberSaveable { mutableIntStateOf(prefs.getInt("brightness", 160)) }
        var foreground by rememberSaveable { mutableIntStateOf(prefs.getInt("foreground", -1)) }
        var background by rememberSaveable { mutableIntStateOf(prefs.getInt("background", 0xff000000.toInt())) }
        var accent by rememberSaveable { mutableIntStateOf(prefs.getInt("accent", -1)) }
        var paletteId by rememberSaveable {
            mutableIntStateOf(prefs.getInt("paletteId", 0).coerceIn(0, NativePaletteEngine.MAX_ID))
        }
        Panel(t) {
            Caption(if (offlinePreview.value)
                "Offline preview · design changes stay local until you leave Show Modes."
            else "Pixel-exact design inspection · effects require CORE rendered frames", t)
            EffectsPreview(t, effect)
            if (!offlinePreview.value && ble.canCaptureFrame()) {
                Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                    Pill("OUT 1", t, captureOutput.intValue == 0) {
                        captureOutput.intValue = 0; invalidateCoreFrame()
                    }
                    Pill("OUT 2", t, captureOutput.intValue == 1) {
                        captureOutput.intValue = 1; invalidateCoreFrame()
                    }
                    Pill("Capture actual CORE frame", t, true) {
                        invalidateCoreFrame()
                        if (ble.captureFrame(captureOutput.intValue)) {
                            pendingFrameRevision = frameRevision
                            pendingFrameOutput = captureOutput.intValue
                            captureMessage.value = "Capturing next completed CORE RGB frame..."
                        } else {
                            captureMessage.value = "CORE frame unavailable or BLE busy; retry"
                        }
                    }
                }
                Caption(captureMessage.value, t)
            } else Caption(if (offlinePreview.value)
                "CORE capture is disabled in offline preview mode."
            else "CORE frame capture unavailable until firmware supports " +
                "the authenticated snapshot capability. Local pixel map remains available.", t)
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
                    PalettePicker("Palette", paletteId, t) { id ->
                        paletteId = id
                        prefs.edit().putInt("paletteId", id).apply()
                        previewCurrentEffect(effect) // one safety gate for every edit gesture
                    }
                    ValueSlider("Speed", speed, 255, t) { speed = it; prefs.edit().putInt("speed", it).apply(); previewCurrentEffect(effect) }
                    ValueSlider("Intensity", intensity, 255, t) { intensity = it; prefs.edit().putInt("intensity", it).apply(); previewCurrentEffect(effect) }
                    ValueSlider("Brightness", brightness, 255, t) { brightness = it; prefs.edit().putInt("brightness", it).apply(); previewCurrentEffect(effect) }
                    PaletteEditor("Foreground", foreground, t) {
                        foreground = it; prefs.edit().putInt("foreground", it).apply(); previewCurrentEffect(effect)
                    }
                    PaletteEditor("Background", background, t) {
                        background = it; prefs.edit().putInt("background", it).apply(); previewCurrentEffect(effect)
                    }
                    PaletteEditor("Accent", accent, t) {
                        accent = it; prefs.edit().putInt("accent", it).apply(); previewCurrentEffect(effect)
                    }
                    Caption(if (offlinePreview.value)
                        "Offline mode: edits update only this local design; no CORE LED commands."
                    else "Supported color/speed/intensity edits preview over authorized BLE. " +
                        "CORE accepts default palette 0 and Embers palettes 2–4 over BLE. " +
                        "Other palette choices stay local; exact animation still requires " +
                        "a CORE frame snapshot.", t)
                }
                // Only compose visible effect rows; never eagerly render 140
                // cards inside the parent scroll (expensive on entry devices).
                val matches = remember(query) {
                    NativeEffectCatalog.all().filter {
                        it.name.contains(query, ignoreCase = true)
                    }
                }
                LazyColumn(Modifier.fillMaxWidth().heightIn(max = 440.dp),
                    verticalArrangement = Arrangement.spacedBy(7.dp)) {
                    items(matches, key = { it.id }) { item ->
                        Panel(t) {
                            Pill(item.name, t, item.id == effect) {
                                selectedEffect.intValue = item.id
                                prefs.edit().putInt("effectId", item.id).apply()
                                previewCurrentEffect(item.id)
                            }
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
        // NativeProject contains plain JSON, not Compose State: read the
        // revision here so toggles and per-step palette saves redraw this
        // editor even when its stable parameters have not changed.
        val modelRevision = revision.intValue
        val steps = remember(modelRevision) { project.sequence() }
        val opts = remember(modelRevision) { project.sequenceOptions() }
        var editingStep by remember { mutableIntStateOf(-1) }
        var editedDuration by remember { mutableStateOf("") }
        var editingPaletteStep by remember { mutableIntStateOf(-1) }
        if (sequenceMessage.value.isNotEmpty()) Caption(sequenceMessage.value, t)
        Panel(t) {
            Caption("Playback settings are saved in the v4 design; they never command CORE.", t)
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                listOf("Shuffle" to "shuffleLoop", "Repeat" to "repeat",
                    "Shared Palette" to "sharedPaletteEnabled").forEach { (name, key) ->
                    val enabled = opts.optBoolean(key, false)
                    Pill(name + (if (enabled) " ✓" else ""), t, enabled) {
                        if (project.updateSequenceOption(key, !enabled)) {
                            sequenceMessage.value = ""
                            refresh()
                        } else sequenceMessage.value = "Could not save sequence options"
                    }
                }
            }
            if (opts.optBoolean("sharedPaletteEnabled", false)) {
                PalettePicker("Shared sequence palette",
                    opts.optInt("sharedPaletteId", 0), t) { id ->
                    if (project.updateSequenceOption("sharedPaletteId", id)) {
                        sequenceMessage.value = ""
                        refresh()
                    } else sequenceMessage.value = "Could not save shared palette"
                }
                Caption("Shared palette applies to compatible steps when the sequence is " +
                    "deployed. Disabling it restores each step's saved palette.", t)
            }
        }
        Panel(t) {
            Title("Sequence designer", t)
            Caption("Locally saved. No BLE commands are sent.", t)
            Pill("+ Add selected effect", t, true) {
                if (steps.length() < NativeProject.MAX_SEQUENCE) {
                    val edited = sequenceList()
                    edited.add(JSONObject().put("fx", NativeEffectCatalog.byId(selectedEffect).name)
                        .put("ms", 1500)
                        .put("settings", JSONObject()
                            .put("pal", prefs.getInt("paletteId", 0).coerceIn(0, 255))))
                    updateSequence(edited)
                }
            }
        }
        for (i in 0 until steps.length()) {
            val item = steps.optJSONObject(i) ?: continue
            Panel(t) {
                val storedPalette = item.optJSONObject("settings")?.optInt("pal", 0) ?: 0
                val effectivePalette = NativePaletteEngine.effectiveStepPalette(
                    storedPalette, opts.optBoolean("sharedPaletteEnabled", false),
                    opts.optInt("sharedPaletteId", 0))
                Text(item.optString("fx") + " · " + item.optInt("ms", 1500) + " ms", color = t.fg)
                Caption("Palette preference · " +
                    NativePaletteEngine.lookup(project.raw(), effectivePalette).name +
                    " (#" + effectivePalette + ") · applies to palette-compatible effects", t)
                Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                    Pill("↑", t) {
                        editingStep = -1; editingPaletteStep = -1; moveSequence(i, -1)
                    }
                    Pill("↓", t) {
                        editingStep = -1; editingPaletteStep = -1; moveSequence(i, 1)
                    }
                    Pill("Duplicate", t) {
                        editingStep = -1; editingPaletteStep = -1; duplicateSequence(i)
                    }
                    Pill("Edit time", t) {
                        editingPaletteStep = -1
                        editingStep = i
                        editedDuration = item.optInt("ms", 1500).toString()
                    }
                    Pill("Palette", t) {
                        editingStep = -1
                        editingPaletteStep = if (editingPaletteStep == i) -1 else i
                    }
                    Pill("Remove", t) {
                        editingStep = -1; editingPaletteStep = -1; removeSequence(i)
                    }
                }
                if (editingPaletteStep == i) {
                    val configured = item.optJSONObject("settings")?.optInt("pal", 0) ?: 0
                    PalettePicker("Step " + (i + 1) + " palette", configured, t) { id ->
                        val edited = sequenceList()
                        val settings = edited[i].optJSONObject("settings")
                            ?: JSONObject().also { edited[i].put("settings", it) }
                        settings.put("pal", id)
                        updateSequence(edited)
                    }
                    if (opts.optBoolean("sharedPaletteEnabled", false)) {
                        Caption("Shared palette is active; step palette remains saved " +
                            "and returns when the shared option is off.", t)
                    }
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
                        if (updateSequence(updated)) editingStep = -1
                    }
                } },
                dismissButton = { Pill("Cancel", t) { editingStep = -1 } })
        }
    }
    private fun sequenceList(): MutableList<JSONObject> =
        (0 until project.sequence().length()).mapNotNull {
            project.sequence().optJSONObject(it)?.let { entry -> JSONObject(entry.toString()) }
        }.toMutableList()
    private fun updateSequence(list: List<JSONObject>): Boolean {
        val arr = JSONArray(); list.forEach { arr.put(it) }
        val saved = project.setSequence(arr)
        if (saved) {
            sequenceMessage.value = ""
            refresh()
        } else sequenceMessage.value = "Could not save sequence; previous design retained"
        return saved
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
        var pendingLoad by remember { mutableIntStateOf(-1) }
        var presetStatus by remember { mutableStateOf("") }
        Panel(t) {
            Title("Presets", t)
            Caption("These save a local design snapshot, not CORE firmware.", t)
            Pill("+ Save current design", t, true) {
                if (presets.length() < NativeProject.MAX_PRESETS) {
                    // Presets store the design only, not recursive copies of
                    // the entire preset library (which grow on every save).
                    val snapshot = JSONObject(project.exportJson())
                    snapshot.put("savedPresets", JSONArray())
                    val updated = JSONArray(presets.toString())
                    updated.put(JSONObject().put("name", "Preset " + (presets.length() + 1))
                        .put("studio", snapshot.toString()))
                    if (project.setPresets(updated)) {
                        presetStatus = "Preset saved locally"
                        refresh()
                    } else presetStatus = "Could not save preset"
                }
            }
            if (presetStatus.isNotEmpty()) Caption(presetStatus, t)
        }
        for (i in 0 until presets.length()) {
            val p = presets.optJSONObject(i) ?: continue
            Panel(t) {
                Text(p.optString("name", "Preset"), color = t.fg)
                Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                    Pill("Load design", t) { pendingLoad = i }
                    Pill("Remove", t) {
                        val next = JSONArray()
                        for (j in 0 until presets.length()) if (j != i) next.put(presets.opt(j))
                        if (project.setPresets(next)) {
                            presetStatus = ""
                            refresh()
                        } else presetStatus = "Could not remove preset"
                    }
                }
            }
        }
        if (pendingLoad in 0 until presets.length()) {
            AlertDialog(onDismissRequest = { pendingLoad = -1 },
                title = { Text("Replace current design?") },
                text = { Text("Loading this preset replaces the current local project. Export a backup first if needed.") },
                confirmButton = { Pill("Load preset", t, true) {
                    val saved = presets.optJSONObject(pendingLoad)?.optString("studio", "").orEmpty()
                    val restored = runCatching {
                        val design = JSONObject(saved)
                        // Loading one design never deletes or rolls back the
                        // user's independently saved preset collection.
                        design.put("savedPresets", JSONArray(project.presets().toString()))
                        project.importJson(design.toString())
                    }.getOrDefault(false)
                    presetStatus = if (restored) "Preset loaded" else "Preset is invalid or could not be loaded"
                    if (restored) refreshLoadedProject()
                    pendingLoad = -1
                } },
                dismissButton = { Pill("Cancel", t) { pendingLoad = -1 } })
        }
    }
    @Composable
    private fun LayoutPage(t: Tone) {
        Title("Layout", t)
        // Only document replacement reloads this field. Keying it to every
        // keystroke's save revision would erase typed trailing spaces.
        var name by remember(projectLoadRevision.intValue) { mutableStateOf(project.name()) }
        var editingId by remember { mutableStateOf<String?>(null) }
        var deleteId by remember { mutableStateOf<String?>(null) }
        Panel(t) {
            Field(name, "Project name", t) {
                name = it
                if (it.trim().isNotEmpty()) {
                    if (project.name(it)) refresh()
                    else name = project.name() // restore last committed value
                }
            }
            Caption("Project name saves as you type. No separate edit mode.", t)
            Caption(project.zoneCount().toString() + " zones · " + project.pixelCount() + " design pixels", t)
            PixelStrip(t, (0 until project.zoneCount()).map { i ->
                val zone = project.zone(i)
                (zone?.optInt("length", 0) ?: 0) to
                    (if (i % 2 == 0) t.accent else t.fg)
            })
            Caption("Live layout strip · changes follow zone offsets immediately", t)
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
                Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Pill("Edit", t) { editingId = id }
                    Pill("+ Add next", t) { addZoneAfter(i) }
                    Pill("Remove", t) { deleteId = id }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Pill("Move ↑", t) { if (project.moveZone(i, i - 1)) refresh() }
                    Pill("Move ↓", t) { if (project.moveZone(i, i + 1)) refresh() }
                }
                // Approved Layout: the active zone expands INSIDE its card, not
                // a blocking modal. Every valid edit updates the local preview.
                if (editingId == id) {
                    var label by remember(id) { mutableStateOf(z.optString("name")) }
                    var count by remember(id) { mutableStateOf(z.optInt("length").toString()) }
                    var reverse by remember(id) { mutableStateOf(z.optBoolean("reverse")) }
                    var selectedPixel by remember(id) { mutableIntStateOf(0) }
                    fun applyLiveZone(): Boolean {
                        val n = count.toIntOrNull() ?: return false
                        if (n !in 1..10000 || label.trim().isEmpty() ||
                            !project.editZone(id,label,n,reverse)) return false
                        refresh()
                        return true
                    }
                    Caption("Zone name", t)
                    Field(label, "Zone name", t) {
                        label=it
                        applyLiveZone()
                    }
                    Caption("Zone length", t)
                    Field(count, "LED count", t) {
                        count=it.filter(Char::isDigit).take(5)
                        applyLiveZone()
                    }
                    val n=count.toIntOrNull() ?: 0
                    CapsuleSlider(n, 1, 10000, t,
                        change={ count=it.toString() },
                        finished={ applyLiveZone() })
                    Caption(project.previewZoneOffsets(id, n), t)
                    Pill(if(reverse) "Reverse ✓" else "Reverse", t, reverse) {
                        reverse=!reverse
                        applyLiveZone()
                    }
                    Caption("LED preview · indexed dots, not simulated FX", t)
                    ZoneDotPreview(t,n,reverse)
                    val pixelLimit=n.coerceAtLeast(1)-1
                    val activePixel=selectedPixel.coerceIn(0,pixelLimit)
                    ValueSlider("Pixel", activePixel, pixelLimit.coerceAtLeast(1),t) {
                        selectedPixel=it.coerceIn(0,pixelLimit)
                    }
                    val painted=project.pixelColor(id,activePixel)
                    val liveColor=android.graphics.Color.parseColor(painted)
                    PaletteEditor("Pixel " + activePixel,liveColor,t) { color ->
                        val hex=String.format("#%06X",color and 0xFFFFFF)
                        if(project.setPixelColor(id,activePixel,hex))refresh()
                    }
                    Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                        Pill("Clear pixel",t) {
                            if(project.clearPixelColor(id,activePixel))refresh()
                        }
                        Pill("Done",t,true) { if(applyLiveZone())editingId=null }
                    }
                } else {
                    ZoneDotPreview(t,length,z.optBoolean("reverse"))
                }
            }
        }
        // Actual hardware output/section assignment belongs beside Layout.
        // Saving to CORE still requires an explicit authenticated confirmation.
        HardwareOutputEditor(t)
        deleteId?.let { id ->
            AlertDialog(onDismissRequest = { deleteId = null }, title = { Text("Remove this zone?") },
                text = { Text("Only assignments to this zone will be removed.") },
                confirmButton = { Pill("Remove", t, true) {
                    if (project.removeZone(id)) {
                        deleteId = null
                        refresh()
                    }
                } }, dismissButton = { Pill("Cancel", t) { deleteId = null } })
        }
    }
    private fun addZoneAfter(index: Int) {
        if (project.addZoneAt("Zone " + (project.zoneCount() + 1), 30, index + 1))
            refresh()
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
        var triggerMessage by remember { mutableStateOf("") }
        val profiles = project.profiles()
        val names = profiles.names()
        val keys = (0 until (names?.length() ?: 0))
            .mapNotNull { names?.optString(it) }
            .filterNot { it == "Startup" || it == "Shutdown" || it == "Show" }
        Panel(t) {
            Caption("Multiple triggers can be selected and edited together. Nothing on this page sends BLE commands.", t)
            if (triggerMessage.isNotEmpty()) Caption(triggerMessage, t)
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
                Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)) {
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
                if (active) {
                    val previewColor = runCatching {
                        Color(android.graphics.Color.parseColor(profile.optString("color", "#ff0000")))
                    }.getOrDefault(t.accent)
                    Box(Modifier.fillMaxWidth().height(24.dp).clip(RoundedCornerShape(99.dp))
                        .background(previewColor))
                    Caption("Local trigger color swatch only. No simulated effect or physical CORE output.", t)
                }
            }
        }
        if (editing) {
            fun applyTriggerStyle(): Boolean {
                if (selections.isEmpty() ||
                    !Regex("^#[0-9A-Fa-f]{6}$").matches(chosenColor) ||
                    NativeEffectCatalog.all().none { it.name == chosenFx }) return false
                val saved = project.styleTriggers(selections, chosenFx, chosenColor)
                triggerMessage = if (saved) "" else "Could not save trigger styles"
                if (saved) refresh()
                return saved
            }
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
                                Pill(fx.name, t, chosenFx == fx.name) {
                                    chosenFx = fx.name
                                    applyTriggerStyle()
                                }
                            }
                        Caption("Trigger color (hex #RRGGBB)", t)
                        Field(chosenColor, "#ff0000", t) {
                            chosenColor = it.take(7)
                            applyTriggerStyle()
                        }
                        Caption("Changes apply immediately to the local design; DONE only closes this editor.", t)
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
                                if (project.assignMany(selections, zoneId, !checked)) refresh()
                            }
                        }
                    }
                },
                confirmButton = { Pill("Done", t, true) {
                    if (applyTriggerStyle()) editing = false
                } },
                dismissButton = { Pill("Close", t) { editing = false } })
        }
        CoreTriggerMappingPage(t)
    }
    @Composable
    private fun CoreTriggerMappingPage(t: Tone) {
        // BLE connection state is not Compose state. Observe the existing
        // status signal so the optional panel appears/disappears immediately.
        val canMap = remember(bleMessage.value) { ble.canMapTriggers() }
        if (!canMap) return // Older firmware keeps offline authoring intact
        val snapshot = coreTriggerMapping.value
        val names = listOf("Disabled", "Park", "Brake", "Left", "Right",
            "Reverse", "AUX1", "AUX2", "AUX3")
        var draft by remember(snapshot) { mutableStateOf(snapshot?.functions?.toList() ?: emptyList()) }
        var confirm by remember { mutableStateOf(false) }
        Panel(t) {
            Caption("CORE input mapping · authorized BLE v1 · IN1–IN8 are logical functions, not GPIO selections", t)
            Pill("Read CORE input mapping", t) { ble.readTriggerMap() }
            if (snapshot != null) {
                Caption("Revision " + snapshot.revision +
                    " · physical mask " + snapshot.rawPhysicalMask +
                    " · effective mask " + snapshot.effectiveMask +
                    (if (!snapshot.persisted) " · pending application or unsaved"
                     else if (snapshot.status != 0) " · active map read; last save rejected (" + snapshot.status + ")"
                     else " · active/persisted"), t)
                for (i in 0 until NativeTriggerMappingCodec.INPUTS) {
                    Caption("IN" + (i + 1) + " → " + names[draft[i]], t)
                    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        names.forEachIndexed { id, name ->
                            Pill(name, t, draft[i] == id) {
                                draft = draft.toMutableList().also { it[i] = id }
                            }
                        }
                    }
                }
                if (snapshot.persisted && snapshot.revision in 1..65534)
                    Pill("Save mapping to CORE · verify readback", t, true) { confirm = true }
                else Caption("CORE mapping cannot be edited until the pending profile applies, a valid readback is loaded, or revisions are available.", t)
            } else Caption("Read the authenticated CORE map before editing; no writes on selection.", t)
        }
        if (confirm && snapshot != null && snapshot.persisted &&
            snapshot.revision in 1..65534) AlertDialog(
            onDismissRequest = { confirm = false },
            title = { Text("Save physical input mapping?") },
            text = { Text("CORE will reject active triggers or stale revisions. Readback must confirm the saved mapping. Offline trigger styles are separate.") },
            confirmButton = { Pill("Save mapping", t, true) {
                ble.saveTriggerMap(snapshot.revision, draft.toIntArray())
                confirm = false
            } },
            dismissButton = { Pill("Cancel", t) { confirm = false } })
    }
    @Composable
    private fun AutoEventsPage(t: Tone, previewEffect: (Int) -> Unit) {
        Title("Auto Events", t)
        Caption("Startup and shutdown are optional offline designs. CORE alone controls real ignition transitions.", t)
        var choosing by remember { mutableStateOf<String?>(null) }
        var search by remember { mutableStateOf("") }
        var eventMessage by remember { mutableStateOf("") }
        if (eventMessage.isNotEmpty()) Caption(eventMessage, t)
        fun applyAutoEvent(event: String, field: String, value: Any): Boolean {
            val saved = project.setAutoEvent(event, field, value)
            eventMessage = if (saved) "" else "Could not save " + event + " settings"
            if (saved) refresh()
            return saved
        }
        listOf("Startup", "Shutdown").forEach { key ->
            val event = project.trigger(key)
            Panel(t) {
                Text(key, color = t.fg, fontWeight = FontWeight.Bold)
                val enabled = event.optBoolean("eventUiEnabled", false)
                Pill(if (enabled) "Enabled ✓" else "Disabled", t, enabled) {
                    applyAutoEvent(key, "eventUiEnabled", !enabled)
                }
                Caption("Animation · " + event.optString("eventFx", "Solid"), t)
                Pill("Choose " + key + " animation", t) {
                    choosing = key
                    search = ""
                }
                val fade = event.optInt("eventUiFadeMs", 0).coerceIn(0, 5000)
                ValueSlider("Transition fade (ms)", fade, 5000, t) {
                    applyAutoEvent(key, "eventUiFadeMs", it)
                }
                val tenths = (event.optDouble("eventDuration", 2.0) * 10.0).toInt().coerceIn(2, 300)
                ValueSlider("Duration (tenths of sec)", tenths, 300, t) {
                    applyAutoEvent(key, "eventDuration", it.coerceAtLeast(2) / 10.0)
                }
                Pill("Preview offline", t) {
                    val selected = NativeEffectCatalog.byName(event.optString("eventFx", "Solid"))
                    previewEffect(selected.id)
                }
                Caption("Offline preview inspects exact design pixels; CORE provides animation.", t)
            }
        }
        choosing?.let { key ->
            AlertDialog(onDismissRequest = { choosing = null },
                title = { Text("Select " + key + " animation") },
                text = {
                    Column(Modifier.heightIn(max = 360.dp).verticalScroll(rememberScrollState()),
                        verticalArrangement = Arrangement.spacedBy(7.dp)) {
                        Field(search, "Search effects", t) { search = it }
                        NativeEffectCatalog.all()
                            .filter { it.name.contains(search, ignoreCase = true) }
                            .take(15).forEach { item ->
                                Pill(item.name, t) {
                                    if (applyAutoEvent(key, "eventFx", item.name))
                                        choosing = null
                                }
                            }
                    }
                },
                confirmButton = { Pill("Done", t, true) { choosing = null } })
        }
    }
    @Composable
    private fun CorePaletteTransferPanel(t: Tone) {
        var slotText by rememberSaveable { mutableStateOf("0") }
        var stopsText by rememberSaveable {
            mutableStateOf("0:#FF0000,255:#0000FF")
        }
        var deleteConfirm by remember { mutableStateOf(false) }
        val slot = slotText.toIntOrNull()
        val validSlot = slot != null && slot in 0..127
        val matchesReadback = validSlot &&
            paletteLoadedSlot.intValue == slot && paletteLoadedRevision.longValue >= 0L
        Panel(t) {
            Title("CORE custom palettes", t)
            Caption("Authenticated BLE v3 · 128 slots · 2–18 exact RGB stops. " +
                "Stored in CORE flash only after explicit Upload/Delete and verified readback.", t)
            Pill("Check CORE palette v3", t) {
                if (!ble.negotiatePaletteV3())
                    paletteTransferMessage.value = "Authorize a CORE advertising palette v3 and finish the current BLE transfer"
            }
            Caption(if (paletteV3Supported.value)
                "CORE v3 capability negotiated · palette IDs 200 down to 73"
                else "No authenticated v3 palette capability verified; older CORE stays offline-only", t)
            Field(slotText, "Palette slot 0–127", t) {
                slotText = it.filter { c -> c.isDigit() }.take(3)
            }
            if (validSlot) Caption("Custom palette ID " + (200 - slot!!), t)
            Field(stopsText, "0:#FF0000,128:#00FF00,255:#0000FF", t) {
                stopsText = it.take(320)
            }
            Caption("Strictly increasing positions; include 0 and 255. " +
                "Example: 0:#FF0000,255:#0000FF", t)
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Pill("Download / query", t) {
                    if (!validSlot || !ble.downloadPalette(slot!!))
                        paletteTransferMessage.value = "Negotiate v3, authorize, and enter a valid slot"
                }
                Pill("Upload / verify", t) {
                    if (!matchesReadback) {
                        paletteTransferMessage.value = "Download this slot first to obtain its current revision"
                    } else if (ble.uploadPalette(slot!!, paletteLoadedRevision.longValue, stopsText)) {
                        paletteLoadedSlot.intValue = -1 // no save success until CRC/readback confirms
                        paletteTransferMessage.value = "Upload requested · verifying CORE NVS and RGB stops"
                        invalidateCoreFrame()
                    } else paletteTransferMessage.value = "Upload rejected; check BLE state or exact stop syntax"
                }
                Pill("Delete", t) {
                    if (matchesReadback && paletteLoadedPresent.value) deleteConfirm = true
                    else paletteTransferMessage.value = "Download a present palette before deleting"
                }
            }
            if (matchesReadback) {
                Caption("CORE revision " + paletteLoadedRevision.longValue +
                    (if (paletteLoadedPresent.value) " · stored" else " · empty / tombstone"), t)
                if (paletteLoadedPresent.value) {
                    Caption("Verified stored stops: " + paletteLoadedStops.value, t)
                    Pill("Copy CORE stops into editor", t) { stopsText = paletteLoadedStops.value }
                }
            }
            Caption(paletteTransferMessage.value, t)
            Caption("Uploading a custom palette does not prove live support for any selected effect. " +
                "Unsupported effect/palette combinations stay local until CORE advertises them; " +
                "only CORE frame snapshots can verify logical RGB output.", t)
        }
        if (deleteConfirm) AlertDialog(
            onDismissRequest = { deleteConfirm = false },
            title = { Text("Delete stored CORE palette?") },
            text = { Text("This writes a revision tombstone to CORE flash. The palette must not be active or referenced.") },
            confirmButton = { Pill("Delete from CORE", t, true) {
                deleteConfirm = false
                if (matchesReadback && ble.deletePalette(slot!!, paletteLoadedRevision.longValue)) {
                    paletteLoadedSlot.intValue = -1
                    paletteTransferMessage.value = "Delete requested · awaiting tombstone readback"
                    invalidateCoreFrame()
                } else paletteTransferMessage.value = "Delete rejected; reload slot and check CORE inputs"
            } },
            dismissButton = { Pill("Cancel", t) { deleteConfirm = false } }
        )
    }

    @Composable
    private fun SettingsPage(t: Tone, changePage: (Int) -> Unit) {
        Title("Settings", t)
        Panel(t) {
            Text("Bluetooth · " + ble.connectionPhase(), color = t.fg)
            Caption(bleMessage.value, t)
            Caption("Authorize only on a physically present CORE bench controller.", t)
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                ConnectPill {
                    foundDevices.clear()
                    ble.scanHeadless()
                }
                Pill("Preview Offline", t) {
                    enterOfflinePreview()
                    changePage(1)
                }
            }
            foundDevices.forEach { (address, label) ->
                Pill(label + " · Connect", t) { ble.connectFoundDevice(address) }
            }
            Pill("Authorize / Pair with BOOT", t) { ble.authorizeOrPair() }
            Pill("Read supported effects", t) { ble.readFxCapabilities() }
            Caption(supportedIds.value.size.toString() + " effects reported by connected CORE", t)
            Caption("Every editor is live. When BLE is authorized, effect gestures automatically preview on existing CORE zones (temporary TTL, max 24/255).", t)
            if (ble.canControl() && supportedIds.value.isNotEmpty()) {
                Caption(if (liveArmed.value) "Hardware preview active" else "Hardware preview ready on next edit", t)
                Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                    listOf("Both" to 3, "OUT 1" to 1, "OUT 2" to 2)
                        .forEach { (label, mask) ->
                            Pill(label, t, liveChainMask.intValue == mask) {
                                liveChainMask.intValue = mask
                                previewCurrentEffect()
                            }
                        }
                }
                Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Pill("All zones", t, liveZoneMask.intValue == 255) {
                        liveZoneMask.intValue = 255
                        previewCurrentEffect()
                    }
                    (0 until 8).forEach { zone ->
                        val mask = 1 shl zone
                        Pill("Zone " + (zone + 1), t, liveZoneMask.intValue == mask) {
                            liveZoneMask.intValue = mask
                            previewCurrentEffect()
                        }
                    }
                }
            } else Caption("Offline design updates instantly. Connect and authorize CORE to preview supported effects on LEDs.", t)
            Caption("Changing zone boundaries and individual pixels on hardware requires a firmware volatile-preview command; Android does not flash NVS on each edit.", t)
        }
        CorePaletteTransferPanel(t)
        var confirm by remember { mutableStateOf(false) }
        Panel(t) {
            Caption("Bench effect demo requires confirmation. Authorized live effect edits also send bounded temporary previews automatically.", t)
            Pill("Test a supported effect", t) { confirm = true }
            Pill("Stop CORE show", t) {
                invalidateCoreFrame()
                if (liveArmed.value) livePreview.stop() else ble.stopShow()
                liveArmed.value = false
            }
        }
        if (confirm) AlertDialog(onDismissRequest = { confirm = false },
            title = { Text("Test CORE LEDs on isolated bench?") },
            text = { Text("Requires BLE authorization and reported effect support. 15-second TTL, brightness 24/255. Vehicle inputs retain priority.") },
            confirmButton = { Pill("Start bench test", t, true) {
                val id = prefs.getInt("effectId", 0)
                if (ble.canControl() && id in supportedIds.value) {
                    invalidateCoreFrame()
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
    }
    @Composable
    private fun HardwareOutputEditor(t: Tone) {
        val outputLimit = BleConfigCodec.MAX_LEDS_PER_OUTPUT
        var index by rememberSaveable { mutableIntStateOf(0) }
        // String snapshots make edits observable without mutating the last
        // confirmed hardware readback. A failed save cannot masquerade as NVS.
        var draftText by remember(index, outputs[index]) {
            mutableStateOf(outputs[index]?.toString().orEmpty())
        }
        var confirm by remember { mutableStateOf(false) }
        var structureVersion by remember(index, outputs[index]) { mutableIntStateOf(0) }
        val draft = runCatching { JSONObject(draftText) }.getOrNull()
        fun edit(change: (JSONObject) -> Unit) {
            runCatching {
                val updated = JSONObject(draftText)
                change(updated)
                draftText = updated.toString()
            }.onFailure { outputMessage.value = "Invalid draft: " + (it.message ?: "input") }
        }
        // Generic live zone cursor logic. Updating any zone recomputes the
        // output's active allocation and every downstream start automatically.
        // This changes draft RAM only; NVS still requires confirmed Save.
        fun adjustZone(zoneIndex: Int, newLength: Int) {
            if (newLength !in 0..outputLimit) return
            edit { next ->
                val arr = next.getJSONArray("zones")
                if (zoneIndex !in 0 until arr.length()) return@edit
                val others = (0 until arr.length())
                    .filter { it != zoneIndex }.sumOf { arr.getJSONObject(it).getInt("pixels") }
                val total = others + newLength
                if (total !in 1..outputLimit) return@edit
                arr.getJSONObject(zoneIndex).put("pixels", newLength)
                next.put("pixels", total)
            }
        }
        fun adjustOutput(newTotal: Int) {
            if (newTotal !in 1..outputLimit) return
            edit { next ->
                val arr = next.getJSONArray("zones")
                if (arr.length() == 0) return@edit
                val others = (0 until arr.length() - 1)
                    .sumOf { arr.getJSONObject(it).getInt("pixels") }
                if (newTotal < others) return@edit
                arr.getJSONObject(arr.length() - 1).put("pixels", newTotal - others)
                next.put("pixels", newTotal)
            }
        }

        Panel(t) {
            Title("CORE Output Manager", t)
            Caption("Hardware settings are separate from your Studio design. Reload each output over authorized BLE before editing.", t)
            Caption("Physical pixel capacity depends on the installed CORE firmware and board memory budget.", t)
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
        var outputCountText by remember(index, outputs[index]) {
            mutableStateOf(outputs[index]?.optInt("pixels", 1)?.toString() ?: "1")
        }
        Panel(t) {
            Text("Output " + (index + 1), color = t.fg, fontWeight = FontWeight.Bold)
            Field(draft.optString("name"), "Output name", t) { value ->
                edit { it.put("name", value) }
            }
            Caption("Pixels (1–" + outputLimit + " BLE format limit; CORE validates hardware capacity)", t)
            Field(outputCountText, "Pixel count", t) { value ->
                outputCountText = value.filter(Char::isDigit).take(5)
                outputCountText.toIntOrNull()?.let(::adjustOutput)
            }
            ValueSlider("Total output pixels", pixelCount.coerceIn(1, outputLimit), outputLimit, t) { newCount ->
                adjustOutput(newCount.coerceAtLeast(1))
                outputCountText = newCount.coerceAtLeast(1).toString()
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
            var zoneCountText by remember(index, i, outputs[index], structureVersion) {
                mutableStateOf(zone.optInt("pixels", 0).toString())
            }
            Panel(t) {
                Text("Zone " + (i + 1), color = t.fg, fontWeight = FontWeight.SemiBold)
                Caption("Start " + from + " · end " + (if (count > 0) cursor - 1 else "empty") +
                    " · next " + cursor, t)
                Field(zone.optString("name"), "Zone name", t) { value ->
                    edit { it.getJSONArray("zones").getJSONObject(i).put("name", value) }
                }
                Field(zoneCountText, "Zone pixel count", t) { value ->
                    zoneCountText = value.filter(Char::isDigit).take(5)
                    zoneCountText.toIntOrNull()?.let { n ->
                        adjustZone(i, n)
                        outputCountText = JSONObject(draftText).optInt("pixels", 0).toString()
                    }
                }
                ValueSlider("Next start · zero-based", cursor.coerceIn(0, outputLimit), outputLimit, t) { nextStart ->
                    val n = (nextStart - from).coerceAtLeast(0)
                    adjustZone(i, n)
                    zoneCountText = n.toString()
                    outputCountText = JSONObject(draftText).optInt("pixels", 0).toString()
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
                        var remaining = (0 until next.length()).sumOf { j ->
                            next.getJSONObject(j).getInt("pixels")
                        }
                        if (remaining == 0 && next.length() > 0) {
                            next.getJSONObject(0).put("pixels", 1)
                            remaining = 1
                        }
                        it.put("zones", next)
                        it.put("pixels", remaining)
                    }
                    structureVersion++
                    outputCountText = JSONObject(draftText).optInt("pixels", 0).toString()
                }
            }
        }
        Panel(t) {
            Caption("Assigned " + assigned + " of " + pixelCount + " pixels · " +
                (pixelCount - assigned) + " remaining. Zone offsets are zero-based and automatically contiguous.", t)
            PixelStrip(t, (0 until zones.length()).map { j ->
                val part = zones.optJSONObject(j)
                (part?.optInt("pixels", 0) ?: 0) to rolePixelColor(part?.optString("role", "off") ?: "off")
            })
            Caption("Immediate draft preview. Physical boundaries change only after confirmed NVS Save.", t)
            if (zones.length() < 8) Pill("+ Add zone", t) {
                edit {
                    it.getJSONArray("zones").put(JSONObject()
                        .put("name", "Zone " + (zones.length() + 1))
                        .put("pixels", 0).put("role", "off"))
                }
                structureVersion++
            }
            Pill("Fit last zone to remaining pixels", t) {
                edit {
                    val group = it.getJSONArray("zones")
                    if (group.length() > 0) {
                        val other = (0 until group.length() - 1)
                            .sumOf { j -> group.getJSONObject(j).getInt("pixels") }
                        val remainder = it.getInt("pixels") - other
                        if (remainder in 0..outputLimit)
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
                        if (ble.saveOutput(index, draftText))
                            outputMessage.value = "Save requested · verifying hardware readback"
                        else
                            outputMessage.value = "Save request rejected · see BLE status"
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
