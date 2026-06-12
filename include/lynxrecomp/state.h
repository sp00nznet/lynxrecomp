/* state.h - Lynx save states.
 *
 * Serializes the full runtime machine state — RAM, CPU registers, Suzy + Mikey
 * registers, and the timer/audio internals — to a flat buffer or a file. This
 * is the data half of a save state; because the recompiled "PC" lives in the C
 * call stack (not a variable), a host restores a state by loading it at a frame
 * boundary and re-entering the recompiled game (calling the entry/main-loop
 * again), which resumes from the restored RAM/peripheral state. The blob is
 * also what the netplay layer sends to sync a peer.
 *
 * Versioned and self-describing; loads reject a mismatched magic/version. The
 * format is build-specific (struct layouts), not a portable interchange format.
 */
#ifndef LYNXRECOMP_STATE_H
#define LYNXRECOMP_STATE_H

#include <stdint.h>
#include <stddef.h>

/* Exact size of a serialized state, so callers can size a buffer. */
size_t lynx_state_size(void);

/* Serialize the machine state into `buf` (>= lynx_state_size()). Returns the
 * number of bytes written, or 0 if `cap` is too small. */
size_t lynx_state_save(uint8_t *buf, size_t cap);

/* Restore a state previously produced by lynx_state_save. Returns 0 on success,
 * -1 on a bad magic/version/size. */
int lynx_state_load(const uint8_t *buf, size_t size);

/* Convenience file wrappers. Return 0 on success. */
int lynx_state_save_file(const char *path);
int lynx_state_load_file(const char *path);

#endif /* LYNXRECOMP_STATE_H */
