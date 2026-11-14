package com.opentail.android;

import java.util.Arrays;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;
import java.util.List;
import java.util.Map;
import org.json.JSONObject;

/**
 * Protocol-independent guard for Layout geometry edits.
 * The confirmed image is never replaced by a preview or unverified commit.
 */
final class GeometryTransactionManager {
    enum State { READY, BEGINNING, ACTIVE, UPDATE_PENDING, COMMIT_REQUESTED,
                 COMMITTING, RECONCILING, ERROR }

    static final class Snapshot {
        final int lane,revision;
        final byte[] wire;
        Snapshot(int lane,int revision,byte[] wire) {
            if(lane<0||lane>1||revision<0||revision>65535||wire==null)
                throw new IllegalArgumentException("Invalid authoritative geometry snapshot");
            this.lane=lane;this.revision=revision;this.wire=Arrays.copyOf(wire,wire.length);
        }
    }
    static final class Update {
        final long sequence;
        final byte[] packet;
        Update(long sequence,byte[] packet) {
            this.sequence=sequence;this.packet=Arrays.copyOf(packet,packet.length);
        }
    }

    /** Explicit Panel/Zone translation; no implicit housing or trunk inference. */
    static final class MappedPanel {
        final int id,lane,zoneCount;
        final String name;
        MappedPanel(int id,int lane,String name,int zoneCount) {
            this.id=id;this.lane=lane;this.name=name;this.zoneCount=zoneCount;
        }
    }
    static final class MappedZone {
        final int id,panelId,section,start,count,roleFlags,flags;
        final String name;
        final List<PanelPreviewProtocol.MapRange> ranges;
        MappedZone(int id,int panelId,int section,int start,int count,String name) {
            this(id,panelId,section,start,count,name,PanelPreviewProtocol.ROLE_UNASSIGNED_WIRE);
        }
        MappedZone(int id,int panelId,int section,int start,int count,String name,int roleFlags) {
            this(id,panelId,section,name,roleFlags,0,
                count>0?Collections.singletonList(new PanelPreviewProtocol.MapRange(start,count)):
                    Collections.emptyList());
        }
        MappedZone(int id,int panelId,int section,String name,int roleFlags,int flags,
                   List<PanelPreviewProtocol.MapRange> ranges) {
            this.id=id;this.panelId=panelId;this.section=section;this.name=name;
            this.roleFlags=roleFlags;this.flags=flags;
            this.ranges=Collections.unmodifiableList(new ArrayList<>(ranges));
            this.start=ranges.isEmpty()?0:ranges.get(0).start;
            int total=0;
            for(PanelPreviewProtocol.MapRange range:ranges)total+=range.count;
            this.count=total;
        }
        PanelPreviewProtocol.UpdateDescriptor descriptor() { return descriptor(roleFlags); }
        PanelPreviewProtocol.UpdateDescriptor descriptor(int roles) {
            if(ranges.size()!=1)
                throw new IllegalArgumentException("Multi-range Zone needs one descriptor per range");
            return new PanelPreviewProtocol.UpdateDescriptor(panelId,id,0,start,count,roles);
        }
        PanelPreviewProtocol.UpdateDescriptor descriptorAt(int slot) {
            PanelPreviewProtocol.MapRange range=ranges.get(slot);
            return new PanelPreviewProtocol.UpdateDescriptor(panelId,id,slot,
                range.start,range.count,roleFlags);
        }
    }
    static final class MappedLane {
        final int lane,activePixels;
        final List<MappedPanel> panels;
        final List<MappedZone> zones;
        MappedLane(int lane,int activePixels,List<MappedPanel> panels,List<MappedZone> zones) {
            this.lane=lane;this.activePixels=activePixels;
            Set<Integer> panelAliases=new HashSet<>(),zoneAliases=new HashSet<>(),fullIds=new HashSet<>(),panelIds=new HashSet<>();
            for(MappedPanel panel:panels) {
                int alias=panel.id&0xff;
                if(panel.id<0||panel.id>65535||alias==0||!panelAliases.add(alias)||
                   !fullIds.add(panel.id)||!panelIds.add(panel.id))
                    throw new IllegalArgumentException("Mapped Panel IDs have an invalid or ambiguous low-byte alias");
            }
            for(MappedZone zone:zones) {
                int alias=zone.id&0xff;
                if(zone.id<0||zone.id>65535||alias==0||!zoneAliases.add(alias)||
                   !fullIds.add(zone.id)||!panelIds.contains(zone.panelId))
                    throw new IllegalArgumentException("Mapped Zone IDs have an invalid or ambiguous low-byte alias");
            }
            this.panels=Collections.unmodifiableList(new ArrayList<>(panels));
            this.zones=Collections.unmodifiableList(new ArrayList<>(zones));
        }
        PanelPreviewProtocol.UpdateDescriptor[] descriptors() {
            List<PanelPreviewProtocol.UpdateDescriptor> result=new ArrayList<>();
            for(MappedZone zone:zones)for(int slot=0;slot<zone.ranges.size();slot++)
                result.add(zone.descriptorAt(slot));
            return result.toArray(new PanelPreviewProtocol.UpdateDescriptor[0]);
        }

        /** Deterministic local candidate receipt; CORE's candidate CRC remains authoritative. */
        byte[] candidateWire() {
            ByteArrayOutputStream out=new ByteArrayOutputStream();
            out.write(lane);out.write(activePixels&0xff);out.write((activePixels>>>8)&0xff);
            out.write(panels.size());out.write(zones.size());
            for(MappedPanel panel:panels) {
                byte[] name=panel.name.getBytes(StandardCharsets.UTF_8);
                out.write(panel.id&0xff);out.write((panel.id>>>8)&0xff);
                out.write(Math.min(255,name.length));out.write(name,0,Math.min(255,name.length));
            }
            for(MappedZone zone:zones) {
                byte[] name=zone.name.getBytes(StandardCharsets.UTF_8);
                out.write(zone.id&0xff);out.write((zone.id>>>8)&0xff);
                out.write(zone.panelId&0xff);out.write((zone.panelId>>>8)&0xff);
                out.write(zone.section);out.write(zone.start&0xff);out.write((zone.start>>>8)&0xff);
                out.write(zone.count&0xff);out.write((zone.count>>>8)&0xff);
                out.write(zone.roleFlags);out.write(zone.flags);out.write(zone.ranges.size());
                for(PanelPreviewProtocol.MapRange range:zone.ranges){
                    out.write(range.start&0xff);out.write((range.start>>>8)&0xff);
                    out.write(range.count&0xff);out.write((range.count>>>8)&0xff);
                }
                out.write(Math.min(255,name.length));
                out.write(name,0,Math.min(255,name.length));
            }
            return out.toByteArray();
        }

        /** Split descriptors to the exact negotiated ATT write-command budget. */
        List<PanelPreviewProtocol.UpdateDescriptor[]> descriptorBatches(int mtu) {
            if(mtu<PanelPreviewProtocol.TX_MIN_ATT_MTU)
                throw new IllegalArgumentException("ATT MTU is below the BLE minimum");
            int perPacket=Math.min(4,(mtu-3-11)/8);
            if(perPacket<1)
                throw new IllegalArgumentException("ATT MTU cannot carry one Panel UPDATE descriptor");
            List<PanelPreviewProtocol.UpdateDescriptor[]> batches=new ArrayList<>();
            PanelPreviewProtocol.UpdateDescriptor[] all=descriptors();
            for(int offset=0;offset<all.length;offset+=perPacket) {
                int count=Math.min(perPacket,all.length-offset);
                batches.add(Arrays.copyOfRange(all,offset,offset+count));
            }
            return Collections.unmodifiableList(batches);
        }
        private static PanelPreviewProtocol.MapPanel findUniquePanelByName(
                List<PanelPreviewProtocol.MapPanel> panels,String name) {
            if(name==null||name.trim().isEmpty())return null;
            PanelPreviewProtocol.MapPanel found=null;
            for(PanelPreviewProtocol.MapPanel panel:panels)if(panel.name.equalsIgnoreCase(name.trim())) {
                if(found!=null&&found.id!=panel.id)
                    throw new IllegalArgumentException("Ambiguous Panel name in authoritative map");
                found=panel;
            }
            return found;
        }
        private static PanelPreviewProtocol.MapZone findUniqueZoneByName(
                List<PanelPreviewProtocol.MapZone> zones,String name) {
            if(name==null||name.trim().isEmpty())return null;
            PanelPreviewProtocol.MapZone found=null;
            for(PanelPreviewProtocol.MapZone zone:zones)if(zone.name.equalsIgnoreCase(name.trim())) {
                if(found!=null&&found.id!=zone.id)
                    throw new IllegalArgumentException("Ambiguous Zone name in authoritative Panel map");
                found=zone;
            }
            return found;
        }
        private static MappedZone findUniqueMappedZoneByName(List<MappedZone> zones,String name) {
            if(name==null||name.trim().isEmpty())return null;
            MappedZone found=null;
            for(MappedZone zone:zones)if(zone.name.equalsIgnoreCase(name.trim())) {
                if(found!=null&&found.id!=zone.id)
                    throw new IllegalArgumentException("Ambiguous Zone name in previous Layout map");
                found=zone;
            }
            return found;
        }
        private static MappedPanel findUniqueMappedPanelByName(List<MappedPanel> panels,String name) {
            if(name==null||name.trim().isEmpty())return null;
            MappedPanel found=null;
            for(MappedPanel panel:panels)if(panel.name.equalsIgnoreCase(name.trim())) {
                if(found!=null&&found.id!=panel.id)
                    throw new IllegalArgumentException("Ambiguous Panel name in previous Layout map");
                found=panel;
            }
            return found;
        }
        /** Resolve local low-byte aliases to authoritative full IDs and roles. */
        MappedLane withRolesFromSnapshot(PanelPreviewProtocol.LaneSnapshot snapshot) {
            if(snapshot==null||snapshot.lane!=lane)
                throw new IllegalArgumentException("Authoritative role snapshot lane mismatch");
            PanelPreviewProtocol.MapIdentity current=PanelPreviewProtocol.MapIdentity.parse(snapshot);
            Map<Integer,PanelPreviewProtocol.MapPanel> panelsByAlias=new HashMap<>();
            Map<Integer,PanelPreviewProtocol.MapZone> zonesByAlias=new HashMap<>();
            for(PanelPreviewProtocol.MapPanel panel:current.panels)
                panelsByAlias.put(panel.id&0xff,panel);
            for(PanelPreviewProtocol.MapZone zone:current.zones)
                zonesByAlias.put(zone.id&0xff,zone);

            List<MappedPanel> resolvedPanels=new ArrayList<>();
            Map<Integer,Integer> panelIdsByLocalId=new HashMap<>();
            for(MappedPanel panel:panels) {
                int alias=panel.id&0xff;
                PanelPreviewProtocol.MapPanel byAlias=panelsByAlias.get(alias);
                PanelPreviewProtocol.MapPanel byName=findUniquePanelByName(current.panels,panel.name);
                if(byAlias!=null&&byName!=null&&byAlias.id!=byName.id)
                    throw new IllegalArgumentException("Layout Panel alias and name resolve to different CORE identities");
                PanelPreviewProtocol.MapPanel prior=byAlias!=null?byAlias:byName;
                int fullId=prior==null?(0x8000|alias):prior.id;
                panelIdsByLocalId.put(panel.id,fullId);
                resolvedPanels.add(new MappedPanel(fullId,panel.lane,panel.name,panel.zoneCount));
            }
            List<MappedZone> resolvedZones=new ArrayList<>();
            for(MappedZone zone:zones) {
                int alias=zone.id&0xff;
                PanelPreviewProtocol.MapZone byAlias=zonesByAlias.get(alias);
                PanelPreviewProtocol.MapZone byName=findUniqueZoneByName(current.zones,zone.name);
                if(byAlias!=null&&byName!=null&&byAlias.id!=byName.id)
                    throw new IllegalArgumentException("Layout Zone alias and name resolve to different CORE identities");
                PanelPreviewProtocol.MapZone prior=byAlias!=null?byAlias:byName;
                int fullId=prior==null?(0x4000|alias):prior.id;
                Integer panelId=panelIdsByLocalId.get(zone.panelId);
                if(panelId==null)
                    throw new IllegalArgumentException("Layout Zone references a missing local Panel identity");
                int role=prior==null?zone.roleFlags:wireRole(prior.role);
                resolvedZones.add(new MappedZone(fullId,panelId,zone.section,zone.name,role,
                    zone.flags,zone.ranges));
            }
            return new MappedLane(lane,activePixels,resolvedPanels,resolvedZones);
        }
        /** Preserve full identities and CORE roles across successive live Layout drafts. */
        MappedLane withRolesFrom(MappedLane previous) {
            if(previous==null||previous.lane!=lane)
                throw new IllegalArgumentException("Previous geometry lane mismatch");
            Map<Integer,MappedPanel> panelsByAlias=new HashMap<>();
            Map<Integer,MappedZone> zonesByAlias=new HashMap<>();
            for(MappedPanel panel:previous.panels)panelsByAlias.put(panel.id&0xff,panel);
            for(MappedZone zone:previous.zones)zonesByAlias.put(zone.id&0xff,zone);
            List<MappedPanel> resolvedPanels=new ArrayList<>();
            Map<Integer,Integer> panelIdsByLocalId=new HashMap<>();
            for(MappedPanel panel:panels) {
                int alias=panel.id&0xff;
                MappedPanel byAlias=panelsByAlias.get(alias);
                MappedPanel byName=findUniqueMappedPanelByName(previous.panels,panel.name);
                if(byAlias!=null&&byName!=null&&byAlias.id!=byName.id)
                    throw new IllegalArgumentException("Layout Panel alias and name resolve to different CORE identities");
                MappedPanel prior=byAlias!=null?byAlias:byName;
                int fullId=prior==null?(0x8000|alias):prior.id;
                panelIdsByLocalId.put(panel.id,fullId);
                resolvedPanels.add(new MappedPanel(fullId,panel.lane,panel.name,panel.zoneCount));
            }
            List<MappedZone> resolvedZones=new ArrayList<>();
            for(MappedZone zone:zones) {
                int alias=zone.id&0xff;
                MappedZone byAlias=zonesByAlias.get(alias);
                MappedZone byName=findUniqueMappedZoneByName(previous.zones,zone.name);
                if(byAlias!=null&&byName!=null&&byAlias.id!=byName.id)
                    throw new IllegalArgumentException("Layout Zone alias and name resolve to different CORE identities");
                MappedZone prior=byAlias!=null?byAlias:byName;
                int fullId=prior==null?(0x4000|alias):prior.id;
                Integer panelId=panelIdsByLocalId.get(zone.panelId);
                if(panelId==null)
                    throw new IllegalArgumentException("Layout Zone references a missing local Panel identity");
                int role=prior==null?zone.roleFlags:prior.roleFlags;
                resolvedZones.add(new MappedZone(fullId,panelId,zone.section,zone.name,role,
                    zone.flags,zone.ranges));
            }
            return new MappedLane(lane,activePixels,resolvedPanels,resolvedZones);
        }
        /** Initial preview diff; CORE Zones not represented by the draft are preserved. */
        List<PanelPreviewProtocol.UpdateDescriptor[]> descriptorBatches(
                PanelPreviewProtocol.LaneSnapshot snapshot,int mtu) {
            if(snapshot==null||snapshot.lane!=lane)
                throw new IllegalArgumentException("Authoritative geometry lane mismatch");
            PanelPreviewProtocol.MapIdentity current=PanelPreviewProtocol.MapIdentity.parse(snapshot);
            // An authoritative multi-panel or noncontiguous map is a full
            // physical candidate, not a single-range project approximation.
            if(requiresRangePlanner()||current.panels.size()>1||
                current.zones.stream().anyMatch(z -> z.flags!=0||z.ranges.size()!=1))
                return descriptorBatches(fromV4SaveCandidate(lane,snapshot.wire),mtu);
            Map<Integer,ExistingZone> old=new HashMap<>();
            Map<Integer,PanelPreviewProtocol.MapZone> currentById=new HashMap<>();
            List<PanelPreviewProtocol.UpdateDescriptor> deletes=new ArrayList<>();
            for(PanelPreviewProtocol.MapZone zone:current.zones)currentById.put(zone.id,zone);
            for(MappedZone zone:zones) {
                PanelPreviewProtocol.MapZone prior=currentById.get(zone.id);
                if(prior==null)continue;
                if(prior.ranges.size()>1||(prior.ranges.isEmpty()&&prior.flags!=1))
                    throw new IllegalArgumentException("Existing CORE Zone cannot be represented by this single-range Layout");
                int start=prior.ranges.isEmpty()?-1:prior.ranges.get(0).start;
                int count=prior.ranges.isEmpty()?0:prior.ranges.get(0).count;
                if(prior.panelId!=zone.panelId) {
                    /* Explicitly represented Panel moves are delete-then-upserted
                     * with the same authoritative Zone ID, never a guessed identity. */
                    deletes.add(new PanelPreviewProtocol.UpdateDescriptor(
                        prior.panelId,prior.id,0,0,0,0x20));
                    continue;
                }
                old.put(zone.id,new ExistingZone(prior.panelId,wireRole(prior.role),start,count));
            }
            return buildBatches(old,deletes,mtu);
        }
        /** Live draft diff: only explicitly removed/moved local IDs are deleted. */
        List<PanelPreviewProtocol.UpdateDescriptor[]> descriptorBatches(MappedLane previous,int mtu) {
            if(previous==null||previous.lane!=lane)
                throw new IllegalArgumentException("Previous geometry lane mismatch");
            if(requiresRangePlanner()||previous.requiresRangePlanner()||
               requiresReorderedSlots(previous))
                return rangeAwareBatches(previous,mtu);
            Map<Integer,MappedZone> targetById=new HashMap<>();
            for(MappedZone zone:zones)targetById.put(zone.id,zone);
            Map<Integer,ExistingZone> old=new HashMap<>();
            List<PanelPreviewProtocol.UpdateDescriptor> deletes=new ArrayList<>();
            for(MappedZone prior:previous.zones) {
                MappedZone target=targetById.get(prior.id);
                if(target==null||target.panelId!=prior.panelId)
                    deletes.add(new PanelPreviewProtocol.UpdateDescriptor(prior.panelId,prior.id,0,0,0,0x20));
                else old.put(prior.id,new ExistingZone(prior.panelId,prior.roleFlags,prior.start,prior.count));
            }
            return buildBatches(old,deletes,mtu);
        }

        private boolean requiresReorderedSlots(MappedLane previous) {
            Map<Integer,MappedZone> before=new HashMap<>(),after=new HashMap<>();
            for(MappedZone zone:previous.zones)before.put(zone.id,zone);
            for(MappedZone zone:zones)after.put(zone.id,zone);
            List<Integer> appended=new ArrayList<>(),expected=new ArrayList<>();
            for(MappedZone old:previous.zones) {
                MappedZone target=after.get(old.id);
                if(target!=null&&target.panelId==old.panelId)appended.add(old.id);
            }
            for(MappedZone zone:zones) {
                MappedZone old=before.get(zone.id);
                if(old==null||old.panelId!=zone.panelId)appended.add(zone.id);
                expected.add(zone.id);
            }
            return !appended.equals(expected);
        }
        private boolean requiresRangePlanner() {
            if(panels.size()>1)return true;
            int cursor=0;
            for(MappedZone zone:zones) {
                if(zone.flags!=0||zone.ranges.size()!=1||zone.start!=cursor)return true;
                cursor+=zone.count;
            }
            return cursor!=activePixels;
        }
        /**
         * Firmware Panel Preview v2 is a range-slot protocol: preserve each
         * physical range, its ordering and stable full identity. The old
         * single-range planner must never flatten an imported map. A partial
         * or unrepresentable transition fails before any descriptor is sent.
         */
        private List<PanelPreviewProtocol.UpdateDescriptor[]> rangeAwareBatches(
                MappedLane previous,int mtu) {
            if(mtu<PanelPreviewProtocol.TX_MIN_ATT_MTU)
                throw new IllegalArgumentException("ATT MTU is below the BLE minimum");
            final int perPacket=Math.min(4,(mtu-3-11)/8);
            if(perPacket<1)throw new IllegalArgumentException("ATT MTU cannot carry an UPDATE");
            Map<Integer,MappedZone> oldById=new HashMap<>(),newById=new HashMap<>();
            Map<Integer,MappedPanel> oldPanels=new HashMap<>();
            for(MappedPanel panel:previous.panels)oldPanels.put(panel.id,panel);
            for(MappedPanel panel:panels)
                if(!oldPanels.containsKey(panel.id) &&
                   panel.id!=(0x8000|(panel.id&0xff)))
                    throw new IllegalArgumentException("New Panel full ID cannot be created by v2 alias");
            for(MappedZone old:previous.zones)oldById.put(old.id,old);
            for(MappedZone zone:zones)newById.put(zone.id,zone);
            // UPDATE cannot reorder existing records. Reordering needs a
            // separate, identity-safe plan; do not silently Save a mismatch.
            int priorPanel=-1;
            for(MappedPanel panel:panels)if(oldPanels.containsKey(panel.id)) {
                int pos=previous.panels.indexOf(oldPanels.get(panel.id));
                if(pos<priorPanel)throw new IllegalArgumentException(
                    "Physical Panel reordering is not supported by this live UPDATE");
                priorPanel=pos;
            }
            Set<Integer> recreate=new HashSet<>();
            if(requiresReorderedSlots(previous)) {
                // Preview v2 has no reorder opcode. Delete only the affected
                // suffix and append it in the intended order, keeping the
                // untouched prefix's IDs and animation phases. Migrated IDs
                // cannot be recreated from low-byte aliases, so fail closed.
                int prefix=0;
                while(prefix<previous.zones.size()&&prefix<zones.size()&&
                    previous.zones.get(prefix).id==zones.get(prefix).id&&
                    previous.zones.get(prefix).panelId==zones.get(prefix).panelId)prefix++;
                for(int i=prefix;i<previous.zones.size();i++) {
                    MappedZone item=previous.zones.get(i);
                    MappedZone target=newById.get(item.id);
                    if(target!=null&&item.id!=(0x4000|(item.id&0xff)))
                        throw new IllegalArgumentException("Cannot reorder a migrated full Zone ID over v2 aliases");
                    recreate.add(item.id);
                }
            }
            // A suffix rebuild can also remove empty Panels. Verify the
            // resulting Panel ordering before issuing any destructive UPDATE.
            List<Integer> eventualPanels=new ArrayList<>();
            for(MappedPanel panel:previous.panels) {
                boolean retained=false;
                for(MappedZone old:previous.zones) {
                    MappedZone target=newById.get(old.id);
                    if(old.panelId==panel.id&&!recreate.contains(old.id)&&
                       target!=null&&target.panelId==old.panelId){retained=true;break;}
                }
                if(retained)eventualPanels.add(panel.id);
            }
            for(MappedZone zone:zones)if(!eventualPanels.contains(zone.panelId)) {
                // A fully emptied migrated Panel would be re-created by
                // Firmware using only its low-byte prefix, changing its
                // authoritative 16-bit identity. Reject before any delete.
                if(oldPanels.containsKey(zone.panelId)&&
                   zone.panelId!=(0x8000|(zone.panelId&0xff)))
                    throw new IllegalArgumentException("Cannot rebuild a migrated full Panel ID over v2 aliases");
                eventualPanels.add(zone.panelId);
            }
            List<Integer> wantedPanels=new ArrayList<>();
            for(MappedPanel panel:panels)wantedPanels.add(panel.id);
            if(!eventualPanels.equals(wantedPanels))
                throw new IllegalArgumentException("Physical panel order cannot be preserved by v2 UPDATE");
            List<PanelPreviewProtocol.UpdateDescriptor[]> batches=new ArrayList<>();
            Map<Integer,List<PanelPreviewProtocol.MapRange>> staged=new HashMap<>();
            for(MappedZone old:previous.zones)
                staged.put(old.id,new ArrayList<>(old.ranges));
            // Delete removed or moved Zones first; the firmware drops an
            // emptied Panel itself. Never recreate a migrated full ID by guess.
            for(MappedZone old:previous.zones) {
                MappedZone target=newById.get(old.id);
                if(target==null||target.panelId!=old.panelId||recreate.contains(old.id)) {
                    if(target!=null && target.id!=(0x4000|(target.id&0xff)))
                        throw new IllegalArgumentException("Moving a migrated full Zone ID requires an explicit firmware mapping");
                    batches.add(new PanelPreviewProtocol.UpdateDescriptor[]{
                        new PanelPreviewProtocol.UpdateDescriptor(old.panelId,old.id,0,0,0,0x20)});
                    staged.remove(old.id);
                }
            }
            List<PanelPreviewProtocol.UpdateDescriptor> upserts=new ArrayList<>();
            for(MappedZone zone:zones) {
                MappedZone old=oldById.get(zone.id);
                boolean retained=old!=null&&old.panelId==zone.panelId&&
                    !recreate.contains(zone.id);
                if(!retained&&zone.id!=(0x4000|(zone.id&0xff)))
                    throw new IllegalArgumentException("New Zone full ID cannot be recreated from an alias");
                if(zone.ranges.isEmpty()) {
                    if(!retained||!old.ranges.isEmpty()||old.flags!=zone.flags)
                        throw new IllegalArgumentException("Legacy empty Zone cannot be created by live UPDATE");
                    continue;
                }
                if(retained&&old.ranges.size()>zone.ranges.size()) {
                    if(zone.ranges.isEmpty())throw new IllegalArgumentException("Cannot produce an empty live Zone");
                    // Highest slot first: lower slots never shift identity.
                    for(int slot=old.ranges.size()-1;slot>=zone.ranges.size();slot--) {
                        batches.add(new PanelPreviewProtocol.UpdateDescriptor[]{
                            new PanelPreviewProtocol.UpdateDescriptor(zone.panelId,zone.id,slot,0,0,0x10)});
                        staged.get(zone.id).remove(slot);
                    }
                }
                for(int slot=0;slot<zone.ranges.size();slot++) {
                    PanelPreviewProtocol.MapRange wanted=zone.ranges.get(slot);
                    boolean changed=!retained||slot>=old.ranges.size();
                    if(!changed) {
                        PanelPreviewProtocol.MapRange was=old.ranges.get(slot);
                        changed=was.start!=wanted.start||was.count!=wanted.count||
                            (slot==0&&old.roleFlags!=zone.roleFlags);
                    }
                    if(changed)upserts.add(zone.descriptorAt(slot));
                }
            }
            // A candidate is sent only if every ACK-bound intermediate image
            // is nonoverlapping. Atomic swaps fit one packet; larger unsafe
            // cycles are rejected rather than corrupting a physical output.
            while(!upserts.isEmpty()) {
                int safe=-1;
                for(int i=0;i<upserts.size();i++) {
                    Map<Integer,List<PanelPreviewProtocol.MapRange>> trial=copyRanges(staged);
                    if(stageRange(trial,upserts.get(i))&&validRanges(trial,activePixels)) {
                        safe=i;break;
                    }
                }
                if(safe>=0) {
                    PanelPreviewProtocol.UpdateDescriptor one=upserts.remove(safe);
                    if(!stageRange(staged,one))throw new IllegalStateException("Invalid range slot order");
                    batches.add(new PanelPreviewProtocol.UpdateDescriptor[]{one});
                    continue;
                }
                if(upserts.size()>perPacket)
                    throw new IllegalArgumentException("Unsafe noncontiguous range transition exceeds atomic ATT batch");
                Map<Integer,List<PanelPreviewProtocol.MapRange>> trial=copyRanges(staged);
                for(PanelPreviewProtocol.UpdateDescriptor d:upserts)
                    if(!stageRange(trial,d))
                        throw new IllegalArgumentException("Range slot must be appended in order");
                if(!validRanges(trial,activePixels))
                    throw new IllegalArgumentException("Physical range transition overlaps another range");
                batches.add(upserts.toArray(new PanelPreviewProtocol.UpdateDescriptor[0]));
                staged=trial;
                upserts.clear();
            }
            for(MappedZone target:zones) {
                List<PanelPreviewProtocol.MapRange> applied=staged.get(target.id);
                if(applied==null||applied.size()!=target.ranges.size())
                    throw new IllegalArgumentException("Live range slot count differs from Save candidate");
                for(int slot=0;slot<applied.size();slot++)
                    if(applied.get(slot).start!=target.ranges.get(slot).start||
                       applied.get(slot).count!=target.ranges.get(slot).count)
                        throw new IllegalArgumentException("Live range order differs from Save candidate");
            }
            return Collections.unmodifiableList(batches);
        }
        private static Map<Integer,List<PanelPreviewProtocol.MapRange>> copyRanges(
                Map<Integer,List<PanelPreviewProtocol.MapRange>> source) {
            Map<Integer,List<PanelPreviewProtocol.MapRange>> result=new HashMap<>();
            for(Map.Entry<Integer,List<PanelPreviewProtocol.MapRange>> e:source.entrySet())
                result.put(e.getKey(),new ArrayList<>(e.getValue()));
            return result;
        }
        private static boolean stageRange(
                Map<Integer,List<PanelPreviewProtocol.MapRange>> state,
                PanelPreviewProtocol.UpdateDescriptor d) {
            List<PanelPreviewProtocol.MapRange> ranges=state.get(d.zoneId);
            if(ranges==null){ranges=new ArrayList<>();state.put(d.zoneId,ranges);}
            if(d.rangeSlot>ranges.size()||d.count==0)return false;
            PanelPreviewProtocol.MapRange value=new PanelPreviewProtocol.MapRange(d.start,d.count);
            if(d.rangeSlot==ranges.size())ranges.add(value);
            else ranges.set(d.rangeSlot,value);
            return true;
        }
        private static boolean validRanges(
                Map<Integer,List<PanelPreviewProtocol.MapRange>> state,int capacity) {
            List<PanelPreviewProtocol.MapRange> occupied=new ArrayList<>();
            for(List<PanelPreviewProtocol.MapRange> zone:state.values())
                for(PanelPreviewProtocol.MapRange range:zone) {
                    if(range.count<=0||(long)range.start+range.count>capacity)return false;
                    for(PanelPreviewProtocol.MapRange p:occupied)
                        if(range.start<p.start+p.count&&p.start<range.start+range.count)return false;
                    occupied.add(range);
                }
            return true;
        }

        private static final class ExistingZone {
            final int panelId,roleFlags,start,count;
            ExistingZone(int panelId,int roleFlags,int start,int count) {
                this.panelId=panelId;this.roleFlags=roleFlags;this.start=start;this.count=count;
            }
        }
        private static int wireRole(int role) {
            return role==0xff?PanelPreviewProtocol.ROLE_UNASSIGNED_WIRE:role;
        }
        private List<PanelPreviewProtocol.UpdateDescriptor[]> buildBatches(
                Map<Integer,ExistingZone> old,List<PanelPreviewProtocol.UpdateDescriptor> deletes,int mtu) {
            if(mtu<PanelPreviewProtocol.TX_MIN_ATT_MTU)
                throw new IllegalArgumentException("ATT MTU is below the BLE minimum");
            int perPacket=Math.min(4,(mtu-3-11)/8);
            if(perPacket<1)throw new IllegalArgumentException("ATT MTU cannot carry one Panel UPDATE descriptor");
            List<PanelPreviewProtocol.UpdateDescriptor[]> batches=new ArrayList<>();
            for(int offset=0;offset<deletes.size();offset+=perPacket) {
                int count=Math.min(perPacket,deletes.size()-offset);
                batches.add(deletes.subList(offset,offset+count).toArray(new PanelPreviewProtocol.UpdateDescriptor[count]));
            }
            Map<Integer,ExistingZone> simulated=new HashMap<>(old);
            List<MappedZone> pending=new ArrayList<>();
            for(MappedZone zone:zones) {
                ExistingZone prior=old.get(zone.id);
                // Caller resolved inherited roles before planning; the intended
                // candidate role is authoritative, not the prior live role.
                int role=zone.roleFlags;
                if(prior==null||prior.start!=zone.start||prior.count!=zone.count||prior.roleFlags!=role)
                    pending.add(new MappedZone(zone.id,zone.panelId,zone.section,zone.start,zone.count,zone.name,role));
            }
            while(!pending.isEmpty()) {
                List<PanelPreviewProtocol.UpdateDescriptor> batch=new ArrayList<>();
                while(batch.size()<perPacket) {
                    int safe=-1;
                    for(int i=0;i<pending.size();i++) {
                        MappedZone candidate=pending.get(i);long end=(long)candidate.start+candidate.count;
                        boolean overlap=false;
                        for(Map.Entry<Integer,ExistingZone> entry:simulated.entrySet()) {
                            if(entry.getKey()==candidate.id)continue;
                            ExistingZone occupied=entry.getValue();
                            if(occupied.start<0||occupied.count<1)continue;
                            long occupiedEnd=(long)occupied.start+occupied.count;
                            if(candidate.start<occupiedEnd&&occupied.start<end){overlap=true;break;}
                        }
                        if(!overlap){safe=i;break;}
                    }
                    if(safe<0)break;
                    MappedZone ready=pending.remove(safe);
                    batch.add(ready.descriptor());
                    simulated.put(ready.id,new ExistingZone(ready.panelId,ready.roleFlags,ready.start,ready.count));
                }
                if(batch.isEmpty()) {
                    if(pending.size()>perPacket)
                        throw new IllegalStateException("Geometry transition requires more simultaneous descriptors than the negotiated ATT MTU can carry atomically");
                    for(MappedZone zone:pending) {
                        batch.add(zone.descriptor());
                        simulated.put(zone.id,new ExistingZone(zone.panelId,zone.roleFlags,zone.start,zone.count));
                    }
                    pending.clear();
                }
                batches.add(batch.toArray(new PanelPreviewProtocol.UpdateDescriptor[0]));
            }
            return Collections.unmodifiableList(batches);
        }
    }
    static final class LayoutMapping {
        final MappedLane[] lanes;
        LayoutMapping(MappedLane[] lanes) { this.lanes=lanes; }
        MappedLane lane(int lane) {
            if(lane<0||lane>=lanes.length)throw new IllegalArgumentException("Invalid output lane");
            return lanes[lane];
        }
        LayoutMapping withRolesFromSnapshots(PanelPreviewProtocol.LaneSnapshot[] snapshots) {
            if(snapshots==null||snapshots.length!=2)
                throw new IllegalArgumentException("Both authoritative output snapshots are required");
            return new LayoutMapping(new MappedLane[]{
                lanes[0].withRolesFromSnapshot(snapshots[0]),
                lanes[1].withRolesFromSnapshot(snapshots[1])});
        }
        LayoutMapping withLane(int lane,MappedLane replacement) {
            if(replacement==null||replacement.lane!=lane)
                throw new IllegalArgumentException("Preview lane replacement mismatch");
            MappedLane[] updated=lanes.clone();
            updated[lane]=replacement;
            return new LayoutMapping(updated);
        }
        LayoutMapping withRolesFrom(LayoutMapping previous) {
            if(previous==null)throw new IllegalArgumentException("Previous geometry mapping is required");
            return new LayoutMapping(new MappedLane[]{
                lanes[0].withRolesFrom(previous.lanes[0]),
                lanes[1].withRolesFrom(previous.lanes[1])});
        }
    }

    /**
     * Maps existing local Output/section links to CORE's hierarchical schema.
     * Panel IDs/names must be explicit project metadata; a design zone is never
     * guessed to be a housing or trunk Panel. Existing UI structure is unchanged.
     * Current Layout sections are contiguous single-range zones; multi-range
     * authoring remains disabled until the UI has explicit range metadata.
     */
    /**
     * Convert the EXACT validated 667-byte Save candidate to volatile preview
     * geometry, rather than re-deriving roles/IDs/ranges from NativeProject.
     * The synthesized snapshot is an in-memory parser input only; it never
     * claims controller confirmation or changes its authoritative revision.
     */
    static MappedLane fromV4SaveCandidate(int lane,byte[] wire) {
        if(lane<0||lane>1||wire==null||wire.length!=PanelV4LayoutProtocol.MAP_BYTES)
            throw new IllegalArgumentException("Invalid full Panel v4 Save candidate");
        byte[] header=new byte[14];
        header[0]=PanelPreviewProtocol.WIRE_VERSION;
        header[1]=(byte)0x81;header[2]=(byte)lane;
        header[4]=(byte)wire.length;header[5]=(byte)(wire.length>>>8);
        int crc=PanelPreviewProtocol.crc16(wire);
        header[6]=(byte)crc;header[7]=(byte)(crc>>>8);
        header[13]=1;
        PanelPreviewProtocol.SnapshotHeader parsed=
            PanelPreviewProtocol.SnapshotHeader.parse(header,lane);
        parsed.verify(wire,lane);
        PanelPreviewProtocol.MapIdentity current=PanelPreviewProtocol.MapIdentity.parse(
            new PanelPreviewProtocol.LaneSnapshot(parsed,wire));
        List<MappedPanel> panels=new ArrayList<>();
        List<MappedZone> zones=new ArrayList<>();
        for(PanelPreviewProtocol.MapPanel panel:current.panels)
            panels.add(new MappedPanel(panel.id,panel.lane,panel.name,panel.zoneCount));
        int section=0;
        for(PanelPreviewProtocol.MapZone zone:current.zones)
            zones.add(new MappedZone(zone.id,zone.panelId,section++,
                zone.name,MappedLane.wireRole(zone.role),zone.flags,zone.ranges));
        return new MappedLane(lane,current.activePixels,panels,zones);
    }

    /** Mirror Firmware ot_pz_live_geometry_matches: names, color order and
     * calibration are metadata; full IDs, ordering, role and ranges are not. */
    static boolean sameLiveGeometry(MappedLane accepted,MappedLane upload) {
        if(accepted==null||upload==null||accepted.lane!=upload.lane||
           accepted.activePixels!=upload.activePixels||
           accepted.panels.size()!=upload.panels.size()||
           accepted.zones.size()!=upload.zones.size())return false;
        for(int i=0;i<accepted.panels.size();i++) {
            MappedPanel a=accepted.panels.get(i),b=upload.panels.get(i);
            if(a.id!=b.id||a.lane!=b.lane||a.zoneCount!=b.zoneCount)return false;
        }
        for(int i=0;i<accepted.zones.size();i++) {
            MappedZone a=accepted.zones.get(i),b=upload.zones.get(i);
            if(a.id!=b.id||a.panelId!=b.panelId||a.roleFlags!=b.roleFlags||
               a.flags!=b.flags||a.ranges.size()!=b.ranges.size())return false;
            for(int n=0;n<a.ranges.size();n++){
                PanelPreviewProtocol.MapRange ar=a.ranges.get(n),br=b.ranges.get(n);
                if(ar.start!=br.start||ar.count!=br.count)return false;
            }
        }
        return true;
    }

    static LayoutMapping mapLayout(NativeProject project) {
        if(project==null)throw new IllegalArgumentException("Layout project is required");
        @SuppressWarnings("unchecked")
        List<JSONObject>[] assigned=new List[]{new ArrayList<>(),new ArrayList<>()};
        boolean allExplicitPanel=true,partialPanelMetadata=false;
        for(int i=0;i<project.zoneCount();i++) {
            JSONObject zone=project.zone(i);
            int lane=zone.optInt("physicalOutput",-1);
            if(lane<0)continue;
            if(lane>1)throw new IllegalArgumentException("Physical output is outside the two-lane contract");
            boolean hasId=zone.has("physicalPanelId"),hasName=zone.has("physicalPanelName");
            boolean explicit=hasId&&hasName;
            partialPanelMetadata|=hasId!=hasName;
            allExplicitPanel&=explicit;
            assigned[lane].add(zone);
        }
        if(partialPanelMetadata)
            throw new IllegalStateException("Panel identity requires both ID and name");
        // The only implicit layout is a truly empty universal controller.
        // All configured sections require stable, explicit Panel identities.
        if(assigned[0].isEmpty()&&assigned[1].isEmpty())
            return new LayoutMapping(new MappedLane[]{
                new MappedLane(0,0,new ArrayList<>(),new ArrayList<>()),
                new MappedLane(1,0,new ArrayList<>(),new ArrayList<>())});
        if(!allExplicitPanel)
            throw new IllegalStateException("Every assigned section must have an explicit CORE Panel identity");
        MappedLane[] lanes=new MappedLane[2];
        Map<Integer,Integer> globalPanelLanes=new HashMap<>();
        for(int lane=0;lane<2;lane++) {
            List<JSONObject> sections=assigned[lane];
            Collections.sort(sections,(a,b)->Integer.compare(
                a.optInt("physicalSection",-1),b.optInt("physicalSection",-1)));
            if(sections.size()>8)
                throw new IllegalArgumentException("Output "+(lane+1)+" exceeds CORE's eight-zone limit");
            Map<Integer,String> panelNames=new HashMap<>();
            Map<Integer,Integer> panelCounts=new HashMap<>();
            List<MappedZone> zones=new ArrayList<>();
            int cursor=0;
            for(JSONObject source:sections) {
                int section=source.optInt("physicalSection",-1);
                int panelId=source.optInt("physicalPanelId",-1);
                String panelName=source.optString("physicalPanelName","").trim();
                int count=source.optInt("length",-1);
                if(section<0||section>7||panelId<1||panelId>127||
                   panelName.isEmpty()||count<1)
                    throw new IllegalArgumentException("Invalid physical section or Panel identity");
                Integer priorLane=globalPanelLanes.get(panelId);
                if(priorLane!=null&&priorLane.intValue()!=lane)
                    throw new IllegalArgumentException("CORE Panel IDs are controller-wide and cannot cross outputs");
                if(priorLane==null)globalPanelLanes.put(panelId,lane);
                String priorName=panelNames.get(panelId);
                if(priorName!=null&&!priorName.equals(panelName))
                    throw new IllegalArgumentException("One Panel ID has conflicting names");
                if(priorName==null)panelNames.put(panelId,panelName);
                panelCounts.put(panelId,panelCounts.containsKey(panelId)?
                    panelCounts.get(panelId)+1:1);
                long end=(long)cursor+count;
                if(end>2000)
                    throw new IllegalArgumentException("Output "+(lane+1)+" exceeds the 2,000-pixel Android limit");
                int zoneId=128+lane*8+section+1;
                zones.add(new MappedZone(zoneId,panelId,section,cursor,count,source.optString("name")));
                cursor=(int)end;
            }
            if(panelNames.size()>4)
                throw new IllegalArgumentException("Output "+(lane+1)+" exceeds CORE's four-Panel limit");
            List<MappedPanel> panels=new ArrayList<>();
            for(Map.Entry<Integer,String> entry:panelNames.entrySet())
                panels.add(new MappedPanel(entry.getKey(),lane,entry.getValue(),
                    panelCounts.get(entry.getKey())));
            Collections.sort(panels,(a,b)->Integer.compare(a.id,b.id));
            lanes[lane]=new MappedLane(lane,cursor,panels,zones);
        }
        return new LayoutMapping(lanes);
    }


    /**
     * Validate the proposed identity/ranges against both authoritative CORE maps
     * before BEGIN. Existing geometry is never implicitly deleted or overwritten
     * by a guessed ID; only an exact existing Zone identity may be updated in place.
     */
    static void validateMappingAgainstSnapshots(LayoutMapping mapping,
            PanelPreviewProtocol.LaneSnapshot[] snapshots,
            PanelPreviewProtocol.Status[] statuses) {
        if(mapping==null||snapshots==null||snapshots.length!=2||
           statuses==null||statuses.length!=2)
            throw new IllegalArgumentException("Both authoritative output snapshots and statuses are required");
        PanelPreviewProtocol.MapIdentity[] current=new PanelPreviewProtocol.MapIdentity[2];
        Map<Integer,PanelPreviewProtocol.MapPanel> currentPanels=new HashMap<>();
        Map<Integer,Integer> panelLanes=new HashMap<>();
        Map<Integer,PanelPreviewProtocol.MapZone> currentZones=new HashMap<>();
        Map<Integer,Integer> zoneLanes=new HashMap<>();
        for(int lane=0;lane<2;lane++) {
            PanelPreviewProtocol.LaneSnapshot snapshot=snapshots[lane];
            PanelPreviewProtocol.Status status=statuses[lane];
            boolean idle=snapshot!=null&&status!=null&&
                status.state==PanelPreviewProtocol.STATE_IDLE&&status.sessionId==0;
            boolean otherLanePreview=snapshot!=null&&status!=null&&
                status.state==PanelPreviewProtocol.STATE_ACTIVE&&status.sessionId!=0&&
                status.baseRevision==snapshot.activeRevision&&status.leaseRemainingMs>0;
            if(snapshot==null||status==null||snapshot.lane!=lane||status.lane!=lane||
               snapshot.pending||!snapshot.persistent||(!idle&&!otherLanePreview)||
               status.configPending||!status.persistent||status.hardSyncFault||status.warningObserved||
               snapshot.activeRevision!=status.activeRevision||
               snapshot.stagedRevision!=status.stagedRevision||
               status.activeRevision!=status.renderRevision)
                throw new IllegalArgumentException("CORE output "+(lane+1)+
                    " is not in a stable, persistent geometry state");
            current[lane]=PanelPreviewProtocol.MapIdentity.parse(snapshot);
            if(current[lane].lane!=lane)
                throw new IllegalArgumentException("Authoritative map lane identity mismatch");
            if(status.pixelCapacity!=current[lane].activePixels)
                throw new IllegalArgumentException("CORE status pixel capacity disagrees with authoritative Panel map");
            for(PanelPreviewProtocol.MapPanel panel:current[lane].panels) {
                if(currentZones.containsKey(panel.id))
                    throw new IllegalArgumentException("CORE Panel/Zone ID namespace collision");
                if(currentPanels.put(panel.id,panel)!=null)
                    throw new IllegalArgumentException("Duplicate controller-wide Panel ID");
                panelLanes.put(panel.id,lane);
            }
            for(PanelPreviewProtocol.MapZone zone:current[lane].zones) {
                if(currentPanels.containsKey(zone.id))
                    throw new IllegalArgumentException("CORE Panel/Zone ID namespace collision");
                if(currentZones.put(zone.id,zone)!=null)
                    throw new IllegalArgumentException("Duplicate controller-wide Zone ID");
                zoneLanes.put(zone.id,lane);
            }
        }
        for(int lane=0;lane<2;lane++) {
            MappedLane proposed=mapping.lane(lane);
            PanelPreviewProtocol.MapIdentity active=current[lane];
            if(proposed.activePixels<0||proposed.activePixels>2000)
                throw new IllegalArgumentException("Output "+(lane+1)+
                    " layout exceeds the supported 2000-LED limit");
            for(MappedPanel panel:proposed.panels) {
                if(currentZones.containsKey(panel.id))
                    throw new IllegalArgumentException("Proposed Panel ID collides with an existing Zone ID");
                PanelPreviewProtocol.MapPanel prior=currentPanels.get(panel.id);
                if(prior!=null&&panelLanes.get(panel.id)!=lane)
                    throw new IllegalArgumentException("Proposed Panel ID conflicts with authoritative Panel identity");
            }
            Set<Integer> proposedZoneIds=new HashSet<>();
            for(MappedZone zone:proposed.zones)proposedZoneIds.add(zone.id);
            for(MappedZone zone:proposed.zones) {
                if(currentPanels.containsKey(zone.id))
                    throw new IllegalArgumentException("Proposed Zone ID collides with an existing Panel ID");
                PanelPreviewProtocol.MapZone prior=currentZones.get(zone.id);
                if(prior!=null&&zoneLanes.get(zone.id)!=lane)
                    throw new IllegalArgumentException("Existing Zone ID belongs to another output");
                if(zone.ranges.isEmpty()) {
                    if(prior==null||zone.flags!=1||!prior.ranges.isEmpty())
                        throw new IllegalArgumentException("Cannot create a new empty physical Zone over UPDATE");
                    continue; // preserved legacy placeholder, never fabricated
                }
                if(zone.flags!=0)
                    throw new IllegalArgumentException("Physical range flags are not supported by v2 UPDATE");
                for(PanelPreviewProtocol.MapRange proposedRange:zone.ranges) {
                    long end=(long)proposedRange.start+proposedRange.count;
                    if(proposedRange.start<0||proposedRange.count<1||end>proposed.activePixels)
                        throw new IllegalArgumentException("Proposed physical range is outside active pixel capacity");
                    for(PanelPreviewProtocol.MapZone existing:active.zones) {
                        // Unrepresented saved Zones are still physical obstacles;
                        // metadata-only edits must not delete or move them.
                        if(existing.id==zone.id||proposedZoneIds.contains(existing.id))continue;
                        for(PanelPreviewProtocol.MapRange original:existing.ranges)
                            if(proposedRange.start<original.start+original.count &&
                               original.start<end)
                                throw new IllegalArgumentException("Proposed range overlaps an unrelated saved Zone");
                    }
                }
            }
            // Validate ALL physical slots, not a synthetic contiguous length.
            // Gaps and ordered discontiguous ranges are legal; overlaps are not.
            List<PanelPreviewProtocol.MapRange> occupied=new ArrayList<>();
            for(MappedZone zone:proposed.zones)
                for(PanelPreviewProtocol.MapRange range:zone.ranges) {
                    for(PanelPreviewProtocol.MapRange prior:occupied)
                        if(range.start<prior.start+prior.count &&
                           prior.start<range.start+range.count)
                            throw new IllegalArgumentException("Proposed physical pixel ranges overlap");
                    occupied.add(range);
                }
        }
    }

    private Snapshot lastConfirmed;
    private State state=State.READY;
    private long sessionId,nextSequence=1,pendingSequence;
    private byte[] candidate,pendingPacket;
    private long candidateCrc32;
    private int targetRevision=-1;
    private boolean syncUncertain;
    private String lastError;

    GeometryTransactionManager(Snapshot initial) {
        if(initial==null)throw new IllegalArgumentException("Initial authoritative geometry is required");
        lastConfirmed=copy(initial);
    }

    synchronized long sessionId() { return sessionId; }
    synchronized State state() { return state; }
    synchronized Snapshot lastConfirmed() { return copy(lastConfirmed); }
    synchronized byte[] candidateCopy() { return candidate==null?null:Arrays.copyOf(candidate,candidate.length); }
    synchronized boolean syncUncertain() { return syncUncertain; }
    synchronized String lastError() { return lastError; }

    synchronized void markSynchronizationUncertain(String reason) {
        syncUncertain=true;
        lastError=reason==null?"Geometry synchronization is uncertain":reason;
        if(state!=State.COMMITTING && state!=State.COMMIT_REQUESTED)
            state=State.RECONCILING;
    }

    synchronized boolean requestBegin() {
        if(state!=State.READY||syncUncertain)return false;
        state=State.BEGINNING;lastError=null;
        candidate=Arrays.copyOf(lastConfirmed.wire,lastConfirmed.wire.length);
        nextSequence=1;pendingSequence=0;pendingPacket=null;sessionId=0;
        return true;
    }

    synchronized boolean acceptBegin(PanelPreviewProtocol.Status status) {
        if(state!=State.BEGINNING||status==null||status.state!=PanelPreviewProtocol.STATE_ACTIVE||
           status.lane!=lastConfirmed.lane||status.sessionId==0||
           status.baseRevision!=lastConfirmed.revision||status.pixelCapacity==0)return false;
        sessionId=status.sessionId;candidateCrc32=status.candidateCrc32;
        state=State.ACTIVE;return true;
    }

    /** Only the newest not-yet-sent candidate is retained by the caller. */
    synchronized Update queueUpdate(byte[] candidateWire,byte[] encodedPacket) {
        if(state!=State.ACTIVE||syncUncertain||pendingPacket!=null||
           candidateWire==null||encodedPacket==null)return null;
        if(encodedPacket.length<19||encodedPacket[0]!=(byte)0xfe||encodedPacket[1]!=2||
           (encodedPacket.length-11)%8!=0||(encodedPacket.length-11)/8<1||
           (encodedPacket.length-11)/8>4||encodedPacket[2]!=(byte)lastConfirmed.lane||
           PanelPreviewProtocol.u32(encodedPacket,3)!=sessionId||
           PanelPreviewProtocol.u32(encodedPacket,7)!=nextSequence)
            throw new IllegalArgumentException("Panel UPDATE packet does not match the active lane/session/sequence");
        if(!PanelPreviewProtocol.CORE_UNASSIGNED_ROLE_UPDATE_SUPPORTED) {
            for(int at=18;at<encodedPacket.length;at+=8)
                if((encodedPacket[at]&0x0f)==PanelPreviewProtocol.ROLE_UNASSIGNED_WIRE)
                    throw new IllegalStateException("CORE does not yet accept the UNASSIGNED Panel UPDATE role sentinel");
        }
        candidate=Arrays.copyOf(candidateWire,candidateWire.length);
        pendingSequence=nextSequence;
        pendingPacket=Arrays.copyOf(encodedPacket,encodedPacket.length);
        state=State.UPDATE_PENDING;
        return new Update(pendingSequence,pendingPacket);
    }

    /** Build the next authenticated, sequenced UPDATE from CORE's exact wire contract. */
    synchronized Update queueUpdate(byte[] candidateWire,
            PanelPreviewProtocol.UpdateDescriptor[] descriptors,int attMtu) {
        if(state!=State.ACTIVE||syncUncertain||pendingPacket!=null||
           candidateWire==null||descriptors==null||descriptors.length==0)return null;
        byte[] packet=PanelPreviewProtocol.update(lastConfirmed.lane,sessionId,
            nextSequence,descriptors,attMtu);
        return queueUpdate(candidateWire,packet);
    }

    synchronized boolean acknowledgeUpdate(PanelPreviewProtocol.Status status) {
        if(status==null)return false;
        if(state==State.COMMITTING||status.state==PanelPreviewProtocol.STATE_COMMITTING)
            return false;
        if(state!=State.UPDATE_PENDING)return false;
        if(status.lane!=lastConfirmed.lane||status.sessionId!=sessionId||
           status.lastAcceptedSequence!=pendingSequence) {
            if(status.lastRejectedSequence==pendingSequence || status.hardSyncFault) {
                requireReconciliation("CORE did not authoritatively accept the pending geometry UPDATE");
            }
            return false;
        }
        candidateCrc32=status.candidateCrc32;
        pendingPacket=null;pendingSequence=0;
        nextSequence=next(nextSequence);
        state=State.ACTIVE;return true;
    }

    synchronized byte[] requestCommit(PanelPreviewProtocol.Status status) {
        if(state!=State.ACTIVE||syncUncertain||pendingPacket!=null||status==null||
           status.state!=PanelPreviewProtocol.STATE_ACTIVE||status.lane!=lastConfirmed.lane||
           status.sessionId!=sessionId||status.hardSyncFault||
           status.lastAcceptedSequence!=(nextSequence==1?0:previous(nextSequence))||
           status.candidateCrc32!=candidateCrc32)return null;
        targetRevision=lastConfirmed.revision+1;
        state=State.COMMIT_REQUESTED;
        return PanelPreviewProtocol.commit(lastConfirmed.lane,sessionId,status.candidateCrc32);
    }

    synchronized void commitWriteCompleted() {
        // ATT write completion is transport delivery only. CORE status must
        // explicitly report COMMITTING before the manager enters that state.
        // A successful write response is never commit confirmation.
    }

    synchronized void observeStatus(PanelPreviewProtocol.Status status) {
        if(status==null)return;
        // COMMITTING and terminal transaction states take precedence over the
        // retained last-rejection diagnostic. CORE intentionally keeps that
        // diagnostic after a delayed UPDATE is rejected during COMMITTING.
        if(status.state==PanelPreviewProtocol.STATE_COMMITTING) {
            if(status.sessionId==sessionId)state=State.COMMITTING;
            return;
        }
        if(status.state==PanelPreviewProtocol.STATE_COMMITTED) {
            if(status.commitFullyApplied())state=State.RECONCILING;
            else requireReconciliation("CORE reported COMMITTED before active/render revisions agreed");
            return;
        }
        if(status.state==PanelPreviewProtocol.STATE_CANCELLED ||
           status.state==PanelPreviewProtocol.STATE_EXPIRED ||
           status.state==PanelPreviewProtocol.STATE_ERROR) {
            requireReconciliation("Panel transaction reached terminal state; reload authoritative geometry");
            return;
        }
        if(status.lastRejectedReason==PanelPreviewProtocol.ERR_COMMITTING_LOCKED ||
           status.committingRejectCount>0) {
            // A delayed UPDATE rejection is not a commit failure. Preserve the
            // confirmed image and continue observing the transaction. A stale
            // rejection must never mask a later authoritative terminal status.
            return;
        }
        if(status.hardSyncFault) {
            requireReconciliation("CORE geometry sequence fault requires cancel and authoritative reload");
        }
    }

    synchronized void connectionLost() {
        if(state!=State.READY)requireReconciliation("BLE disconnected during geometry transaction");
    }

    synchronized boolean reconcile(PanelPreviewProtocol.Status status,Snapshot authoritative) {
        if(status==null||authoritative==null||authoritative.lane!=lastConfirmed.lane||
           status.lane!=lastConfirmed.lane||authoritative.revision!=status.activeRevision||
           (status.hardSyncFault&&status.state!=PanelPreviewProtocol.STATE_CANCELLED)||
           status.state==PanelPreviewProtocol.STATE_COMMITTING||
           status.activeRevision!=status.renderRevision)return false;

        // A recoverable duplicate may resume the same active preview only
        // when CORE confirms its exact accepted sequence and candidate CRC.
        if(status.state==PanelPreviewProtocol.STATE_ACTIVE && sessionId!=0 &&
           status.sessionId==sessionId && candidate!=null &&
           state!=State.COMMIT_REQUESTED && state!=State.COMMITTING) {
            if(pendingPacket!=null) {
                if(status.lastAcceptedSequence!=pendingSequence)return false;
                pendingPacket=null;pendingSequence=0;
                nextSequence=next(status.lastAcceptedSequence);
                candidateCrc32=status.candidateCrc32;
            } else {
                long expected=nextSequence==1?0:previous(nextSequence);
                if(status.lastAcceptedSequence!=expected ||
                   status.candidateCrc32!=candidateCrc32)return false;
            }
            lastConfirmed=copy(authoritative);
            syncUncertain=false;lastError=null;state=State.ACTIVE;
            return true;
        }

        // A new session after cancellation/reload can return the manager to
        // READY, but the old hard-fault session itself cannot be cleared.
        if(status.state==PanelPreviewProtocol.STATE_ACTIVE &&
           state==State.RECONCILING && candidate==null && status.sessionId==sessionId)
            return false;
        if(status.state==PanelPreviewProtocol.STATE_ACTIVE &&
           state==State.RECONCILING && candidate==null && status.sessionId!=sessionId) {
            lastConfirmed=copy(authoritative);
            candidate=null;pendingPacket=null;pendingSequence=0;sessionId=0;
            targetRevision=-1;syncUncertain=false;lastError=null;state=State.READY;
            return true;
        }
        if(status.state==PanelPreviewProtocol.STATE_ACTIVE &&
           (state==State.COMMIT_REQUESTED||state==State.COMMITTING))return false;
        lastConfirmed=copy(authoritative);
        candidate=null;pendingPacket=null;pendingSequence=0;sessionId=0;
        targetRevision=-1;syncUncertain=false;lastError=null;state=State.READY;
        return true;
    }

    synchronized boolean rejectUpdateDuringCommit(int firmwareError) {
        if(state!=State.COMMITTING && state!=State.COMMIT_REQUESTED)return false;
        if(firmwareError==PanelPreviewProtocol.ERR_COMMITTING_LOCKED) {
            state=State.COMMITTING;
            lastError="Delayed geometry UPDATE rejected while COMMITTING";
            return true;
        }
        return false;
    }

    private void requireReconciliation(String reason) {
        syncUncertain=true;state=State.RECONCILING;lastError=reason;
        candidate=null;pendingPacket=null;pendingSequence=0;
    }
    private static Snapshot copy(Snapshot s) { return new Snapshot(s.lane,s.revision,s.wire); }
    private static long next(long n) { return n==0xffffffffL?1L:n+1L; }
    private static long previous(long n) { return n==1L?0xffffffffL:n-1L; }
}
