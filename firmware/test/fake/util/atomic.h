#ifndef DOMECONTROL_FAKE_UTIL_ATOMIC_H
#define DOMECONTROL_FAKE_UTIL_ATOMIC_H

/*
 * Host stand-in for <util/atomic.h>. Tests are single-threaded, so the critical
 * section is just the block that follows the macro.
 */

#define ATOMIC_RESTORESTATE 0
#define ATOMIC_FORCEON 0
#define ATOMIC_BLOCK(type)

#endif /* DOMECONTROL_FAKE_UTIL_ATOMIC_H */