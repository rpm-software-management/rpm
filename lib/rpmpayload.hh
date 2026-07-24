#ifndef RPM_PAYLOAD_H
#define RPM_PAYLOAD_H

#include <rpm/header.h>
#include <rpm/rpmio.h>

struct rpmPayloadInfo {
    off_t start;
    const char *io;
    int raw;
};

/*
 * Probe the physical payload representation at the current position.
 * On success, fd is positioned at the effective reader start. The io string
 * is either borrowed from h or a static literal.
 */
int rpmPayloadProbe(FD_t fd, Header h, struct rpmPayloadInfo *info);

#endif /* RPM_PAYLOAD_H */
