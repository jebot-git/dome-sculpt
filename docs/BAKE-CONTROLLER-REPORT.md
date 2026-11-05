# Controller bake report regression

The older frozen689 regression test439 fails because its controller fixture
expects “normal attributes prepared” after the implementation already changed
that status to explicit portable-normal authoring and rest quantization. The
actual controller policy, native bake and preparation assertions pass before
that stale message check. The original failure and full-suite result remain.

This test-only correction requires the current portable-authoring label and
rest-space degree units, then compares the displayed decimal value against the
actual persisted NormalPreparationReceipt measurement within0.00051 degrees.
Original controller gestures, independent policy rows, opt-in state, Cancel,
focus loss and exact one-step Undo/Redo remain. Both complete native consumers
pass9 checks against exact matching-owning main Release/sanitizer core/support;
all inputs remain unchanged. No runtime, public header or native format changes.
Read validation/BAKE-CONTROLLER-REPORT.json for commands, hashes and raw logs.
This is not a green full689 or new762 suite result or physical-controller test.
