package com.opentail.android;
import org.junit.Test;
import static org.junit.Assert.*;
import org.json.JSONObject;
import org.json.JSONArray;

public class CoreBenchEvidenceTest {
    private JSONObject output(String role,int pixels)throws Exception {
        return new JSONObject().put("pixels",pixels).put("zones",new JSONArray()
            .put(new JSONObject().put("pixels",pixels).put("role",role)));
    }
    @Test public void completedBlackFrameIsNotMissing() {
        ExactLedFrame black=ExactLedFrame.fromCoreRgb888(0,7,new byte[90]);
        assertEquals("CORE frame · 30 LEDs · all black",CorePreviewEvidence.label(black,"Awaiting CORE frame",false));
        assertEquals("CORE frame black · design colors",CorePreviewEvidence.label(black,"",true));
        assertEquals("CORE frame · 1 LEDs",CorePreviewEvidence.label(
            ExactLedFrame.fromCoreRgb888(0,8,new byte[]{24,0,0}),"",false));
    }
    @Test public void captureFailuresAndLocalPaintCannotMasqueradeAsCore() {
        assertEquals("CORE did not complete the requested frame",CorePreviewEvidence.label(null,
            "CORE did not complete the requested frame",false));
        assertEquals("CORE did not complete the requested frame · design colors",
            CorePreviewEvidence.label(null,"CORE did not complete the requested frame",true));
        assertEquals("CORE frame not captured",CorePreviewEvidence.label(
            ExactLedFrame.fromProject(new int[]{0xffff0000}),"CORE frame not captured",false));
    }
    @Test public void unsavedLocalLayoutAndInertFactoryZoneCannotActivateHardware()throws Exception {
        assertEquals(0,CoreBenchTargets.enabledMask(new JSONObject[2],3,255));
        assertEquals(0,CoreBenchTargets.enabledMask(new JSONObject[]{output("off",1),null},3,255));
        assertEquals(0,CoreBenchTargets.enabledMask(new JSONObject[]{output("off",30),null},1,255));
        assertEquals(0,CoreBenchTargets.enabledMask(new JSONObject[]{output("unknown",30),null},1,255));
    }
    @Test public void onlyReadBackEnabledSelectedOutputsAndZonesAreTested()throws Exception {
        JSONObject a=output("decorative",30),b=output("brake",30);
        String before=a.toString();
        assertEquals(1,CoreBenchTargets.enabledMask(new JSONObject[]{a,b},1,255));
        assertEquals(2,CoreBenchTargets.enabledMask(new JSONObject[]{a,b},2,255));
        assertEquals(3,CoreBenchTargets.enabledMask(new JSONObject[]{a,b},3,255));
        assertEquals(0,CoreBenchTargets.enabledMask(new JSONObject[]{a,b},1,2));
        assertEquals(before,a.toString());
    }
}
