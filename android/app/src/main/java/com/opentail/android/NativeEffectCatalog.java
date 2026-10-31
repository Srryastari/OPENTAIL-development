package com.opentail.android;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/** OpenTail effect names and IDs for authoring only; renderer stays inside CORE. */
final class NativeEffectCatalog {
    static final class Effect {
        final int id;
        final String name;
        Effect(int id, String name) {this.id=id;this.name=name;}
        @Override public String toString(){return name;}
    }
    private static final Effect[] ALL = new Effect[] {
        new Effect(0, "Solid"),
        new Effect(1, "Blink"),
        new Effect(2, "Breathe"),
        new Effect(3, "Wipe"),
        new Effect(4, "Wipe Random"),
        new Effect(5, "Random Colors"),
        new Effect(6, "Sweep"),
        new Effect(7, "Dynamic"),
        new Effect(8, "Colorloop"),
        new Effect(9, "Rainbow"),
        new Effect(10, "Scan"),
        new Effect(11, "Scan Dual"),
        new Effect(12, "Fade"),
        new Effect(13, "Theater"),
        new Effect(14, "Theater Rainbow"),
        new Effect(15, "Running"),
        new Effect(16, "Saw"),
        new Effect(17, "Twinkle"),
        new Effect(18, "Dissolve"),
        new Effect(19, "Dissolve Rnd"),
        new Effect(20, "Sparkle"),
        new Effect(21, "Sparkle Dark"),
        new Effect(22, "Sparkle+"),
        new Effect(23, "Strobe"),
        new Effect(24, "Strobe Rainbow"),
        new Effect(25, "Strobe Mega"),
        new Effect(26, "Blink Rainbow"),
        new Effect(27, "Android"),
        new Effect(28, "Chase"),
        new Effect(29, "Chase Random"),
        new Effect(30, "Chase Rainbow"),
        new Effect(31, "Chase Flash"),
        new Effect(32, "Chase Flash Rnd"),
        new Effect(33, "Rainbow Runner"),
        new Effect(34, "Colorful"),
        new Effect(35, "Traffic Light"),
        new Effect(36, "Sweep Random"),
        new Effect(37, "Chase 2"),
        new Effect(38, "Aurora"),
        new Effect(39, "Stream"),
        new Effect(40, "Scanner"),
        new Effect(41, "Lighthouse"),
        new Effect(42, "Fireworks"),
        new Effect(43, "Rain"),
        new Effect(44, "Tetrix"),
        new Effect(45, "Fire Flicker"),
        new Effect(46, "Gradient"),
        new Effect(47, "Loading"),
        new Effect(48, "Rolling Balls"),
        new Effect(49, "Fairy"),
        new Effect(50, "Two Dots"),
        new Effect(51, "Fairytwinkle"),
        new Effect(52, "Running Dual"),
        new Effect(54, "Chase 3"),
        new Effect(55, "Tri Wipe"),
        new Effect(56, "Tri Fade"),
        new Effect(57, "Lightning"),
        new Effect(58, "ICU"),
        new Effect(59, "Multi Comet"),
        new Effect(60, "Scanner Dual"),
        new Effect(61, "Stream 2"),
        new Effect(62, "Oscillate"),
        new Effect(63, "Pride 2015"),
        new Effect(64, "Juggle"),
        new Effect(65, "Palette"),
        new Effect(66, "Fire 2012"),
        new Effect(67, "Colorwaves"),
        new Effect(68, "Bpm"),
        new Effect(69, "Fill Noise"),
        new Effect(70, "Noise 1"),
        new Effect(71, "Noise 2"),
        new Effect(72, "Noise 3"),
        new Effect(73, "Noise 4"),
        new Effect(74, "Colortwinkles"),
        new Effect(75, "Lake"),
        new Effect(76, "Meteor"),
        new Effect(78, "Railway"),
        new Effect(79, "Ripple"),
        new Effect(80, "Twinklefox"),
        new Effect(81, "Twinklecat"),
        new Effect(82, "Halloween Eyes"),
        new Effect(83, "Solid Pattern"),
        new Effect(84, "Solid Pattern Tri"),
        new Effect(85, "Spots"),
        new Effect(86, "Spots Fade"),
        new Effect(87, "Glitter"),
        new Effect(88, "Candle"),
        new Effect(89, "Fireworks Starburst"),
        new Effect(90, "Fireworks 1D"),
        new Effect(91, "Bouncing Balls"),
        new Effect(92, "Sinelon"),
        new Effect(93, "Sinelon Dual"),
        new Effect(94, "Sinelon Rainbow"),
        new Effect(95, "Popcorn"),
        new Effect(96, "Drip"),
        new Effect(97, "Plasma"),
        new Effect(98, "Percent"),
        new Effect(99, "Ripple Rainbow"),
        new Effect(100, "Heartbeat"),
        new Effect(101, "Pacifica"),
        new Effect(102, "Candle Multi"),
        new Effect(103, "Solid Glitter"),
        new Effect(104, "Sunrise"),
        new Effect(105, "Phased"),
        new Effect(106, "Twinkleup"),
        new Effect(107, "Noise Pal"),
        new Effect(108, "Sine"),
        new Effect(109, "Phased Noise"),
        new Effect(110, "Flow"),
        new Effect(111, "Chunchun"),
        new Effect(112, "Dancing Shadows"),
        new Effect(113, "Washing Machine"),
        new Effect(115, "Blends"),
        new Effect(116, "TV Simulator"),
        new Effect(117, "Dynamic Smooth"),
        new Effect(179, "Flow Stripe"),
        new Effect(184, "Wavesins"),
        new Effect(185, "Rocktaves"),
        new Effect(218, "Color Clouds"),
        new Effect(219, "Slow Transition"),
        new Effect(32768, "Embers"),
        new Effect(32769, "Materialize Sparks"),
        new Effect(32770, "Random Materialize"),
        new Effect(32771, "Smooth Fade"),
        new Effect(32772, "Plasma Sweep"),
        new Effect(32773, "Smooth Fade Out"),
        new Effect(32774, "Dimensional Rift"),
        new Effect(32775, "Meteor Storm"),
        new Effect(32776, "Helix Drive"),
        new Effect(32777, "Crystal Growth"),
        new Effect(32778, "Fusion Core"),
        new Effect(32779, "Portal Gate"),
        new Effect(32780, "Time Fracture"),
        new Effect(32781, "Aurora Surge"),
        new Effect(32782, "Chromatic Shatter"),
        new Effect(32783, "Vortex"),
        new Effect(32784, "Quantum Collapse"),
        new Effect(32785, "Reality Break"),
        new Effect(32786, "Event Horizon"),
        new Effect(32787, "Singularity Shutdown")
    };
    private static final List<Effect> SORTED;
    static {
        ArrayList<Effect> effects = new ArrayList<>();
        Collections.addAll(effects, ALL);
        effects.sort(Comparator.comparing(e -> e.name.toLowerCase(java.util.Locale.ROOT)));
        SORTED = Collections.unmodifiableList(effects);
    }
    static List<Effect> all(){return SORTED;}
    /** These are OpenTail-specific transition animations, authored ONLY in
     * Auto Events (Startup/Shutdown). Standard WLED Strobe #23 remains a
     * general 1D effect and is also available to Auto Events. */
    static boolean autoEventOnly(int id) {
        return id==32769 || id==32770 || id==32771 || id==32773 ||
               id==32787;
    }

    /** General lighting effects are allowed in Shows and Triggers, while
     * startup/shutdown transitions belong exclusively to Auto Events. */
    static List<Effect> generalEffects() {
        ArrayList<Effect> general = new ArrayList<>();
        for (Effect effect : SORTED) if (!autoEventOnly(effect.id)) general.add(effect);
        return Collections.unmodifiableList(general);
    }
    /** General proprietary animations belong in the unified Show Modes
     * catalog (and can be assigned to Triggers). Auto Events only hosts
     * dedicated transitions plus eligible standard WLED effects. The owner's
     * WLED Plasma #97 is also a general Show Modes effect. */
    private static boolean showModesOnly(int id) {
        return id == 97 || (id >= 32768 && id <= 32787 && !autoEventOnly(id));
    }
    /** Keep eligible transitions first, without leaking general proprietary
     * animations into the Startup/Shutdown chooser. */
    static List<Effect> autoEventChoices() {
        ArrayList<Effect> choices = new ArrayList<>();
        for (Effect effect : SORTED)
            if (!showModesOnly(effect.id)) choices.add(effect);
        choices.sort(Comparator.comparing((Effect e) -> !autoEventOnly(e.id))
            .thenComparing(e -> e.name.toLowerCase(java.util.Locale.ROOT)));
        return Collections.unmodifiableList(choices);
    }
    /** Dedicated one-shot effects belong to one lifecycle, not both.
     * Standard WLED animation choices remain available in either Auto Event;
     * WLED Strobe #23 is explicitly preserved for Startup and Shutdown.
     * Do not reclassify the 15 general proprietary Show Mode animations. */
    private static boolean startupOnly(int id) {
        return id == 32769 || id == 32770 || id == 32771;
    }
    private static boolean shutdownOnly(int id) {
        return id == 32773 || id == 32787;
    }
    static boolean isAllowedForAutoEvent(String event, int id) {
        if (!"Startup".equals(event) && !"Shutdown".equals(event)) return false;
        if (showModesOnly(id)) return false;
        boolean found = false;
        for (Effect candidate : ALL) if (candidate.id == id) { found = true; break; }
        if (!found) return false;
        return "Startup".equals(event) ? !shutdownOnly(id) : !startupOnly(id);
    }
    static List<Effect> autoEventChoices(String event) {
        if (!"Startup".equals(event) && !"Shutdown".equals(event))
            return Collections.emptyList();
        ArrayList<Effect> choices = new ArrayList<>();
        for (Effect effect : autoEventChoices())
            if (isAllowedForAutoEvent(event,effect.id)) choices.add(effect);
        return Collections.unmodifiableList(choices);
    }
    /** Fail closed on unknown names; byName intentionally falls back to Solid
     * for imported legacy previews and is not safe as an eligibility check. */
    static boolean isAllowedForAutoEvent(String event, String effectName) {
        if (effectName == null) return false;
        for (Effect effect : ALL)
            if (effect.name.equalsIgnoreCase(effectName))
                return isAllowedForAutoEvent(event,effect.id);
        return false;
    }
    static Effect byId(int id) {
        for (Effect effect : ALL) if (effect.id == id) return effect;
        return ALL[0];
    }
    static Effect byName(String name) {
        for (Effect effect : ALL) if (effect.name.equalsIgnoreCase(name)) return effect;
        return ALL[0];
    }
    static Effect solid(){return byId(0);}
    private NativeEffectCatalog(){}
}
