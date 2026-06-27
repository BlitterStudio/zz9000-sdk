# ZZ9000 68k Offload Exploration

This is a research track for running selected 68k code on the ZZ9000 ARM side.
The first SDK slice is deliberately small: it defines and tests a sandboxed
function-runner contract, backed by a minimal interpreter. It is not a promise
of transparent Workbench or CLI process execution.

The near-term target is opt-in code:

- synthetic 68k functions used as ABI tests
- pure compute routines with explicit input/output memory windows
- library entry points that can be wrapped with known registers, stack, and
  memory lifetime

The first runner lives in `tools/zz9k-m68k-runner.c`. It executes from a caller
provided big-endian memory window with explicit `D0-D7`, `A0-A7`, `PC`, and
`SR` state. The current instruction subset is only large enough to pin the
contract:

- `MOVEQ #imm,Dn`
- `ADDQ.L #n,Dn`
- `SUBQ.L #n,Dn`
- `MOVE.L Dn,(xxx).L`
- `MOVE.L (xxx).L,Dn`
- `RTS`
- illegal-instruction and out-of-bounds reporting
- instruction-limit timeout reporting

This lets tests cover the hard boundary concerns before a real JIT is chosen:
register state, CCR updates, stack return behavior, big-endian memory access,
fault reporting, and bounded execution. A later Emu68-derived, Amiberry-derived,
or custom JIT should be able to replace the execution engine behind the same
contract.

Out of scope for this slice:

- AmigaOS traps, library calls, interrupts, and supervisor/user emulation
- self-modifying-code cache invalidation
- transparent process offload
- custom-chip access or direct host address-space access
