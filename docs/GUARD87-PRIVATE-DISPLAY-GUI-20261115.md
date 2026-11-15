# Current four-case public GUI qualification

Frozen3d164382 now passes all four public desktop cases: PBR quarter/tiny and
MToon quarter/tiny,52 checks each,208 actual checks. All four outer tools and
actual statuses exit0 with zero source/input drift and zero owned survivors.
The runs take47.02,45.84,80.72 and81.08 seconds. They execute actual FLTK universal
menus, Bake/ordered Save, Undo/Redo and fresh reopen, then compare all four actual
visible geometry/normal/FLOAT probes against independent ordinary material
controls. Original bounds remain correct<=3, geometry>=12 and wrong-normal>=7.
All original50-second steps,900-second phases and process/resource guards remain.

The first current GUI attempt failed before launching the application: its old
private Xvfb relied on host libselinux.so.1, removed during system updates. Xvfb
reported the loader error; empty displayfd then produced DisplayNameError(:).
That zero-check failure is retained unchanged. It was not a rendering assertion
or a new monitor failure.

A new private testdisplay bundle copies the unchanged Xvfb/Xfont/bash/xkbcomp
bytes and extracts libselinux3.11-2.fc44.x86_64 from an already cached RPM.
The RPM's digests/signatures verify; no system package is installed. Full loader
closure is explicit. Displayfd, a real1100x800 X11 connection and a real XTEST key
are verified. The private server's actual executable and41 mapped ELFs are
certified against99 presealed file/DSO pins. Its deliberate private /usr/bin bind
is proven through equal actual filesystem paths, child covering-mount device/
source, mapped inode and unchanged host certificate. Application monitors retain
their same-namespace policy unchanged. The real GUI coordinator also verifies the
actual private server and rejects any unexpected mapped provider.

The first display-health attempt passed functional checks but caught its child
still exiting in an immediate teardown snapshot; its overall1 is preserved.
A new health run uses the existing graphics helper's bounded20x50ms owned-exit
observation, sees that child disappear after50ms and requires zero survivors.
The original public GUI scenario and its strict final zero-owned assertion are
unchanged. All four public runs pass that original assertion.

The V3 monitor promotes only fully certified expected critical ELF observations
and retains its earlier15 CPU controls. One PBR-quarter retirement classifier
was exercised in the real application run; no uncertified witness was added.
The current canonical bitmap runtime, source/input roster and actual pixel
oracles are preserved. Current real-loader synthetic XR runs remain separate;
this GUI milestone does not claim physical-headset, performance, full sanitizer,
new mainruntime admission or full production-goal completion. The original full
build outer143 remains separately preserved under closed-artifact admission.

The [receipt](validation/guard87-private-display-GUI-20261115.json) contains raw/
compressed hashes, actual process certificates and retained failure provenance.
