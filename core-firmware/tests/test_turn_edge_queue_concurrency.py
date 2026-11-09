#!/usr/bin/env python3
"""Deterministic SPSC turn-edge interleaving model plus source-linked invariants.

This is host-side concurrency evidence, not a substitute for powered GPIO/ISR
stress. The model exercises the same 2 x 32-event publication/consume protocol;
source checks bind the assumptions to the actual CORE implementation.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "main/opentail_core.c").read_text(encoding="utf-8")
HEADER = (ROOT / "main/core_logic.h").read_text(encoding="utf-8")
LOGIC = (ROOT / "main/core_logic.c").read_text(encoding="utf-8")

CAPACITY = 32
MASK = CAPACITY - 1
U32 = 0xFFFFFFFF
HOLD_US = 400_000


class EdgeQueue:
    """Deterministic model of the ISR producer / 10ms sampler consumer."""

    def __init__(self):
        self.events = [None] * CAPACITY
        self.head = 0
        self.tail = 0
        self.overflow = False
        self.writer_active = False
        self.baseline = None

    def depth(self):
        return (self.head - self.tail) & U32

    def begin(self):
        if self.writer_active:
            raise AssertionError("the SPSC producer cannot be re-entered")
        self.writer_active = True

    def publish(self, timestamp_us, asserted):
        if not self.writer_active:
            raise AssertionError("publish requires an active producer")
        if self.depth() >= CAPACITY:
            self.overflow = True
            self.writer_active = False
            return False
        self.events[self.head & MASK] = (timestamp_us, bool(asserted))
        self.head = (self.head + 1) & U32
        self.writer_active = False
        return True

    def push(self, timestamp_us, asserted):
        self.begin()
        return self.publish(timestamp_us, asserted)

    def _resync(self, raw, now_us, interleave=None):
        if self.writer_active:
            return False
        head_before = self.head
        if interleave is not None:
            raw, now_us = interleave(self, raw, now_us)
        head_after = self.head
        if self.writer_active or head_after != head_before:
            return False
        self.tail = head_after
        self.baseline = (now_us, bool(raw))
        return not self.overflow

    def update(self, now_us, raw=False, after_snapshot=None, resync_interleave=None):
        if self.overflow:
            self.overflow = False
            if not self._resync(raw, now_us, resync_interleave):
                self.overflow = True
                return "deferred", []
            return "resynced", []

        tail = self.tail
        head = self.head
        if after_snapshot is not None:
            after_snapshot(self)

        consumed = []
        while tail != head:
            event = self.events[tail & MASK]
            if event[0] > now_us:
                break
            consumed.append(event)
            tail = (tail + 1) & U32
        self.tail = tail

        if self.overflow:
            self.overflow = False
            if not self._resync(raw, now_us, resync_interleave):
                self.overflow = True
                return "deferred", consumed
            return "resynced", consumed

        head_after = self.head
        if self.writer_active or head_after != head:
            return "deferred", consumed
        return "valid", consumed


class HoldState:
    """Small model used only to verify that deferred queue work cannot expire."""

    def __init__(self):
        self.asserted = True
        self.raw = True
        self.release_pending = False
        self.deadline = 0
        self.assertion_ticks = 0

    def edge(self, raw, at_us):
        if bool(raw) == self.raw:
            return
        if self.release_pending and at_us >= self.deadline:
            self.asserted = False
            self.release_pending = False
            self.assertion_ticks = 0
        self.raw = bool(raw)
        if raw:
            if self.release_pending:
                self.release_pending = False
            elif not self.asserted:
                self.assertion_ticks = 0
        else:
            self.assertion_ticks = 0
            if self.asserted:
                self.release_pending = True
                self.deadline = at_us + HOLD_US

    def sample(self, now_us):
        if self.release_pending and now_us >= self.deadline:
            self.asserted = False
            self.release_pending = False
            self.assertion_ticks = 0
        if not self.raw:
            self.assertion_ticks = 0
        elif not self.asserted:
            self.assertion_ticks += 1
            if self.assertion_ticks >= 4:
                self.asserted = True
        return self.asserted


def apply_events(state, events, now_us):
    for timestamp_us, asserted in events:
        state.edge(asserted, timestamp_us)
    return state.sample(now_us)


class QueueConcurrencyTests(unittest.TestCase):
    def test_inflight_publication_defers_expiry_until_edge_is_visible(self):
        q = EdgeQueue()
        state = HoldState()
        self.assertTrue(q.push(1_000_000, False))
        status, events = q.update(1_000_000)
        self.assertEqual(status, "valid")
        self.assertTrue(apply_events(state, events, 1_000_000))
        self.assertTrue(state.release_pending)

        # Producer has captured the rising transition but has not published head.
        q.begin()
        status, events = q.update(1_400_000)
        self.assertEqual(status, "deferred")
        self.assertEqual(events, [])
        self.assertTrue(state.asserted)  # sampler did not call expiry

        self.assertTrue(q.publish(1_399_999, True))
        status, events = q.update(1_400_000)
        self.assertEqual(status, "valid")
        self.assertTrue(apply_events(state, events, 1_400_000))
        self.assertFalse(state.release_pending)  # 399.999ms bridges the gap

    def test_head_publication_after_consumer_snapshot_defers_one_tick(self):
        q = EdgeQueue()
        state = HoldState()
        q.push(1_000_000, False)
        status, events = q.update(1_000_000)
        self.assertEqual(status, "valid")
        apply_events(state, events, 1_000_000)

        def publish_after_snapshot(queue):
            queue.begin()
            self.assertTrue(queue.publish(1_399_999, True))

        status, events = q.update(1_400_000, after_snapshot=publish_after_snapshot)
        self.assertEqual(status, "deferred")
        self.assertEqual(events, [])
        self.assertTrue(state.asserted)

        status, events = q.update(1_400_000)
        self.assertEqual(status, "valid")
        self.assertTrue(apply_events(state, events, 1_400_000))

    def test_exact_deadline_is_not_bridged(self):
        q = EdgeQueue()
        state = HoldState()
        q.push(1_000_000, False)
        _, events = q.update(1_000_000)
        apply_events(state, events, 1_000_000)
        q.begin()
        self.assertEqual(q.update(1_400_000)[0], "deferred")
        q.publish(1_400_000, True)
        status, events = q.update(1_400_000)
        self.assertEqual(status, "valid")
        self.assertFalse(apply_events(state, events, 1_400_000))

    def test_fifo_timestamp_order_and_future_event_retention(self):
        q = EdgeQueue()
        for event in ((100, False), (200, True), (300, False)):
            self.assertTrue(q.push(*event))
        status, events = q.update(250)
        self.assertEqual(status, "valid")
        self.assertEqual(events, [(100, False), (200, True)])
        self.assertEqual(q.depth(), 1)
        status, events = q.update(300)
        self.assertEqual(status, "valid")
        self.assertEqual(events, [(300, False)])
        self.assertEqual(q.depth(), 0)

    def test_full_queue_drops_without_overwrite_and_resyncs_from_gpio(self):
        q = EdgeQueue()
        for i in range(CAPACITY):
            self.assertTrue(q.push(1_000 + i, bool(i & 1)))
        original = list(q.events)
        original_head = q.head
        self.assertFalse(q.push(2_000, True))
        self.assertTrue(q.overflow)
        self.assertEqual(q.head, original_head)
        self.assertEqual(q.events, original)  # unread history was not overwritten

        status, _ = q.update(2_001, raw=False)
        self.assertEqual(status, "resynced")
        self.assertEqual(q.tail, q.head)
        self.assertEqual(q.baseline, (2_001, False))
        self.assertFalse(q.overflow)

    def test_overflow_during_drain_is_detected_before_sample_validity(self):
        q = EdgeQueue()
        for i in range(CAPACITY):
            q.push(1_000 + i, bool(i & 1))

        def overflow_after_snapshot(queue):
            self.assertFalse(queue.push(3_000, True))

        status, _ = q.update(4_000, raw=False, after_snapshot=overflow_after_snapshot)
        self.assertEqual(status, "resynced")
        self.assertTrue(q.baseline[1] is False)
        self.assertEqual(q.tail, q.head)
        self.assertFalse(q.overflow)

    def test_resync_contention_defers_boundedly_then_recovers(self):
        q = EdgeQueue()
        q.overflow = True
        original_tail = q.tail
        for _ in range(100):
            q.writer_active = True
            status, _ = q.update(5_000, raw=False)
            self.assertEqual(status, "deferred")
            self.assertTrue(q.overflow)
            self.assertEqual(q.tail, original_tail)
        q.writer_active = False
        status, _ = q.update(5_000, raw=False)
        self.assertEqual(status, "resynced")
        self.assertFalse(q.overflow)

    def test_lane_queues_are_independent_and_wrap_safe(self):
        left, right = EdgeQueue(), EdgeQueue()
        left.head = left.tail = 0xFFFFFFF0
        for i in range(CAPACITY):
            self.assertTrue(left.push(10_000 + i, bool(i & 1)))
        self.assertEqual(left.depth(), CAPACITY)
        self.assertFalse(left.push(20_000, True))
        self.assertTrue(left.overflow)
        self.assertTrue(right.push(30_000, True))
        self.assertEqual(right.update(30_000), ("valid", [(30_000, True)]))
        self.assertEqual(left.depth(), CAPACITY)


class SourceOrderTests(unittest.TestCase):
    def between(self, source, start, end):
        self.assertEqual(source.count(start), 1, f"missing/nonunique {start}")
        return source.split(start, 1)[1].split(end, 1)[0]

    def test_two_independent_32_event_spsc_rings_remain_intact(self):
        self.assertIn("#define OT_TURN_EDGE_QUEUE_CAPACITY 32u", CORE)
        self.assertIn("turn_edge_events[2][OT_TURN_EDGE_QUEUE_CAPACITY]", CORE)
        self.assertIn("OT_TURN_EDGE_QUEUE_MASK (OT_TURN_EDGE_QUEUE_CAPACITY-1u)", CORE)

    def test_isr_publication_order_and_full_queue_fail_closed(self):
        isr = self.between(
            CORE, "static void IRAM_ATTR input_edge_isr(void *arg){",
            "static bool input_init(void)"
        )
        active = isr.index("__atomic_store_n(&turn_edge_writer_active[slot],1u,__ATOMIC_RELEASE)")
        timestamp = isr.index("esp_timer_get_time()")
        payload_time = isr.index("event->timestamp_us=edge_time_us;")
        payload_level = isr.index("event->asserted=asserted;")
        publish = isr.index("__atomic_store_n(&turn_edge_head[slot],head+1u,__ATOMIC_RELEASE)")
        inactive = isr.index("__atomic_store_n(&turn_edge_writer_active[slot],0u,__ATOMIC_RELEASE)")
        self.assertLess(active, timestamp)
        self.assertLess(timestamp, payload_time)
        self.assertLess(payload_time, publish)
        self.assertLess(payload_level, publish)
        self.assertLess(publish, inactive)

        full = isr.split(
            "if((uint32_t)(head-tail)>=OT_TURN_EDGE_QUEUE_CAPACITY){", 1
        )[1].split("}else{", 1)[0]
        self.assertIn("__atomic_store_n(&turn_edge_overflow[slot],1u,__ATOMIC_RELEASE)", full)
        self.assertNotIn("event->", full)
        self.assertNotIn("head+1u", full)

    def test_consumer_replays_before_expiry_and_defers_inflight_publication(self):
        update = self.between(
            CORE, "static bool turn_edge_queue_update(",
            "/* RMT drivers are created"
        )
        head = update.index("__atomic_load_n(&turn_edge_head[slot],__ATOMIC_ACQUIRE)")
        event = update.index("const ot_turn_edge_event_t event=")
        transition = update.index("ot_signal_debouncer_transition(state,event.asserted,event.timestamp_us)")
        tail = update.index("__atomic_store_n(&turn_edge_tail[slot],tail,__ATOMIC_RELEASE)")
        head_after = update.index("const uint32_t head_after=")
        sample = update.rindex("return ot_signal_debouncer_sample(state,now_us,OT_DEBOUNCE_TICKS)")
        self.assertLess(head, event)
        self.assertLess(event, transition)
        self.assertLess(transition, tail)
        self.assertLess(tail, head_after)
        self.assertLess(head_after, sample)
        self.assertIn("head_after!=head", update)
        self.assertIn("__atomic_load_n(&turn_edge_writer_active[slot],__ATOMIC_ACQUIRE)", update)

    def test_overflow_resync_uses_stable_head_gpio_boundary_and_no_spin(self):
        resync = self.between(
            CORE, "static bool turn_edge_queue_resync(",
            "/* Consume all ISR-published turn edges"
        )
        first_active = resync.index("__atomic_load_n(&turn_edge_writer_active[slot],__ATOMIC_ACQUIRE)")
        head_before = resync.index("const uint32_t head_before=")
        timestamp = resync.index("esp_timer_get_time()")
        gpio = resync.index("gpio_get_level(input_pin[slot+2u])")
        head_after = resync.index("const uint32_t head_after=")
        tail = resync.index("__atomic_store_n(&turn_edge_tail[slot],head_after,__ATOMIC_RELEASE)")
        reset = resync.index("ot_signal_debouncer_resync(state,raw)")
        self.assertLess(first_active, head_before)
        self.assertLess(head_before, timestamp)
        self.assertLess(timestamp, gpio)
        self.assertLess(gpio, head_after)
        self.assertLess(head_after, tail)
        self.assertLess(tail, reset)
        self.assertNotIn("while(", resync)
        self.assertNotIn("for(", resync)

    def test_deferred_turn_does_not_stall_other_physical_inputs_or_nvs_worker(self):
        sampler = self.between(
            CORE, "static void input_sampler_task(void *arg){",
            "static void frame_wake_callback"
        )
        for token in (
            "uint8_t published=(uint8_t)(physical&",
            "if(left_valid){",
            "(previous&OT_SIG_LEFT)",
            "if(right_valid){",
            "(previous&OT_SIG_RIGHT)",
            "if(left_valid&&right_valid)",
            "ot_sw_idf_input_sampled();",
        ):
            self.assertIn(token, sampler)
        self.assertNotIn("nvs_", sampler)
        self.assertNotIn("xSemaphoreTake", sampler)

    def test_turn_hold_remains_exactly_400ms_and_nonblocking(self):
        self.assertIn("#define OT_TURN_RELEASE_HOLD_US UINT64_C(400000)", HEADER)
        self.assertIn("state->release_deadline_us=edge_time_us+OT_TURN_RELEASE_HOLD_US;", LOGIC)
        self.assertIn("now_us>=state->release_deadline_us", LOGIC)
        self.assertNotIn("vTaskDelay", LOGIC)
        self.assertNotIn("Arduino.h", HEADER)


if __name__ == "__main__":
    unittest.main(verbosity=2)
