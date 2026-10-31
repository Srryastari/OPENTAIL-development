package com.opentail.android

import android.bluetooth.BluetoothAdapter
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.material3.LocalTextStyle
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
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.disabled
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.sp
import org.json.JSONArray
import org.json.JSONObject
import java.math.BigDecimal
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
    private val firmwareVersion = mutableStateOf<String?>(null)
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
    private val localMicrophone = MicAudioPeak()
    private var microphonePermissionRevision by mutableIntStateOf(0)
    private var microphonePermissionRequested = false
    private val microphonePermission = registerForActivityResult(ActivityResultContracts.RequestPermission()) {
        microphonePermissionRevision++
    }
    private lateinit var livePreview: LiveBlePreview
    private val liveArmed = mutableStateOf(false)
    private val offlinePreview = mutableStateOf(false)
    private val offlineReplay = mutableIntStateOf(0)
    private val selectedEffect = mutableIntStateOf(0)
    // Auto Events visual inspection must never change Show Modes' selected
    // effect or navigate away to a general Effects page (owner Test 86).
    private val autoEventPreviewEffect = mutableIntStateOf(-1)
    private val liveChainMask = mutableIntStateOf(3)
    private val liveZoneMask = mutableIntStateOf(255)
    private val coreFrame = mutableStateOf<ExactLedFrame?>(null)
    private val captureMessage = mutableStateOf("CORE render snapshot not captured")
    private val sequenceMessage = mutableStateOf("")
    private val captureOutput = mutableIntStateOf(0)
    private val previewPaused = mutableStateOf(false)
    private val foregroundActivity = mutableStateOf(false)
    private val foregroundEpoch = mutableIntStateOf(0)
    // A verified snapshot is valid only for the UI state and output at the
    // time its capture began. Old asynchronous GATT replies cannot resurrect
    // pixels after an edit, disconnect, or switch into offline preview.
    private var frameRevision by mutableLongStateOf(0L)
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
                ble.readFxCapabilitiesAfterAuth()
            if (!ble.isConnected()) firmwareVersion.value = null
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
        ble.setInfoListener { version ->
            runOnUiThread { firmwareVersion.value = version }
        }
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
        ble.setPreferredListener { address ->
            runOnUiThread { prefs.edit().putString("preferred_ble_address", address).apply() }
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
    override fun onStart() {
        super.onStart()
        foregroundEpoch.intValue++
        foregroundActivity.value = true
        if (!ble.isConnected()) {
            prefs.getString("preferred_ble_address", null)?.let { ble.tryReconnect(it) }
        } else if (!ble.canControl()) {
            ble.authorizeOrPair()
        }
    }
    override fun onStop() {
        localMicrophone.stop()
        foregroundActivity.value = false
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
        localMicrophone.stop()
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
    else Tone(Color.Black, Color(0xFF111111), Color(0xFF191919),
              Color(0xFFF5F5F5), Color(0xFFA7A7A7), Color.White, Color(0xFFED6472))

    @Composable
    private fun OpenTailShell() {
        val changed = revision.intValue // observe authoritative model revisions
        var page by rememberSaveable { mutableIntStateOf(0) }
        var backStack by remember { mutableStateOf(listOf<Int>()) }
        fun navigate(destination: Int) {
            if (destination !in pages.indices || destination == page) return
            backStack = backStack + page
            page = destination
        }
        // All six screens share one navigation stack. Native Compose
        // dialogs handle their own Back. When a field owns the IME, first
        // Back dismisses the keyboard rather than silently leaving the editor.
        val keyboardController = LocalSoftwareKeyboardController.current
        val focusManager = LocalFocusManager.current
        val imeVisible = WindowInsets.ime.getBottom(LocalDensity.current) > 0
        BackHandler(enabled = page != 0 || backStack.isNotEmpty()) {
            if (imeVisible) {
                keyboardController?.hide()
                focusManager.clearFocus()
            } else {
                page = backStack.lastOrNull() ?: 0
                backStack = backStack.dropLast(1)
            }
        }
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
        CompositionLocalProvider(LocalTextStyle provides TextStyle(
            fontFamily=FontFamily.SansSerif,fontSize=14.sp,letterSpacing=0.sp)) {
        Column(Modifier.fillMaxSize().background(t.bg)
            .windowInsetsPadding(WindowInsets.safeDrawing).padding(top = 14.dp)) {
            Row(Modifier.fillMaxWidth().padding(horizontal = 28.dp), verticalAlignment = Alignment.CenterVertically) {
                Text("OpenTail", color = t.fg, fontSize = 23.sp, fontWeight = FontWeight.Bold,
                     modifier = Modifier.weight(1f))
                UiIcon("bluetooth", t.muted)
            }
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal=28.dp,vertical=12.dp),
                horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                pages.forEachIndexed { i, title ->
                    NavigationPill(i, title, t, page == i) {
                        // Avoid showing the previous event's preview when
                        // returning to Auto Events, which defaults to Startup.
                        if(page==4 && i!=4) autoEventPreviewEffect.intValue=-1
                        // Preview Offline remains active across tabs (Test 79).
                        navigate(i)
                    }
                }
            }
            Column(Modifier.fillMaxSize().verticalScroll(pageScroll)
                .padding(horizontal = 28.dp, vertical = 8.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp)) {
                when (page) {
                    0 -> HomePage(t, { navigate(it) })
                    1 -> ShowModesPage(t)
                    2 -> LayoutPage(t)
                    3 -> TriggersPage(t)
                    4 -> AutoEventsPage(t) { effectId ->
                        autoEventPreviewEffect.intValue = effectId
                        offlineReplay.intValue++
                        enterOfflinePreview()
                    }
                    5 -> SettingsPage(t, { navigate(it) }, light) {
                        light = !light
                        prefs.edit().putBoolean("whiteMode", light).apply()
                    }
                }
                Spacer(Modifier.height(24.dp))
            }
        }
        }
        }
    }
    @Composable
    private fun Pill(label: String, t: Tone, selected: Boolean = false, enabled: Boolean = true, strong: Boolean = false, action: () -> Unit) {
        // Selected surfaces are neutral graphite, not colored brand accents.
        val fill = if(strong && t.bg==Color.Black) Color(0xFF444444) else if (selected) (if (t.bg == Color.Black) Color(0xFF303030)
            else Color(0xFFDEDEDE)) else if(t.bg==Color.Black) Color(0xFF2B2B2B) else t.raised
        Box(Modifier.height(28.dp).clip(RoundedCornerShape(99.dp))
                .background(fill)
                .semantics { if(!enabled) disabled() }
                .clickable(enabled=enabled,onClick = action).padding(horizontal = 14.dp),
            contentAlignment = Alignment.Center) {
            Text(label, fontSize = 13.sp, fontWeight = FontWeight.SemiBold,
                 color = if(enabled) t.fg else t.muted.copy(alpha=0.45f), maxLines = 1)
        }
    }
    // Native vector glyphs preserve the approved icon meanings without
    // emoji, downloaded fonts, WebView or an additional icon dependency.
    // Preserve the approved pill styling, but make the actual VISIBLE
    // +/- button 56x48dp. A large invisible wrapper around a tiny Pill
    // does not satisfy the physical phone accessibility requirement.
    @Composable
    private fun PixelStepPill(symbol: String, t: Tone, onClick: () -> Unit) {
        Box(Modifier.size(width=56.dp,height=48.dp)
            .clip(RoundedCornerShape(99.dp)).background(t.raised)
            .semantics { contentDescription=if(symbol=="+") "Add one pixel" else "Remove one pixel" }
            .clickable(onClick=onClick),contentAlignment=Alignment.Center) {
            Text(symbol,color=t.fg,fontSize=22.sp,fontWeight=FontWeight.Bold)
        }
    }
    @Composable
    private fun NavigationPill(index: Int, label: String, t: Tone,
                               selected: Boolean, action: () -> Unit) {
        val fill = if (selected) (if(t.bg == Color.Black) Color(0xFF303030) else Color(0xFFDEDEDE)) else t.raised
        Box(Modifier.height(36.dp).clip(RoundedCornerShape(99.dp))
            .background(fill).clickable(onClick = action)
            .padding(horizontal = 9.dp), contentAlignment = Alignment.Center) {
            Text(label, fontSize = 15.sp,
                fontWeight = if (selected) FontWeight.Bold else FontWeight.Normal,
                color = if (selected) t.fg else t.muted, maxLines = 1)
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
    private fun Panel(t: Tone, minHeight: Dp = 0.dp, fill: Color = t.surface, content: @Composable ColumnScope.() -> Unit) {
        Column(Modifier.fillMaxWidth().heightIn(min=minHeight).clip(RoundedCornerShape(20.dp))
            .background(fill)
            .padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(9.dp), content = content)
    }
    @Composable
    private fun Caption(text: String, t: Tone) {
        Text(text, color = t.muted, fontSize = 12.sp, lineHeight = 17.sp)
    }
    @Composable
    private fun Title(text: String, t: Tone) {
        Row(Modifier.fillMaxWidth().padding(bottom=2.dp),
            verticalAlignment=Alignment.CenterVertically) {
            Text(text, Modifier.weight(1f), color=t.fg, fontSize=23.sp, fontWeight=FontWeight.Bold)
            val index=pages.indexOf(text)
            if(index in 1..4) NavigationGlyph(index,t.muted)
            if(index==5) UiIcon("settings",t.muted)
        }
    }
    @Composable
    private fun Field(value: String, hint: String, t: Tone,
                      modifier: Modifier = Modifier.fillMaxWidth(), onChange: (String) -> Unit) {
        BasicTextField(
            value = value, onValueChange = onChange, singleLine = true,
            cursorBrush = SolidColor(t.accent),
            textStyle = TextStyle(color = t.fg, fontSize = 14.sp),
            modifier = modifier.heightIn(min = 48.dp).clip(RoundedCornerShape(99.dp))
                .background(t.surface).border(1.dp,t.muted.copy(alpha=0.45f),RoundedCornerShape(99.dp))
                .padding(horizontal = 18.dp, vertical = 11.dp),
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
        val committed = value.coerceIn(min, upper).toFloat()
        // Keep the thumb responsive during every gesture, even while an
        // immediate SharedPreferences commit and JSON snapshot are in flight.
        var dragging by remember { mutableStateOf(false) }
        var dragValue by remember { mutableFloatStateOf(committed) }
        LaunchedEffect(committed, dragging) {
            if (!dragging) dragValue = committed
        }
        Slider(
            value = if (dragging) dragValue else committed,
            onValueChange = {
                dragging = true
                dragValue = it.coerceIn(min.toFloat(), upper.toFloat())
                change(dragValue.toInt().coerceIn(min, upper))
            },
            onValueChangeFinished = { dragging = false; finished?.invoke() },
            valueRange = min.toFloat()..upper.toFloat(),
            colors = SliderDefaults.colors(
                thumbColor = t.accent, activeTrackColor = t.accent,
                inactiveTrackColor = t.raised
            ),
            thumb = {
                Box(Modifier.size(20.dp)
                    .shadow(5.dp, RoundedCornerShape(99.dp),
                            ambientColor = t.accent.copy(alpha = 0.25f),
                            spotColor = t.accent.copy(alpha = 0.25f))
                    .clip(RoundedCornerShape(99.dp)).background(t.bg)
                    .border(1.dp,t.muted.copy(alpha=0.4f),RoundedCornerShape(99.dp)))
            },
            track = { state ->
                val progress = ((state.value - min) / (upper - min)).coerceIn(0f, 1f)
                Box(Modifier.fillMaxWidth().height(4.dp)
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
        var expanded by remember { mutableStateOf(false) }
        Text(label,color=t.fg,fontSize=13.sp,fontWeight=FontWeight.SemiBold)
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Box(Modifier.size(34.dp).clip(RoundedCornerShape(99.dp))
                .background(Color(color)).semantics { contentDescription="Edit " + label }
                .clickable { expanded=!expanded })
            Field(hex, "#RRGGBB", t, Modifier.weight(1f)) { entered ->
                hex = entered.take(7)
                if (Regex("^#[0-9A-Fa-f]{6}$").matches(hex))
                    change(android.graphics.Color.parseColor(hex))
            }
        }
        if(expanded) {
            val hsv=FloatArray(3)
            android.graphics.Color.colorToHSV(color,hsv)
            ValueSlider("Brightness",(hsv[2]*255).toInt(),255,t) {
                change(android.graphics.Color.HSVToColor(floatArrayOf(hsv[0],hsv[1],it/255f)))
            }
            ValueSlider("Hue",hsv[0].toInt(),359,t) {
                change(android.graphics.Color.HSVToColor(floatArrayOf(it.toFloat(),1f,hsv[2])))
            }
        }
    }
    // Pure native designer control. Uses the same neutral pills, search field,
    // and OLED/white colors as the locked interface. Never invent CORE pixels.
    @Composable
    private fun PalettePicker(label: String, value: Int, t: Tone, onSelect: (Int) -> Unit) {
        var browsing by rememberSaveable(label) { mutableStateOf(false) }
        var search by rememberSaveable(label) { mutableStateOf("") }
        fun sourcePalette(id: Int): NativePalette {
            val authoritative = if (paletteLoadedPresent.value &&
                paletteLoadedSlot.intValue in 0..127 &&
                id == 200 - paletteLoadedSlot.intValue &&
                paletteLoadedRevision.longValue >= 0L) paletteLoadedStops.value else null
            return NativePaletteEngine.lookup(project.raw(), id,
                if (prefs.contains("foreground")) prefs.getInt("foreground",0) else null,
                if (prefs.contains("background")) prefs.getInt("background",0) else null,
                authoritative)
        }
        val chosen = sourcePalette(value)
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
                    val palette = sourcePalette(id)
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
    private fun EmptyLightingPreview(t: Tone) {
        // An unlit preview remains visible before zones or a CORE frame exist.
        // Neutral dots are UI placeholders, never simulated effect output.
        Canvas(Modifier.fillMaxWidth().height(96.dp)
            .clip(RoundedCornerShape(16.dp)).background(t.raised)) {
            val pitch=size.width/30f
            val rowPitch=size.height/3f
            for(i in 0 until 90){
                val center=Offset((i%30+0.5f)*pitch,(i/30+0.5f)*rowPitch)
                drawCircle(t.muted.copy(alpha=0.10f),radius=7.5f,center=center)
                drawCircle(t.muted.copy(alpha=0.38f),radius=3.5f,center=center)
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
        val connectionPhase = remember(bleMessage.value) { ble.connectionPhase().replaceFirstChar { it.uppercase() } }
        Title("Home", t)
        Panel(t) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically) {
                Text("Connection", color = t.fg, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                Box(Modifier.clip(RoundedCornerShape(99.dp)).background(t.raised)
                    .padding(horizontal=12.dp,vertical=5.dp)) {
                    Text(connectionPhase,color=t.muted,fontSize=11.sp,lineHeight=14.sp)
                }
            }
            Row(Modifier.fillMaxWidth().padding(vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically) {
                UiIcon("bluetooth",t.muted)
                Spacer(Modifier.width(8.dp))
                Column(Modifier.weight(1f)) {
                    Text("OpenTail Controller", color = t.fg, fontSize = 14.sp)
                    Caption(connectionPhase, t)
                }
                Pill("Connect", t, strong=true) { foundDevices.clear(); ble.scanHeadless() }
            }
            // Discovered choices disappear while a controller is connected.
            if (!ble.isConnected()) foundDevices.forEach { (address, label) ->
                Pill(label + " · Connect", t) { ble.connectFoundDevice(address) }
            }
            Box(Modifier.fillMaxWidth().height(1.dp).background(t.raised))
            Row(Modifier.fillMaxWidth().padding(vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically) {
                UiIcon("shield",t.muted)
                Spacer(Modifier.width(8.dp))
                Column(Modifier.weight(1f)) {
                    Text("Authorization", color = t.fg, fontSize = 14.sp)
                    Caption(if (ble.canControl()) "Authorized" else "Not authorized", t)
                }
                Pill("Authorize", t, enabled=ble.isConnected() && !ble.canControl()) { ble.authorizeOrPair() }
            }
        }
        EffectsPreview(t, selectedEffect.intValue)
        Panel(t) {
            Text("Controller Status", color = t.fg, fontSize = 16.sp, fontWeight = FontWeight.Bold)
            listOf("Connection" to connectionPhase,
                "Authorization" to if (ble.canControl()) "Authorized" else "Not authorized",
                "Firmware" to (firmwareVersion.value ?: "Unavailable")).forEachIndexed { index, (label, value) ->
                if (index > 0) Box(Modifier.fillMaxWidth().height(1.dp).background(t.raised))
                Row(Modifier.fillMaxWidth().padding(vertical = 12.dp),
                    horizontalArrangement = Arrangement.SpaceBetween) {
                    Text(label, color = t.muted, fontSize = 13.sp)
                    Text(value, color = t.fg, fontSize = 13.sp)
                }
            }
        }
    }
    private fun capturePreviewFrame() {
        if(!foregroundActivity.value || offlinePreview.value || previewPaused.value || !ble.canCaptureFrame()) return
        if(ble.captureFrame(captureOutput.intValue)) {
            pendingFrameRevision=frameRevision
            pendingFrameOutput=captureOutput.intValue
            captureMessage.value="Capturing CORE frame"
        }
    }
    @Composable
    private fun EffectsPreview(t: Tone, effect: Int, animateOffline: Boolean = false, settingsOverride: IntArray? = null) {
        // Reference dimensions are logical UI pixels (dp), independent of
        // display density. Never substitute design colors for CORE telemetry.
        val connection = bleMessage.value.let { ble.canCaptureFrame() }
        val localRevision = revision.intValue
        // Preference changes invalidate local settings independently of CORE
        // capture invalidation. In particular, scoped Speed never goes stale.
        var settingsRevision by remember { mutableIntStateOf(0) }
        DisposableEffect(effect) {
            val observer = android.content.SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
                if (key?.startsWith("wled.${effect}.") == true ||
                    key in setOf("foreground","background","speed","intensity","brightness","paletteId"))
                    settingsRevision++
            }
            prefs.registerOnSharedPreferenceChangeListener(observer)
            onDispose { prefs.unregisterOnSharedPreferenceChangeListener(observer) }
        }
        val storedSettings = remember(effect, frameRevision, settingsRevision) {
            OfflinePreviewSettings.read(effect) { key, fallback -> prefs.getInt(key,fallback) }
        }
        val settings = settingsOverride ?: storedSettings
        val renderer = remember(effect,offlineReplay.intValue) { OfflineCoreRenderer(effect,30) }
        DisposableEffect(renderer) { onDispose { renderer.close() } }
        val localSketch = animateOffline && (offlinePreview.value || !ble.canControl()) &&
            renderer.available()
        // Read custom gradient stops exactly; no invented palette colors.
        val customStops = remember(settings[5],localRevision,paletteLoadedStops.value,
            paletteLoadedPresent.value,paletteLoadedSlot.intValue) {
            val id=settings[5]
            if(id !in 73..255) null else {
                val verified=if(paletteLoadedPresent.value && id==200-paletteLoadedSlot.intValue)
                    paletteLoadedStops.value else null
                if(verified!=null) runCatching { NativePaletteV3.stops(verified) }.getOrNull()
                else runCatching {
                    val raw=JSONObject(project.exportJson()).optJSONObject("palettePreviews")?.optJSONArray(id.toString())
                    if(raw==null || raw.length() !in 2..18) null else ByteArray(raw.length()*4) { index ->
                        val value=(raw.getJSONArray(index/4).get(index%4) as? Number)?.toDouble()
                            ?: error("Non-numeric palette stop")
                        require(value.isFinite() && value in 0.0..255.0 && value % 1.0 == 0.0)
                        value.toInt().toByte()
                    }
                }.getOrNull()
            }
        }
        val localColors = remember { IntArray(30) }
        val localTimeNanos = remember(effect,offlineReplay.intValue) { mutableLongStateOf(0L) }
        val localRenderAvailable = remember(effect) { mutableStateOf(true) }
        val localFrameMs = remember(effect) { mutableIntStateOf(16) }
        val hasMicPermission = microphonePermissionRevision.let {
            checkSelfPermission(android.Manifest.permission.RECORD_AUDIO) ==
                android.content.pm.PackageManager.PERMISSION_GRANTED
        }
        val needsMic = localSketch && effect==185 && foregroundActivity.value && !previewPaused.value
        DisposableEffect(needsMic,hasMicPermission,foregroundEpoch.intValue) {
            if(needsMic) {
                if(hasMicPermission) localMicrophone.start()
                else if(!microphonePermissionRequested) {
                    microphonePermissionRequested=true
                    microphonePermission.launch(android.Manifest.permission.RECORD_AUDIO)
                }
            }
            onDispose { if(needsMic) localMicrophone.stop() }
        }
        LaunchedEffect(localSketch, effect, offlineReplay.intValue, foregroundActivity.value, previewPaused.value) {
            if (!localSketch || effect == 0 || !foregroundActivity.value || previewPaused.value)
                return@LaunchedEffect
            var previous = -1L
            while (true) {
                withFrameNanos { now ->
                    if (previous >= 0L) {
                        localTimeNanos.longValue += now - previous
                        localFrameMs.intValue=((now-previous)/1_000_000L).toInt().coerceIn(1,65535)
                    }
                    previous = now
                }
            }
        }
        // Separate honest CORE telemetry from the saved design / read-only
        // CORE output profile. A fresh installation has an empty local project;
        // an idle renderer is black even when all hardware zones are configured.
        // The approved 30 capsules must not silently become colorless in either
        // case. Static configured roles are NOT presented as live FX frames.
        val projectPaint = remember(localRevision) {
            runCatching { ExactLedFrame.fromProjectModel(project) }.getOrNull()
        }
        val actualFrame = coreFrame.value
        val hasLiveColors = remember(actualFrame) { actualFrame != null &&
            (0 until actualFrame.size()).any { actualFrame.colorAt(it) and 0xffffff != 0 } }
        val hasProjectColors = remember(projectPaint) { projectPaint != null &&
            (0 until projectPaint.size()).any { projectPaint.colorAt(it) and 0xffffff != 0 } }
        val frame = when {
            offlinePreview.value && hasProjectColors -> projectPaint
            offlinePreview.value -> null
            hasLiveColors -> actualFrame
            hasProjectColors -> projectPaint
            else -> null
        }
        val readback = outputs[captureOutput.intValue]
        val configuredZones = readback?.optJSONArray("zones")
        val configuredPixels = readback?.optInt("pixels",0)?.coerceAtLeast(0) ?: 0
        val useConfiguredColors = frame == null && configuredPixels > 0 &&
            configuredZones != null && configuredZones.length() > 0
        LaunchedEffect(connection, foregroundActivity.value, offlinePreview.value, previewPaused.value, captureOutput.intValue) {
            // Display-paced admission; capture completion remains BLE-bound.
            // A full CORE RGB888 snapshot can require many serialized BLE ATT
            // exchanges. captureFrame() silently declines while busy, so the
            // next request starts as soon as GATT is free without overlapping
            // config reads, owner auth, pending STOP or other commands.
            // The LED capsules always use verified CORE frames, never guessed
            // intermediate pixels. Actual update rate is BLE-throughput bound.
            while(foregroundActivity.value && !offlinePreview.value && !previewPaused.value) {
                capturePreviewFrame()
                withFrameNanos { }
            }
        }
        Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp))
            .background(t.surface).padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("LED PREVIEW",color=t.muted,fontSize=11.sp,lineHeight=14.sp)
            Canvas(Modifier.fillMaxWidth().height(24.dp)) {
                val columns = 30
                val gap=3.dp.toPx()
                val capsuleWidth=(size.width-gap*(columns-1))/columns
                val pitch=capsuleWidth+gap
                val capsuleHeight = 24.dp.toPx()
                val audio = if(effect==185) localMicrophone.latest() else null
                // Keep an effect's saved palette, but do not send it to
                // CORE when that effect's metadata declares no palette.
                // A previous Rainbow/Palette selection otherwise makes
                // Blink, Bouncing Balls and proprietary originals fail render.
                // Preview-only: the locked palette engine is unchanged.
                val rendererPalette = if (NativeEffectParameters.forId(effect)?.palette == true)
                    settings[5] else 0
                val localRendered = localSketch && renderer.render(localTimeNanos.longValue / 1_000_000L,
                    localFrameMs.intValue,settings[2],settings[3],settings[4],rendererPalette,
                    settings[0],settings[1],customStops,audio?.get(0) ?: 0f,
                    audio?.get(1) ?: 0f,audio!=null,localColors)
                if(localSketch) localRenderAvailable.value=localRendered
                for (col in 0 until columns) {
                    val sample = if (frame != null && frame.size() > 0)
                        col * (frame.size() - 1) / (columns - 1) else -1
                    // Physical output configuration is a *static authoring*
                    // preview, never invented LED telemetry or renderer RGB.
                    val configuredSample = if (useConfiguredColors)
                        col * (configuredPixels - 1) / (columns - 1) else -1
                    var configuredColor = t.raised
                    if (configuredSample >= 0 && configuredZones != null) {
                        var cursor = 0
                        for (i in 0 until configuredZones.length()) {
                            val zone = configuredZones.optJSONObject(i) ?: continue
                            cursor += zone.optInt("pixels",0).coerceAtLeast(0)
                            if (configuredSample < cursor) {
                                configuredColor = rolePixelColor(zone.optString("role","off"))
                                break
                            }
                        }
                    }
                    val color = if (localSketch) {
                        if(localRendered) Color(localColors[col]) else t.raised
                    } else if (sample >= 0) Color(frame!!.colorAt(sample))
                        else configuredColor
                    drawRoundRect(color,
                        topLeft = Offset(col * pitch, 0f),
                        size = androidx.compose.ui.geometry.Size(capsuleWidth, capsuleHeight),
                        cornerRadius = androidx.compose.ui.geometry.CornerRadius(capsuleWidth / 2f))
                }
            }
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                Text(when {
                    effect < 0 && animateOffline -> "No effect selected"
                    localSketch && localRenderAvailable.value -> "Offline effect preview · local"
                    localSketch -> "Offline animation unavailable"
                    animateOffline && (offlinePreview.value || !ble.canControl()) ->
                        "Offline animation unavailable"
                    hasLiveColors && !offlinePreview.value -> NativeEffectCatalog.byId(effect).name
                    useConfiguredColors -> "Configured zone colors"
                    offlinePreview.value -> "Offline design preview"
                    hasProjectColors -> "Project pixel colors"
                    else -> "Awaiting CORE frame"
                }, Modifier.weight(1f),color=t.muted,fontSize=14.sp)
                Box(Modifier.size(24.dp).semantics { contentDescription="Refresh LED preview" }
                    .clickable { capturePreviewFrame() },contentAlignment=Alignment.Center) { UiIcon("refresh",t.muted) }
                Box(Modifier.size(24.dp).semantics { contentDescription=if(previewPaused.value) "Resume LED preview" else "Pause LED preview" }
                    .clickable { previewPaused.value=!previewPaused.value },contentAlignment=Alignment.Center) {
                    UiIcon(if(previewPaused.value) "play" else "pause",t.muted)
                }
            }
        }
    }

    @Composable
    private fun ShowModesPage(t: Tone) {
        Title("Show Modes", t)
        var view by rememberSaveable { mutableStateOf("Effects") }
        var query by rememberSaveable { mutableStateOf("") }
        // Legacy saved selections may refer to Auto Events-only transitions.
        // Do not surface those in the general Show Modes editor.
        val effect = selectedEffect.intValue.takeUnless { NativeEffectCatalog.autoEventOnly(it) } ?: 0
        var speed by rememberSaveable(projectLoadRevision.intValue) { mutableIntStateOf(prefs.getInt("speed", 128)) }
        var intensity by rememberSaveable(projectLoadRevision.intValue) { mutableIntStateOf(prefs.getInt("intensity", 128)) }
        var brightness by rememberSaveable(projectLoadRevision.intValue) { mutableIntStateOf(prefs.getInt("brightness", 160)) }
        var foreground by rememberSaveable(projectLoadRevision.intValue) { mutableIntStateOf(prefs.getInt("foreground", -1)) }
        var background by rememberSaveable(projectLoadRevision.intValue) { mutableIntStateOf(prefs.getInt("background", 0xff000000.toInt())) }
        var accent by rememberSaveable(projectLoadRevision.intValue) { mutableIntStateOf(prefs.getInt("accent", -1)) }
        var paletteId by rememberSaveable(projectLoadRevision.intValue) {
            mutableIntStateOf(prefs.getInt("paletteId", 0).coerceIn(0, NativePaletteEngine.MAX_ID))
        }
        var chosen by rememberSaveable(projectLoadRevision.intValue) {
            mutableIntStateOf(effect)
        }
        EffectsPreview(t, if(chosen >= 0) chosen else -1, animateOffline=true)
        Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(8.dp)) {
            listOf("Effects", "Presets", "Sequences").forEach { tab ->
                Box(Modifier.weight(1f)) { Pill(tab,t,view==tab) { view=tab } }
            }
        }
        when(view) {
            "Effects" -> {
                // Owner-approved standalone native editor. Do not reintroduce
                // generic search/list controls or replace the six-screen shell.
                LaunchedEffect(effect) {
                    // Explicit preset import/selection updates the editor only
                    // when the authoritative selected effect actually changes.
                    if (selectedEffect.intValue != chosen && chosen >= 0)
                        chosen = selectedEffect.intValue
                }
                ApprovedWledEffectsEditor(
                    selectedId = chosen.takeIf { it >= 0 },
                    effectOptions = NativeEffectCatalog.generalEffects(),
                    prefs = prefs,
                    paletteFor = { id ->
                        val verified = if(paletteLoadedPresent.value &&
                            paletteLoadedSlot.intValue in 0..127 &&
                            id == 200 - paletteLoadedSlot.intValue &&
                            paletteLoadedRevision.longValue >= 0L)
                            paletteLoadedStops.value else null
                        NativePaletteEngine.lookup(project.raw(), id,
                            if (prefs.contains("foreground")) prefs.getInt("foreground",0) else null,
                            if (prefs.contains("background")) prefs.getInt("background",0) else null,
                            verified,
                            if (prefs.contains("wled.${chosen}.color2"))
                                prefs.getInt("wled.${chosen}.color2",0)
                            else if (prefs.contains("accent")) prefs.getInt("accent",0) else null)
                    },
                    onSelected = { id ->
                        if(id == null) {
                            chosen = -1
                            selectedEffect.intValue = -1
                            prefs.edit().putInt("effectId",-1).apply()
                            if(liveArmed.value) {
                                livePreview.stop()
                                liveArmed.value = false
                            }
                        } else {
                            chosen = id
                            selectedEffect.intValue = id
                            val editor = prefs.edit().putInt("effectId",id)
                            // On effect change restore the independently saved
                            // values to the existing BLE/preset contract.
                            listOf("speed","intensity","brightness","foreground",
                                "background","accent","paletteId").forEach { key ->
                                val scoped = "wled.${id}." + (if(key=="paletteId")"palette" else key)
                                if(prefs.contains(scoped))
                                    editor.putInt(key,prefs.getInt(scoped,0))
                            }
                            editor.apply()
                            if(liveArmed.value) previewCurrentEffect(id)
                            else invalidateCoreFrame()
                        }
                    },
                    onParam = { id,key,value ->
                        val edit = prefs.edit().putInt("wled.${id}.${key}",value)
                        if(key in setOf("speed","intensity","brightness"))
                            edit.putInt(key,value)
                        edit.apply()
                        if(liveArmed.value && key in setOf("speed","intensity","brightness"))
                            previewCurrentEffect(id)
                        else invalidateCoreFrame()
                    },
                    onColor = { id,slot,rgb ->
                        val key = when(slot) { 0->"foreground";1->"background";else->"accent" }
                        prefs.edit().putInt("wled.${id}.color${slot}",rgb)
                            .putInt("wled.${id}.${key}",rgb)
                            .putInt(key,rgb).apply()
                        // Slot 3 is saved correctly but the CORE BLE preview
                        // Frame contract has only foreground/background slots.
                        if(liveArmed.value && slot < 2) previewCurrentEffect(id)
                        else invalidateCoreFrame()
                    },
                    onPalette = { id,selected ->
                        prefs.edit().putInt("wled.${id}.palette",selected)
                            .putInt("paletteId",selected).apply()
                        if(liveArmed.value) previewCurrentEffect(id)
                        else invalidateCoreFrame()
                    }
                )
            }
            "Presets" -> PresetsPage(t)
            "Sequences" -> SequencePage(t,effect)
        }
    }
    private fun effectLabel(id: Int): String = when(id) {
        -1 -> "No effect selected"
        9 -> "Rainbow Show"
        32769 -> "Materialize"
        else -> NativeEffectCatalog.byId(id).name
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
                Text(item.optString("fx") + " · " + formatDurationSeconds(item.optInt("ms", 1500)), color = t.fg)
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
                        editedDuration = BigDecimal.valueOf(item.optInt("ms", 1500).toLong(), 3).stripTrailingZeros().toPlainString()
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
            AlertDialog(containerColor=t.surface, onDismissRequest = { editingStep = -1 },
                title = { Text("Edit sequence duration") },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Caption("0.05–600 seconds. Editable before saving to the design.", t)
                        Field(editedDuration, "Duration in seconds", t) {
                            editedDuration = it.filter { c -> c.isDigit() || c == '.' }.take(10)
                        }
                    }
                },
                confirmButton = { Pill("Save", t, true) {
                    val ms = runCatching {
                        BigDecimal(editedDuration).movePointRight(3).intValueExact()
                    }.getOrNull()
                    if (ms != null && ms in 50..600000) {
                        val updated = sequenceList()
                        updated[editingStep].put("ms", ms)
                        if (updateSequence(updated)) editingStep = -1
                    }
                } },
                dismissButton = { Pill("Cancel", t) { editingStep = -1 } })
        }
    }
    private fun formatDurationSeconds(ms: Int): String =
        BigDecimal.valueOf(ms.toLong(), 3).stripTrailingZeros().toPlainString() + " seconds"

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
        // Plain JSONObject/JSONArray is not observable Compose state.
        // Always consume the committed revision here, not only in the shell.
        val modelRevision = revision.intValue
        val presets = remember(modelRevision) { JSONArray(project.presets().toString()) }
        var pendingLoad by remember { mutableIntStateOf(-1) }
        var presetStatus by remember { mutableStateOf("") }
        var renamePreset by remember { mutableIntStateOf(-1) }
        var renameText by remember { mutableStateOf("") }
        var deletePreset by remember { mutableIntStateOf(-1) }
        Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
            Text("Presets",Modifier.weight(1f),color=t.fg,fontSize=16.sp,fontWeight=FontWeight.Bold)
            Pill("+ Add", t) {
                if (presets.length() < NativeProject.MAX_PRESETS) {
                    // Presets store the design only, not recursive copies of
                    // the entire preset library (which grow on every save).
                    val snapshot = JSONObject(project.exportJson())
                    snapshot.put("savedPresets", JSONArray())
                    val updated = JSONArray(presets.toString())
                    // Effects editor values live in SharedPreferences, not the
                    // Studio v4 document. Store them alongside the document.
                    val effectSettings=JSONObject()
                    for(key in listOf("speed","intensity","brightness","foreground","background","accent","paletteId")) {
                        val fallback=when(key) {
                            "speed","intensity" -> 128
                            "brightness" -> 160
                            "foreground","accent" -> -1
                            "background" -> 0xff000000.toInt()
                            else -> 0
                        }
                        effectSettings.put(key,prefs.getInt(key,fallback))
                    }
                    val editorSettings=JSONObject()
                    val prefix="wled.${selectedEffect.intValue}."
                    for((key,value) in prefs.all) {
                        if(key.startsWith(prefix) && value is Int) editorSettings.put(key,value)
                    }
                    updated.put(JSONObject().put("name", "Preset " + (presets.length() + 1))
                        .put("studio", snapshot.toString()).put("fxId",selectedEffect.intValue)
                        .put("effectName",effectLabel(selectedEffect.intValue))
                        .put("effectSettings",effectSettings)
                        .put("effectEditorSettings",editorSettings))
                    if (project.setPresets(updated)) {
                        presetStatus = "Preset saved locally"
                        refresh()
                    } else presetStatus = "Could not save preset"
                }
            }
        }
        if (presetStatus.isNotEmpty()) Caption(presetStatus, t)
        for (i in 0 until presets.length()) {
            val p = presets.optJSONObject(i) ?: continue
            Panel(t) {
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f).clickable { pendingLoad = i }) {
                        Text(p.optString("name", "Preset"), color = t.fg,
                            fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                        if(p.has("effectName")) Caption(p.optString("effectName"),t)
                    }
                    IconAction("Rename preset", "edit", t) {
                        renamePreset = i; renameText = p.optString("name", "Preset")
                    }
                    IconAction("Delete preset", "delete", t) { deletePreset = i }
                }
                if(renamePreset == i) {
                    Field(renameText, "Preset name", t) { renameText = it }
                    Pill("Done", t) {
                        if(renameText.trim().isNotEmpty()) {
                            val next = JSONArray(presets.toString())
                            next.getJSONObject(i).put("name", renameText.trim())
                            if(project.setPresets(next)) { renamePreset = -1; refresh() }
                            else presetStatus = "Could not rename preset"
                        }
                    }
                }
            }
        }
        if(deletePreset in 0 until presets.length()) AlertDialog(containerColor=t.surface, 
            onDismissRequest = { deletePreset = -1 },
            title = { Text("Delete preset?") },
            text = { Text(presets.optJSONObject(deletePreset)?.optString("name").orEmpty()) },
            confirmButton = { Pill("Delete", t) {
                val next = JSONArray()
                for(j in 0 until presets.length()) if(j != deletePreset) next.put(presets.opt(j))
                if(project.setPresets(next)) { deletePreset = -1; refresh() }
                else presetStatus = "Could not remove preset"
            } }, dismissButton = { Pill("Cancel", t) { deletePreset = -1 } })
        if (pendingLoad in 0 until presets.length()) {
            AlertDialog(containerColor=t.surface, onDismissRequest = { pendingLoad = -1 },
                title = { Text("Replace current design?") },
                text = { Text("Loading this preset replaces the current local project. Export a backup first if needed.") },
                confirmButton = { Pill("Load preset", t, true) {
                    val preset=presets.optJSONObject(pendingLoad)
                    val restored = NativePresetRestore.load(project,prefs,preset)
                    presetStatus = if (restored) "Preset loaded"
                        else "Preset restore failed; previous design retained"
                    if (restored) {
                        if(preset?.has("fxId")==true)
                            selectedEffect.intValue=preset.optInt("fxId",selectedEffect.intValue)
                        refreshLoadedProject()
                    }
                    pendingLoad = -1
                } },
                dismissButton = { Pill("Cancel", t) { pendingLoad = -1 } })
        }
    }
    @Composable
    private fun LayoutPage(t: Tone) {
        val modelRevision = revision.intValue
        val zoneCount = remember(modelRevision) { project.zoneCount() }
        Title("Layout", t)
        EffectsPreview(t, selectedEffect.intValue)
        var name by remember(projectLoadRevision.intValue) { mutableStateOf(project.name()) }
        var editingId by remember(projectLoadRevision.intValue) {
            mutableStateOf<String?>(null) // collapse cards on Layout page entry
        }
        var deleteId by remember { mutableStateOf<String?>(null) }
        var choosingDelete by remember { mutableStateOf(false) }
        var layoutMessage by remember { mutableStateOf("") }
        Panel(t) {
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                Text("Project Layout",Modifier.weight(1f),color=t.fg,fontSize=16.sp,fontWeight=FontWeight.Bold)
                Caption(project.pixelCount().toString()+" LEDs",t)
            }
            Field(name,"Project name",t) {
                name=it
                if(it.trim().isNotEmpty()) {
                    if (project.name(it)) refresh()
                    else name = project.name() // restore last committed value
                }
            }
            if(layoutMessage.isNotEmpty()) Caption(layoutMessage,t)
            for(i in 0 until zoneCount) {
                val z=project.zone(i) ?: continue
                val id=z.optString("id")
                val linkedOutput=z.optInt("physicalOutput",-1)
                val linkedSection=z.optInt("physicalSection",-1)
                val length=z.optInt("length")
                val start=if(linkedOutput<0) z.optInt("start") else
                    (0 until i).mapNotNull { project.zone(it) }
                        .filter { it.optInt("physicalOutput",-1)==linkedOutput }.sumOf { it.optInt("length") }
                Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp))
                    .background(t.raised).padding(12.dp),verticalArrangement=Arrangement.spacedBy(10.dp)) {
                    Row(Modifier.fillMaxWidth().clickable { editingId=if(editingId==id) null else id },
                        verticalAlignment=Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text(z.optString("name"),color=t.fg,fontSize=14.sp,fontWeight=FontWeight.SemiBold)
                            Caption((if(linkedOutput<0) "Unassigned" else "Output "+(linkedOutput+1))+
                                " · Start $start · $length LEDs",t)
                        }
                        UiIcon(if(editingId==id) "up" else "down",t.muted)
                    }
                    if(editingId==id) {
                        var label by remember(id) { mutableStateOf(z.optString("name")) }
                        var count by remember(id) { mutableStateOf(length.toString()) }
                        var reverse by remember(id) { mutableStateOf(z.optBoolean("reverse")) }
                        var outputOpen by remember(id) { mutableStateOf(false) }
                        var advanced by remember(id) { mutableStateOf(false) }
                        var selectedPixel by remember(id) { mutableIntStateOf(0) }
                        fun applyLiveZone(): Boolean {
                            val n=count.toIntOrNull() ?: return false
                            if(n !in 1..10000 || label.trim().isEmpty()) return false
                            if(!project.editZone(id,label,n,reverse)) {
                                layoutMessage="Could not save zone";return false
                            }
                            layoutMessage="";refresh();return true
                        }
                        Caption("Zone name",t)
                        Field(label,"Zone name",t) { label=it;applyLiveZone() }
                        Caption("Output",t)
                        Box(Modifier.fillMaxWidth().height(40.dp).clip(RoundedCornerShape(99.dp))
                            .border(1.dp,t.muted.copy(alpha=0.45f),RoundedCornerShape(99.dp))
                            .clickable { outputOpen=!outputOpen }.padding(horizontal=14.dp),contentAlignment=Alignment.CenterStart) {
                            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                                Text(if(linkedOutput<0) "Unassigned" else "Output "+(linkedOutput+1),Modifier.weight(1f),color=t.fg,fontSize=14.sp)
                                UiIcon("down",t.muted)
                            }
                        }
                        if(outputOpen) Row(horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                            Pill("Unassigned",t,linkedOutput<0) {
                                if(project.mapZoneToOutputSlot(id,-1,-1)) { refresh();outputOpen=false }
                            }
                            (0..1).forEach { output ->
                                Pill("Output "+(output+1),t,linkedOutput==output) {
                                    val occupied=(0 until project.zoneCount()).mapNotNull { project.zone(it) }
                                        .filter { it.optString("id")!=id && it.optInt("physicalOutput",-1)==output }
                                        .map { it.optInt("physicalSection",-1) }.toSet()
                                    val slot=if(linkedOutput==output) linkedSection else (0..7).firstOrNull { it !in occupied }
                                    if(slot!=null && project.mapZoneToOutputSlot(id,output,slot)) {
                                        refresh();outputOpen=false
                                    } else layoutMessage="Output has no free zone slot"
                                }
                            }
                        }
                        val n=count.toIntOrNull() ?: 1
                        Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.SpaceBetween) {
                            Text("Pixel count",color=t.fg,fontSize=13.sp);Text(n.toString(),color=t.fg,fontSize=13.sp)
                        }
                        CapsuleSlider(n, 1, 10000, t,
                            change={ count=it.toString();applyLiveZone() },
                            finished={ applyLiveZone() })
                        Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically,
                            horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                            PixelStepPill("−",t) { count=(n-1).coerceAtLeast(1).toString();applyLiveZone() }
                            PixelStepPill("+",t) { count=(n+1).coerceAtMost(10000).toString();applyLiveZone() }
                            Spacer(Modifier.weight(1f))
                            Row(Modifier.clickable { reverse=!reverse;applyLiveZone() },
                                verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                                Canvas(Modifier.size(20.dp)) {
                                    drawCircle(t.muted.copy(alpha=0.5f),style=Stroke(1.dp.toPx()))
                                    if(reverse) drawCircle(t.fg,radius=size.minDimension*0.3f)
                                }
                                Text("Reverse",color=t.fg,fontSize=14.sp)
                            }
                        }
                        Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                            Pill("+ Add",t) {
                                val created = addZoneAfter(i)
                                if (created != null) { editingId=created; layoutMessage="" }
                                else layoutMessage="Could not add zone"
                            }
                            Pill("Remove",t) { deleteId=id }
                        }
                        // Prior exact-pixel authoring remains available in the deeper editor.
                        Pill(if(advanced) "Close pixel editor" else "Pixel editor",t) { advanced=!advanced }
                        if(advanced) {
                            Caption(project.previewZoneOffsets(id, n),t)
                            ZoneDotPreview(t,n,reverse)
                            val pixelLimit=n.coerceAtLeast(1)-1
                            val activePixel=selectedPixel.coerceIn(0,pixelLimit)
                            ValueSlider("Pixel",activePixel,pixelLimit.coerceAtLeast(1),t) { selectedPixel=it.coerceIn(0,pixelLimit) }
                            PaletteEditor("Pixel "+activePixel,android.graphics.Color.parseColor(project.pixelColor(id,activePixel)),t) { color ->
                                val hex=String.format("#%06X",color and 0xffffff)
                                if(project.setPixelColor(id,activePixel,hex)) refresh()
                            }
                            Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                                Pill("Move ↑",t) { if(project.moveZone(i,i-1)) refresh() }
                                Pill("Move ↓",t) { if(project.moveZone(i,i+1)) refresh() }
                                Pill("Clear pixel",t) { if(project.clearPixelColor(id,activePixel)) refresh() }
                            }
                        }
                    }
                }
            }
            if(zoneCount==0) Pill("+ Add",t) {
                val created=addZoneAfter(-1)
                if (created!=null) { editingId=created; layoutMessage="" }
                else layoutMessage="Could not add zone"
            }
        }
        // Actual hardware output/section assignment belongs beside Layout.
        // Saving to CORE still requires an explicit authenticated confirmation.
        HardwareOutputEditor(t)
        if (choosingDelete) AlertDialog(containerColor=t.surface, 
            onDismissRequest = { choosingDelete = false },
            title = { Text("Choose zone to delete") },
            text = {
                Column(Modifier.heightIn(max = 340.dp).verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    (0 until project.zoneCount()).forEach { index ->
                        val zone = project.zone(index) ?: return@forEach
                        val id = zone.optString("id")
                        Pill(zone.optString("name"), t) {
                            choosingDelete = false
                            deleteId = id // use existing second-stage confirmation
                        }
                    }
                }
            },
            confirmButton = { Pill("Cancel", t) { choosingDelete = false } })
        deleteId?.let { id ->
            AlertDialog(containerColor=t.surface, onDismissRequest = { deleteId = null }, title = { Text("Remove this zone?") },
                text = { Text("Only assignments to this zone will be removed.") },
                confirmButton = { Pill("Remove", t, true) {
                    if (project.removeZone(id)) {
                        deleteId = null
                        refresh()
                    }
                } }, dismissButton = { Pill("Cancel", t) { deleteId = null } })
        }
    }
    private fun addZoneAfter(index: Int): String? {
        val position = index + 1
        if (!project.addZoneAt("Zone " + (project.zoneCount() + 1), 30, position))
            return null
        val id=project.zone(position)?.optString("id")?.takeIf { it.isNotEmpty() }
        refresh()
        return id
    }
    @Composable
    private fun TriggersPage(t: Tone) {
        val modelRevision = revision.intValue
        // Snapshots refresh after each committed edit, including while the
        // existing multi-select editor stays open.
        val profiles = remember(modelRevision) { JSONObject(project.profiles().toString()) }
        var selections by remember { mutableStateOf(setOf<String>()) }
        var editing by remember { mutableStateOf(false) }
        var adding by remember { mutableStateOf(false) }
        var newTrigger by remember { mutableStateOf("") }
        var triggerMessage by remember { mutableStateOf("") }
        var triggerToDelete by remember { mutableStateOf<String?>(null) }
        val names = profiles.names()
        val keys = (0 until (names?.length() ?: 0))
            .mapNotNull { names?.optString(it) }
            .filterNot { it == "Startup" || it == "Shutdown" || it == "Show" }
            .sortedWith(compareBy({ name ->
                val order = listOf("Park", "Brake", "Left Turn", "Right Turn", "Hazards", "Reverse")
                val index = order.indexOf(name)
                if (index >= 0) index else 100
            }, { it }))
        Title("Triggers",t)
        EffectsPreview(t,selectedEffect.intValue)
        if(triggerMessage.isNotEmpty()) Caption(triggerMessage,t)
        Panel(t) {
        Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
            Text("Triggers",Modifier.weight(1f),color=t.fg,fontSize=16.sp,fontWeight=FontWeight.Bold)
            Pill("+ Add",t) { adding=true }
        }
        keys.forEach { name ->
            val profile = profiles.optJSONObject(name) ?: project.trigger(name)
            val selected = name in selections
            val assigned = profile.optJSONArray("zones")?.length() ?: 0
            Row(Modifier.fillMaxWidth().heightIn(min=44.dp)
                .clip(RoundedCornerShape(13.dp))
                .background(if(selected) t.raised else t.surface)
                .clickable { selections=if(selected) selections-name else selections+name },
                verticalAlignment=Alignment.CenterVertically,
                horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                Canvas(Modifier.size(16.dp)) {
                    drawCircle(if(selected) t.fg else t.muted)
                    if (selected) {
                        // Check remains inside the original 16dp circle.
                        val unit = size.minDimension / 16f
                        drawLine(t.bg, Offset(4f*unit, 8f*unit),
                            Offset(7f*unit, 11f*unit), strokeWidth = 2f*unit)
                        drawLine(t.bg, Offset(7f*unit, 11f*unit),
                            Offset(12.5f*unit, 5.5f*unit), strokeWidth = 2f*unit)
                    }
                }
                Column(Modifier.weight(1f)) {
                    Text(name,color=t.fg,fontSize=14.sp,fontWeight=FontWeight.SemiBold)
                    Caption(profile.optString("pattern", "Solid") +
                        if(assigned > 0) " · $assigned zones" else "", t)
                }
                IconAction("Edit " + name, "edit", t) {
                    selections = setOf(name); editing = true
                }
                // Only user-created triggers may be deleted.
                if (profile.optString("kind") == "trigger-custom")
                    IconAction("Delete " + name, "delete", t) { triggerToDelete = name }
                // The current protocol has no universal profile-test command.
                // Preserve a disabled play target rather than target wrong zones.
                Box(Modifier.size(width=44.dp,height=28.dp)
                    .clip(RoundedCornerShape(99.dp)).background(t.raised)
                    .semantics { contentDescription = "Test " + name + " unavailable"; disabled() },
                    contentAlignment=Alignment.Center) {
                    UiIcon("play",t.muted)
                }
            }
        }
        Pill("Edit Selected", t, selections.isNotEmpty(), enabled=selections.isNotEmpty()) {
            if(selections.isNotEmpty()) editing = true
        }
        }
        if(adding) AlertDialog(containerColor=t.surface, 
            onDismissRequest={ adding=false },
            title={ Text("Add trigger") },
            text={ Field(newTrigger,"Custom trigger name",t){ newTrigger=it.take(32) } },
            confirmButton={ Pill("Add",t,true) {
                if(project.customTrigger(newTrigger)){
                    selections=setOf(newTrigger)
                    newTrigger=""
                    adding=false
                    refresh()
                }else triggerMessage="Could not add trigger"
            } },
            dismissButton={ Pill("Cancel",t){ adding=false } })
        if (triggerToDelete != null) AlertDialog(containerColor=t.surface, 
            onDismissRequest = { triggerToDelete = null },
            title = { Text("Delete custom trigger?") },
            text = { Text("Remove " + triggerToDelete + " from this project?") },
            confirmButton = { Pill("Delete", t, true) {
                val target = triggerToDelete
                if(target != null && project.deleteTrigger(target)) {
                    selections = selections - target
                    editing = false
                    triggerToDelete = null
                    refresh()
                } else triggerMessage = "Could not delete custom trigger"
            } },
            dismissButton = { Pill("Cancel", t) { triggerToDelete = null } })
        if (editing) AlertDialog(containerColor=t.surface, 
            onDismissRequest = { editing = false },
            title = { Text("Assign zones to " + selections.size + " triggers") },
            text = {
                Column(Modifier.heightIn(max=400.dp).verticalScroll(rememberScrollState()),
                    verticalArrangement=Arrangement.spacedBy(8.dp)) {
                    val first=selections.firstOrNull()?.let { profiles.optJSONObject(it) }
                    val initialFx=NativeEffectCatalog.byName(first?.optString("pattern","Solid") ?: "Solid")
                    val initialColor=first?.optString("color","#ffffff") ?: "#ffffff"
                    fun roleHex(role: Int): String = when(role){
                        0->first?.optString("color","#ffffff") ?: "#ffffff"
                        1->first?.optString("secondary","#000000") ?: "#000000"
                        else->first?.optString("accent","#ffffff") ?: "#ffffff"
                    }
                    fun roleArgb(role:Int):Int =
                        runCatching { android.graphics.Color.parseColor(roleHex(role)) }
                            .getOrDefault(0xffffffff.toInt())
                    ApprovedWledEffectsEditor(
                        selectedId = initialFx.id,
                        effectOptions = NativeEffectCatalog.generalEffects(),
                        scopeKey = "triggers:" + selections.sorted().joinToString("|"),
                        prefs = prefs,
                        paletteFor = { id ->
                            NativePaletteEngine.lookup(project.raw(),id,roleArgb(0),
                                roleArgb(1),null,roleArgb(2))
                        },
                        readSetting = { field, fallback ->
                            if(field=="brightness")
                                (first?.optInt("brightness",85) ?: 85)*255/100
                            else first?.optInt(field,fallback) ?: fallback
                        },
                        readColor = { role -> roleArgb(role) },
                        readPalette = { first?.optInt("paletteId",0) ?: 0 },
                        onSelected = { selected ->
                            // An unselected physical trigger stays assigned a safe
                            // local Solid design, never a disabled vehicle signal.
                            val fx=NativeEffectCatalog.byId(selected ?: 0).name
                            if(project.editTriggerStyle(selections,fx,null)) refresh()
                            else triggerMessage="Could not save trigger effect"
                        },
                        onParam = { _,field,value ->
                            val fieldValue=if(field=="brightness")value*100/255 else value
                            val saved=if(field in setOf("brightness","speed","intensity"))
                                project.editTriggerParameter(selections,field,fieldValue)
                            else project.editTriggerEditorField(selections,field,fieldValue)
                            if(saved) refresh()
                            else triggerMessage="Could not save trigger parameter"
                        },
                        onColor = { _,slot,rgb ->
                            val hex=String.format("#%06X",rgb and 0xffffff)
                            val saved=if(slot==0) project.editTriggerStyle(selections,null,hex)
                                else project.editTriggerEditorField(selections,
                                    if(slot==1)"secondary" else "accent",hex)
                            if(saved) refresh()
                            else triggerMessage="Could not save trigger color"
                        },
                        onPalette = { _,id ->
                            if(project.editTriggerEditorField(selections,"paletteId",id)) refresh()
                            else triggerMessage="Could not save trigger palette"
                        }
                    )
                    if(project.zoneCount()==0) Caption("Add zones in Layout first.",t)
                    for(i in 0 until project.zoneCount()) {
                        val zone=project.zone(i) ?: continue
                        val zoneId=zone.optString("id")
                        val checked=selections.all { name ->
                            val assigned=profiles.optJSONObject(name)?.optJSONArray("zones")
                            (0 until (assigned?.length() ?: 0))
                                .any { assigned?.optString(it)==zoneId }
                        }
                        Pill(zone.optString("name") + (if(checked) " ✓" else ""),t,checked) {
                            if(project.assignMany(selections,zoneId,!checked)) refresh()
                            else triggerMessage="Could not save zone assignments"
                        }
                    }
                }
            },
            confirmButton = { Pill("Done",t,true) { editing=false } },
            dismissButton = { Pill("Close",t) { editing=false } })
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
            snapshot.revision in 1..65534) AlertDialog(containerColor=t.surface, 
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
        val modelRevision = revision.intValue
        val eventProfiles = remember(modelRevision) {
            listOf("Startup", "Shutdown").associateWith { JSONObject(project.trigger(it).toString()) }
        }
        Title("Auto Events", t)
        var selectedEvent by remember { mutableStateOf("Startup") }
        // The event's own saved animation drives this preview; it is not the
        // last-used Show Modes effect. A Preview tap stays on Auto Events.
        val eventPreviewId=autoEventPreviewEffect.intValue.takeIf { it >= 0 }
            ?: NativeEffectCatalog.byName(eventProfiles.getValue(selectedEvent)
                .optString("eventFx","Solid")).id
        val eventPreview=eventProfiles.getValue(selectedEvent)
        EffectsPreview(t, eventPreviewId, animateOffline=true, settingsOverride=intArrayOf(
            android.graphics.Color.parseColor(eventPreview.optString("color","#ffffff")),
            android.graphics.Color.parseColor(eventPreview.optString("secondary","#000000")),
            eventPreview.optInt("speed",128),eventPreview.optInt("intensity",128),
            eventPreview.optInt("brightness",100)*255/100,eventPreview.optInt("paletteId",0)))
        var eventEditing by remember { mutableStateOf<String?>(null) }
        var choosing by remember { mutableStateOf<String?>(null) }
        var search by remember { mutableStateOf("") }
        var eventMessage by remember { mutableStateOf("") }
        if (eventMessage.isNotEmpty()) Caption(eventMessage, t)
        fun applyAutoEvent(event: String, field: String, value: Any): Boolean {
            val saved = project.setAutoEvent(event, field, value)
            eventMessage = if (saved) "" else "Could not save " + event + " settings"
            if (saved) refresh()
            // One-shot events replay their own source frames from t=0
            // after a locally saved edit, never restart a physical Show.
            if (saved && offlinePreview.value && selectedEvent == event)
                offlineReplay.intValue++
            return saved
        }
        listOf("Startup", "Shutdown").forEach { key ->
            val event = eventProfiles.getValue(key)
            Panel(t,minHeight=70.dp,fill=if(selectedEvent==key) (if(t.bg==Color.Black) Color(0xFF272727) else t.raised) else t.surface) {
                val enabled = event.optBoolean("eventUiEnabled", false)
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f).clickable {
                        if(selectedEvent!=key) {
                            autoEventPreviewEffect.intValue=-1
                            if (offlinePreview.value) offlineReplay.intValue++
                        }
                        selectedEvent=key
                        eventEditing=if(eventEditing==key) null else key
                    }) {
                        Text(key, color = t.fg, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                        Caption(event.optString("eventFx", "Solid"), t)
                    }
                    IconAction("Edit " + key, "edit", t) {
                        if(selectedEvent!=key) {
                            autoEventPreviewEffect.intValue=-1
                            if (offlinePreview.value) offlineReplay.intValue++
                        }
                        selectedEvent=key
                        eventEditing = if (eventEditing == key) null else key
                    }
                    Pill(if (enabled) "On" else "Off", t, enabled) {
                        applyAutoEvent(key, "eventUiEnabled", !enabled)
                    }
                }
                if (eventEditing == key) {
                // Dedicated Startup/Shutdown authoring reuses the exact
                // approved WLED+ interface. Local event fields never write
                // Show Modes settings, BLE, or trigger safety mappings.
                val eventEffect=NativeEffectCatalog.byName(event.optString("eventFx","Solid"))
                fun eventColor(slot:Int):Int =
                    runCatching { android.graphics.Color.parseColor(
                        event.optString(when(slot){0->"color";1->"secondary";else->"accent"},
                            if(slot==0)"#ffffff" else "#000000")) }
                        .getOrDefault(0xffffffff.toInt())
                ApprovedWledEffectsEditor(
                    selectedId=eventEffect.id,
                    effectOptions=NativeEffectCatalog.autoEventChoices(key),
                    scopeKey="auto-event:"+key,
                    prefs=prefs,
                    paletteFor={ id -> NativePaletteEngine.lookup(project.raw(),id,
                        eventColor(0),eventColor(1),null,eventColor(2)) },
                    readSetting={ field,fallback ->
                        if(field=="brightness")event.optInt("brightness",100)*255/100
                        else event.optInt(field,fallback)
                    },
                    readColor={ slot -> eventColor(slot) },
                    readPalette={ event.optInt("paletteId",0) },
                    onSelected={ id ->
                        val fx=NativeEffectCatalog.byId(id ?: 0)
                        if(NativeEffectCatalog.isAllowedForAutoEvent(key,fx.id) &&
                           applyAutoEvent(key,"eventFx",fx.name))
                            autoEventPreviewEffect.intValue=-1
                    },
                    onParam={ _,field,value ->
                        applyAutoEvent(key,field,
                            if(field=="brightness")value*100/255 else value)
                    },
                    onColor={ _,slot,rgb ->
                        applyAutoEvent(key,when(slot){0->"color";1->"secondary";else->"accent"},
                            String.format("#%06X",rgb and 0xffffff))
                    },
                    onPalette={ _,id -> applyAutoEvent(key,"paletteId",id) }
                )
                Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                    Pill("Smooth Fade",t,event.optInt("eventUiFadeMs",0)>0) {
                        applyAutoEvent(key,"eventUiFadeMs",1200)
                    }
                    Pill("Instant Snap",t,event.optInt("eventUiFadeMs",0)==0) {
                        applyAutoEvent(key,"eventUiFadeMs",0)
                    }
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
        }
        choosing?.let { key ->
            AlertDialog(containerColor=t.surface, onDismissRequest = { choosing = null },
                title = { Text("Select " + key + " animation") },
                text = {
                    Column(Modifier.heightIn(max = 360.dp).verticalScroll(rememberScrollState()),
                        verticalArrangement = Arrangement.spacedBy(7.dp)) {
                        Field(search, "Search effects", t) { search = it }
                        // Show all matching choices; the old 15-item
                        // alphabetic cap hid Materialize, Random Materialize,
                        // Smooth Fade and Strobe (owner Test 86).
                        NativeEffectCatalog.autoEventChoices(key)
                            .filter { it.name.contains(search, ignoreCase = true) }
                            .forEach { item ->
                                Pill(item.name, t) {
                                    if (applyAutoEvent(key, "eventFx", item.name)) {
                                        autoEventPreviewEffect.intValue=-1
                                        choosing = null
                                    }
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
        if (deleteConfirm) AlertDialog(containerColor=t.surface, 
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
    private fun UiIcon(kind: String, color: Color) {
        Canvas(Modifier.size(18.dp)) {
            val u=size.width/24f
            val w=1.8f*u
            fun line(x: Float,y: Float,xx: Float,yy: Float) =
                drawLine(color,Offset(x*u,y*u),Offset(xx*u,yy*u),w)
            when(kind) {
                "bluetooth" -> { line(11f,2f,11f,22f);line(11f,2f,18f,8f)
                    line(18f,8f,5f,18f);line(5f,6f,18f,16f);line(18f,16f,11f,22f) }
                "shield" -> { val p=Path().apply { moveTo(12*u,2*u);lineTo(21*u,6*u)
                    lineTo(20*u,15*u);quadraticTo(18*u,21*u,12*u,23*u)
                    quadraticTo(6*u,21*u,4*u,15*u);lineTo(3*u,6*u);close() }
                    drawPath(p,color,style=Stroke(w));line(8f,12f,11f,15f);line(11f,15f,16f,9f) }
                "chevron" -> { line(9f,4f,16f,12f);line(16f,12f,9f,20f) }
                "down" -> { line(5f,9f,12f,16f);line(12f,16f,19f,9f) }
                "up" -> { line(5f,15f,12f,8f);line(12f,8f,19f,15f) }
                "pause" -> { drawCircle(color);drawLine(Color.Black,Offset(10*u,8*u),Offset(10*u,16*u),w)
                    drawLine(Color.Black,Offset(14*u,8*u),Offset(14*u,16*u),w) }
                "play" -> { drawCircle(color,style=Stroke(w));val p=Path().apply {
                    moveTo(10*u,7*u);lineTo(17*u,12*u);lineTo(10*u,17*u);close() };drawPath(p,color) }
                "refresh", "reset" -> { drawArc(color,40f,290f,false,style=Stroke(w))
                    line(20f,2f,20f,8f);line(20f,8f,14f,8f) }
                "settings" -> { line(2f,7f,22f,7f);line(2f,17f,22f,17f)
                    drawCircle(color,3*u,Offset(8*u,7*u),style=Stroke(w))
                    drawCircle(color,3*u,Offset(16*u,17*u),style=Stroke(w)) }
                "controller" -> { line(5f,4f,19f,4f);line(5f,4f,2f,10f)
                    line(19f,4f,22f,10f);line(2f,10f,22f,10f);line(3f,15f,21f,15f);line(4f,20f,20f,20f) }
                "firmware" -> { drawRect(color,Offset(5*u,5*u),androidx.compose.ui.geometry.Size(14*u,14*u),style=Stroke(w))
                    for(i in listOf(8f,16f)){line(i,1f,i,5f);line(i,19f,i,23f);line(1f,i,5f,i);line(19f,i,23f,i)} }
                "appearance" -> { drawCircle(color,9*u,Offset(12*u,12*u),style=Stroke(w))
                    for(pt in listOf(Offset(8*u,8*u),Offset(14*u,6*u),Offset(18*u,11*u),Offset(7*u,14*u)))
                        drawCircle(color,1.3f*u,pt) }
            }
        }
    }
    @Composable
    private fun IconAction(label: String, kind: String, t: Tone, action: () -> Unit) {
        Box(Modifier.size(40.dp).clip(RoundedCornerShape(99.dp))
            .semantics { contentDescription = label }.clickable(onClick = action),
            contentAlignment = Alignment.Center) {
            Canvas(Modifier.size(18.dp)) {
                val u = size.width / 24f
                if (kind == "edit") {
                    val p = Path().apply {
                        moveTo(4*u, 16*u); lineTo(16*u, 4*u); lineTo(20*u, 8*u)
                        lineTo(8*u, 20*u); lineTo(3*u, 21*u); close()
                    }
                    drawPath(p, t.muted, style = Stroke(1.8f*u))
                    drawLine(t.muted, Offset(13*u,7*u), Offset(17*u,11*u), 1.8f*u)
                } else {
                    drawLine(t.muted,Offset(5*u,7*u),Offset(19*u,7*u),2*u)
                    drawLine(t.muted,Offset(9*u,4*u),Offset(15*u,4*u),2*u)
                    drawLine(t.muted,Offset(7*u,9*u),Offset(8*u,20*u),2*u)
                    drawLine(t.muted,Offset(17*u,9*u),Offset(16*u,20*u),2*u)
                    drawLine(t.muted,Offset(8*u,20*u),Offset(16*u,20*u),2*u)
                }
            }
        }
    }
    @Composable
    private fun SettingsRow(label: String, subtitle: String, t: Tone,
                            actionLabel: String = "›", rowHeight: Int = 59, action: () -> Unit) {
        Row(Modifier.fillMaxWidth().heightIn(min = rowHeight.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            UiIcon(when(label) { "Controller" -> "bluetooth"; "Authorization" -> "shield"
                "Active Controller" -> "controller"; "Appearance" -> "appearance"
                "Firmware" -> "firmware"; else -> "reset" },t.muted)
            Column(Modifier.weight(1f)) {
                Text(label, color = t.fg, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                Caption(subtitle, t)
            }
            if(actionLabel=="›") Box(Modifier.size(40.dp).clickable(onClick=action)
                .semantics { contentDescription="Open " + label },contentAlignment=Alignment.Center) {
                UiIcon("chevron",t.muted)
            } else Pill(actionLabel, t, enabled=actionLabel!="Authorize" || (ble.isConnected() && !ble.canControl()), strong=actionLabel=="Connect", action = action)
        }
    }
    @Composable
    private fun SettingsPage(t: Tone, changePage: (Int) -> Unit,
                             light: Boolean, toggleAppearance: () -> Unit) {
        val connectionPhase = remember(bleMessage.value) { ble.connectionPhase().replaceFirstChar { it.uppercase() } }
        Title("Settings", t)
        var controllers by remember { mutableStateOf(false) }
        var appearance by remember { mutableStateOf(false) }
        var diagnostics by remember { mutableStateOf(false) }
        Panel(t) {
            Text("Connection", color=t.fg, fontSize=16.sp, fontWeight=FontWeight.Bold)
            SettingsRow("Controller", connectionPhase, t, "Connect") {
                foundDevices.clear(); ble.scanHeadless(); controllers = true
            }
            Box(Modifier.fillMaxWidth().height(1.dp).background(t.raised))
            SettingsRow("Authorization", if(ble.canControl()) "Authorized" else "Not authorized", t, "Authorize") {
                ble.authorizeOrPair()
            }
            Box(Modifier.fillMaxWidth().height(1.dp).background(t.raised))
            SettingsRow("Active Controller", if(ble.canControl()) "Connected controller" else "No controller selected", t) {
                controllers = !controllers
            }
            if(controllers) foundDevices.forEach { (address,label) ->
                Pill(label, t) { ble.connectFoundDevice(address); controllers=false }
            }
        }
        Panel(t) {
            Text("App Preferences", color=t.fg, fontSize=16.sp, fontWeight=FontWeight.Bold)
            SettingsRow("Appearance", if(light) "Light mode" else "Dark mode", t, rowHeight=52) { appearance = !appearance }
            if(appearance) Pill(if(light) "Dark mode" else "Light mode",t) { toggleAppearance() }
        }
        Panel(t) {
            Text("System", color=t.fg, fontSize=16.sp, fontWeight=FontWeight.Bold)
            SettingsRow("Firmware", firmwareVersion.value ?: "Version unavailable", t, rowHeight=46) { diagnostics = !diagnostics }
            Box(Modifier.fillMaxWidth().height(1.dp).background(t.raised))
            // No reset command exists in the pinned CORE interface. Do not
            // present a local document clear as a successful controller reset.
            SettingsRow("Reset Configuration", "Restore controller defaults", t, rowHeight=46) {
                outputMessage.value = "Controller reset is unavailable in the current CORE protocol"
                diagnostics = true
            }
        }
        if(diagnostics) SettingsDiagnostics(t, changePage)
    }
    @Composable
    private fun SettingsDiagnostics(t: Tone, changePage: (Int) -> Unit) {
        Title("Settings", t)
        // Developer testing is deliberately isolated from universal trigger
        // authoring. This is a local UI test; it does not assert GPIO or write NVS.
        var simulatedTriggers by remember { mutableStateOf(setOf<String>()) }
        Panel(t) {
            Title("Developer · Trigger Testing", t)
            Caption("Local simulation only · physical trigger inputs remain CORE-authoritative.", t)
            val defaults = listOf("Park","Brake","Left Turn","Right Turn","Hazards","Reverse")
            defaults.chunked(2).forEach { pair ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                    pair.forEach { name ->
                        Box(Modifier.weight(1f)) {
                            Pill((if(name in simulatedTriggers) "■ Stop " else "▷ Test ") + name,
                                t,name in simulatedTriggers) {
                                simulatedTriggers=if(name in simulatedTriggers)
                                    simulatedTriggers-name else simulatedTriggers+name
                            }
                        }
                    }
                }
            }
            Caption(simulatedTriggers.size.toString() + " simulated triggers active", t)
            if(simulatedTriggers.isNotEmpty())
                Pill("Clear test states",t) { simulatedTriggers=emptySet() }
        }
        Panel(t) {
            Text("Bluetooth · " + ble.connectionPhase(), color = t.fg)
            Caption(bleMessage.value, t)
            Caption("Authorize only on a physically present CORE bench controller.", t)
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                ConnectPill {
                    foundDevices.clear()
                    ble.connectFromApp(prefs.getString("preferred_ble_address", null))
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
        if (confirm) AlertDialog(containerColor=t.surface, onDismissRequest = { confirm = false },
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
                Pill("Output 1", t, index == 0) { index = 0; captureOutput.intValue = 0; invalidateCoreFrame() }
                Pill("Output 2", t, index == 1) { index = 1; captureOutput.intValue = 1; invalidateCoreFrame() }
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
            // Restore the approved pixel +/- controls beside the draft
            // slider. These change only RAM; Save still requires confirmation.
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                PixelStepPill("−",t) {
                    val n=(pixelCount-1).coerceAtLeast(1)
                    adjustOutput(n)
                    outputCountText=JSONObject(draftText).optInt("pixels",pixelCount).toString()
                }
                PixelStepPill("+",t) {
                    val n=(pixelCount+1).coerceAtMost(outputLimit)
                    adjustOutput(n)
                    outputCountText=JSONObject(draftText).optInt("pixels",pixelCount).toString()
                }
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
                Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                    PixelStepPill("−",t) {
                        val n=(count-1).coerceAtLeast(0)
                        adjustZone(i,n)
                        zoneCountText=JSONObject(draftText).getJSONArray("zones").getJSONObject(i).getInt("pixels").toString()
                        outputCountText=JSONObject(draftText).optInt("pixels",0).toString()
                    }
                    PixelStepPill("+",t) {
                        val n=(count+1).coerceAtMost(outputLimit)
                        adjustZone(i,n)
                        zoneCountText=JSONObject(draftText).getJSONArray("zones").getJSONObject(i).getInt("pixels").toString()
                        outputCountText=JSONObject(draftText).optInt("pixels",0).toString()
                    }
                }
                // Bidirectional association: choose a design zone here, or
                // choose its physical output/section from the expanded design card.
                val linkedId=(0 until project.zoneCount()).firstOrNull { localIndex ->
                    val local=project.zone(localIndex)
                    local?.optInt("physicalOutput",-1)==index &&
                        local.optInt("physicalSection",-1)==i
                }?.let { project.zone(it)?.optString("id") }
                val linkedName=(0 until project.zoneCount()).firstOrNull { localIndex ->
                    project.zone(localIndex)?.optString("id")==linkedId
                }?.let { project.zone(it)?.optString("name") }
                var choosingDesign by remember(index,i) { mutableStateOf(false) }
                Pill("Design zone: " + (linkedName ?: "Unassigned"),t) {
                    choosingDesign=!choosingDesign
                }
                if(choosingDesign) {
                    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                        for(localIndex in 0 until project.zoneCount()) {
                            val local=project.zone(localIndex) ?: continue
                            val localId=local.optString("id")
                            Pill(local.optString("name"),t,localId==linkedId) {
                                val newPixels=local.optInt("length",0)
                                val newTotal=pixelCount-count+newPixels
                                if(newPixels !in 1..outputLimit || newTotal !in 1..outputLimit) {
                                    outputMessage.value="Design zone does not fit this physical output"
                                } else if(project.mapZoneToOutputSlot(localId,index,i)) {
                                    // Update only the editable BLE draft; physical
                                    // outputs do not change until confirmed Save.
                                    edit { next ->
                                        val part=next.getJSONArray("zones").getJSONObject(i)
                                        part.put("name",local.optString("name"))
                                            .put("pixels",newPixels)
                                        next.put("pixels",newTotal)
                                    }
                                    zoneCountText=newPixels.toString()
                                    outputCountText=newTotal.toString()
                                    choosingDesign=false
                                    structureVersion++
                                    refresh()
                                } else outputMessage.value="Could not save local output assignment"
                            }
                        }
                    }
                }
                Caption("Physical section link is local until CORE Save/readback.",t)
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
        if (confirm) AlertDialog(containerColor=t.surface, 
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
