#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "client.h"
#include "ctable.h"
#include "cJSON.h"

#define PORT_MIN 1024
#define PORT_MAX 65535

/* Client structure: a network node.
 *
 *   Client is an opaque type; it is not used whatsoever in the networking
 * code, and its definition is unavailable to the back-end code.  This module
 * provides the glue between the two.
 *
 */
struct client {
    in_addr_t ip;   // XXX: IPv4 only
    in_port_t port;
};

unsigned int ctable_hash (const struct client *client) {
    return client->ip + client->port;
}

bool ctable_equals (const struct client *a, const struct client *b) {
    return a->ip == b->ip && a->port == b->port;
}

void ctable_act (struct client *client) {
#ifdef P2PSERV_LOG
    printf ("Expiring (%d, %d)\n", client->ip, client->port); fflush (stdout);
#endif
    free (client);
}

/*-----------------------------------------------------------------------------
 * Fills out a client structure with the given ip and port number, ensuring
 * that the arguments are valid */
//-----------------------------------------------------------------------------
static int make_client (struct client *client, const char *ip,
        const char *port) {
    struct in_addr addr;
    char *endptr;
    long lport;

    if (!inet_aton (ip, &addr))
        return CL_BADIP;

    lport = strtol (port, &endptr, 10);
    if (lport < PORT_MIN || lport > PORT_MAX || *endptr != '\0')
        return CL_BADPORT;

    *client = (struct client)
        { .ip = addr.s_addr, .port = (in_port_t) lport };

    return CL_OK;
}

/*-----------------------------------------------------------------------------
 * Adds a client to the network */
//-----------------------------------------------------------------------------
int add_client (const char *ip, const char *port) {
    struct client *client;
    int rc;

    client = malloc (sizeof (struct client));
    if ((rc = make_client (client, ip, port)))
        return rc;

    ctable_insert (client);

    return CL_OK;
}

/*-----------------------------------------------------------------------------
 * Removes a client from the network */
//-----------------------------------------------------------------------------
int remove_client (const char *ip, const char *port) {
    struct client client;
    int rc;

    if ((rc = make_client (&client, ip, port)))
        return rc;

    if (ctable_remove (&client))
        return CL_NOTFOUND;

    return CL_OK;
}

int print_client (const struct client *client, void *data) {
    FILE *stream = data;
    fprintf (stream, "{ %d, %d }\n", client->ip, client->port);
    return 0;
}

static int add_client_to_array (const struct client *client, void *data) {
    cJSON *list = data;
    cJSON *elm  = cJSON_CreateObject ();

    cJSON_AddItemToArray (list, elm);
    cJSON_AddNumberToObject (elm, "ip",   client->ip);
    cJSON_AddNumberToObject (elm, "port", client->port);
    return 0;
}

/*-----------------------------------------------------------------------------
 * Returns a JSON representation of the clients currently connected to the
 * network */
//-----------------------------------------------------------------------------
char *clients_to_json (void) {

    char *out;
    cJSON *client_list = cJSON_CreateArray ();

    ctable_foreach (add_client_to_array, client_list);
    out = cJSON_PrintUnformatted (client_list);
    cJSON_Delete (client_list);

    return out; // XXX: must be freed by caller
}
