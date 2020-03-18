#include "gredis_custom.h"
#include "server.h"

void timeflagCommand(client *c) {
    addReply(c,shared.ok);
}


