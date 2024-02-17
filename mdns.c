#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include "game.h"

struct wled_server {
    char address[AVAHI_ADDRESS_STR_MAX];
    bool is_wled;
};

static pthread_t mdns_thread;
static AvahiSimplePoll *simple_poll = NULL;
static AvahiClient *client = NULL;
static AvahiServiceBrowser *sb = NULL;
static bool all_for_now = false;
static size_t n_resolvers = 0;
static struct wled_server *wled_servers = NULL;
static size_t num_wled_servers = 0;

static void resolve_callback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *address, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void* userdata) {
    char a[AVAHI_ADDRESS_STR_MAX];
    char *t;
    struct wled_server *new_wled_servers;

    (void)interface;
    (void)protocol;
    (void)userdata;

    assert(r);

    switch (event) {
        case AVAHI_RESOLVER_FAILURE:
            fprintf(stderr, "mdns: (Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
            break;

        case AVAHI_RESOLVER_FOUND: {
            fprintf(stderr, "mdns: Service '%s' of type '%s' in domain '%s':\n", name, type, domain);
            avahi_address_snprint(a, sizeof(a), address);
            t = avahi_string_list_to_string(txt);
            if (t) {
                fprintf(stderr,
                        "mdns: %s:%u (%s): TXT=%s, cookie is %u, is_local: %i, our_own: %i, wide_area: %i, multicast: %i, cached: %i\n",
                        host_name, port, a,
                        t,
                        avahi_string_list_get_service_cookie(txt),
                        !!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
                        !!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN),
                        !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
                        !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
                        !!(flags & AVAHI_LOOKUP_RESULT_CACHED));
                avahi_free(t);
            }
            if (wled_servers) {
                new_wled_servers = realloc(wled_servers, sizeof(struct wled_server) * (num_wled_servers + 1));
            } else {
                new_wled_servers = malloc(sizeof(struct wled_server));
            }
            if (new_wled_servers) {
                memcpy(&new_wled_servers[num_wled_servers].address, a, sizeof(a));
                new_wled_servers[num_wled_servers].is_wled = false;
                wled_servers = new_wled_servers;
                num_wled_servers++;
            }
        }
    }
    avahi_service_resolver_free(r);

    n_resolvers--;
    if (all_for_now && n_resolvers == 0) {
        avahi_simple_poll_quit(simple_poll);
    }
}

static void browse_callback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void *userdata) {
    AvahiClient *c = userdata;

    assert(b);

    switch (event) {
        case AVAHI_BROWSER_FAILURE:
            fprintf(stderr, "mdns: (Browser) %s\n", avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))));
            avahi_simple_poll_quit(simple_poll);
            return;

        case AVAHI_BROWSER_NEW:
            fprintf(stderr, "mdns: (Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
            n_resolvers++;
            if (! avahi_service_resolver_new(c, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, c)) {
                fprintf(stderr, "mdns: Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_client_errno(c)));
			}
            break;

        case AVAHI_BROWSER_REMOVE:
            fprintf(stderr, "mdns: (Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            fprintf(stderr, "mdns: (Browser) %s\n", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            if (event == AVAHI_BROWSER_ALL_FOR_NOW) {
                all_for_now = true;
                if (all_for_now && n_resolvers == 0) {
                    avahi_simple_poll_quit(simple_poll);
                }
            }
            break;
    }
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void *userdata) {
	assert(c);

    if (state == AVAHI_CLIENT_FAILURE) {
        fprintf(stderr, "mdns: Server connection failure: %s\n", avahi_strerror(avahi_client_errno(c)));
        avahi_simple_poll_quit(simple_poll);
    }
}

static void *mdns_thread_func(void *arg)
{
    int error;
    size_t i;

    (void)arg;

    fprintf(stderr, "mdns: Initializing.\n");

    for (;;) {
        fprintf(stderr, "mdns: Browsing.\n");

        all_for_now = false;
        n_resolvers = 0;

        simple_poll = avahi_simple_poll_new();
        if (! simple_poll) {
            fprintf(stderr, "mdns: Failed to create simple poll object.\n");
            return NULL;
        }

        client = avahi_client_new(avahi_simple_poll_get(simple_poll), AVAHI_CLIENT_NO_FAIL, client_callback, NULL, &error);
        if (! client) {
            fprintf(stderr, "mdns: Failed to create client: %s\n", avahi_strerror(error));
            avahi_simple_poll_free(simple_poll);
            return NULL;
        }

        sb = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_wled._tcp", NULL, 0, browse_callback, client);
        if (! sb) {
            fprintf(stderr, "mdns: Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
            avahi_client_free(client);
            avahi_simple_poll_free(simple_poll);
            return NULL;
        }

        avahi_simple_poll_loop(simple_poll);

        avahi_service_browser_free(sb);
        avahi_client_free(client);
        avahi_simple_poll_free(simple_poll);

        for (i = 0; i < num_wled_servers; i++) {
            if (wled_api_check(wled_servers[i].address)) {
                wled_servers[i].is_wled = true;
            }
        }

        for (i = 0; i < num_wled_servers; i++) {
            if (wled_servers[i].is_wled) {
                update_wled_ip(wled_servers[i].address);
                break;
            }
        }

        if (wled_servers) {
            free(wled_servers);
        }
        wled_servers = NULL;
        num_wled_servers = 0;

        sleep(60);
    }

    fprintf(stderr, "mdns: Done.\n");

    return NULL;
}

void mdns_init(void)
{
    if (pthread_create(&mdns_thread, NULL, mdns_thread_func, NULL) != 0) {
        perror("pthread_create");
        return;
    }

    (void)pthread_detach(mdns_thread);
}
