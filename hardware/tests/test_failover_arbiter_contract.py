"""Reference-only failover arbiter logic tests; NOT synthesizable hardware or a safety case.

Run: python3 hardware/tests/test_failover_arbiter_contract.py
Models debounced independent P and B health from external hardware;
software tests cannot prove gate propagation delay, watchdog electrical behavior,
actual DI isolation, LED visibility, or automotive functional safety.
"""
from dataclasses import dataclass
from itertools import product

DEAD_STEPS = 2
PRIMARY_RECOVERY_STEPS = 3


@dataclass(frozen=True)
class Health:
    rail: bool
    frame: bool
    profile: bool
    sampler: bool

    @property
    def ready(self):
        return self.rail and self.frame and self.profile and self.sampler


GOOD = Health(True, True, True, True)
BAD = Health(False, False, False, False)


class ReferenceArbiter:
    def __init__(self):
        self.owner = None
        self.pending = None
        self.dead_left = 0
        self.primary_streak = 0

    def step(self, primary: Health, backup: Health):
        self.primary_streak = (
            min(PRIMARY_RECOVERY_STEPS, self.primary_streak + 1)
            if primary.ready else 0
        )
        # Backup must be a complete independent source; never trust a mirrored
        # primary input mask or a merely toggling non-frame timer.
        p_ok = primary.ready and self.primary_streak >= PRIMARY_RECOVERY_STEPS
        b_ok = backup.ready
        desired = "P" if p_ok else "B" if b_ok else None

        if self.owner is not None:
            if self.owner != desired:
                self.owner = None
                self.pending = desired
                self.dead_left = DEAD_STEPS
            # No software owner can assert enable for the other bank.
            return self.owner

        if desired is None:
            self.pending = None
            self.dead_left = 0
            return None

        if self.pending != desired:
            self.pending = desired
            self.dead_left = DEAD_STEPS
            return None

        if self.dead_left:
            self.dead_left -= 1
            return None

        self.owner = desired
        self.pending = None
        return self.owner

    @property
    def grants(self):
        return (self.owner == "P", self.owner == "B")


def test_reset_and_handover():
    a = ReferenceArbiter()
    assert a.grants == (False, False)

    # Qualify P while B is offline. Never drive unqualified outputs.
    for _ in range(6):
        a.step(GOOD, BAD)
    assert a.grants == (True, False)

    # Primary disappears while B remains independently healthy.
    assert a.step(BAD, GOOD) is None
    assert a.grants == (False, False)
    a.step(BAD, GOOD)
    a.step(BAD, GOOD)
    assert a.grants == (False, False)
    assert a.step(BAD, GOOD) == "B"
    assert a.grants == (False, True)

    # P recovery may not immediately cause P to steal LED lines.
    assert a.step(GOOD, GOOD) == "B"
    assert a.step(GOOD, GOOD) == "B"
    assert a.step(GOOD, GOOD) is None
    for _ in range(DEAD_STEPS):
        assert a.step(GOOD, GOOD) is None
    assert a.step(GOOD, GOOD) == "P"


def test_individual_health_faults():
    for field in ("rail", "frame", "profile", "sampler"):
        vals = {key: True for key in ("rail", "frame", "profile", "sampler")}
        vals[field] = False
        broken = Health(**vals)
        a = ReferenceArbiter()
        for _ in range(6):
            a.step(broken, GOOD)
        assert a.grants == (False, True), field
        # A healthy P must never override an invalid backup during a fail.
        a = ReferenceArbiter()
        for _ in range(6):
            a.step(broken, broken)
        assert a.grants == (False, False), field


def test_exhaustive_short_sequences():
    # All 4^7 P/B ready/fault histories; 16,384 sequences.
    # Each event is one *logical simulation step*, NOT milliseconds.
    cases = [(BAD, BAD), (GOOD, BAD), (BAD, GOOD), (GOOD, GOOD)]
    evaluated = 0
    for seq in product(range(4), repeat=7):
        a = ReferenceArbiter()
        previous = None
        for event in seq:
            p, b = cases[event]
            active = a.step(p, b)
            gp, gb = a.grants
            assert not (gp and gb)
            assert active == ("P" if gp else "B" if gb else None)
            assert active is None or (p.ready if active == "P" else b.ready)
            assert not (previous == "P" and active == "B")
            assert not (previous == "B" and active == "P")
            previous = active
        evaluated += 1
    assert evaluated == 16384
    print(f"arbiter reference: PASS ({evaluated} state histories, logical only)")


if __name__ == "__main__":
    test_reset_and_handover()
    test_individual_health_faults()
    test_exhaustive_short_sequences()
