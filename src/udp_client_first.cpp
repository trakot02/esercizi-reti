#include "udp/exports.hpp"

#include <stdio.h>

static const str8 CLIENT_MSG = pax_str8("Ciao, sono il client!");

static const str8 CLIENT_ARG_ADDR = pax_str8("--addr=");
static const str8 CLIENT_ARG_PORT = pax_str8("--port=");

int
main(int argc, const char* argv[])
{
    Arena arena = system_reserve(16);

    if (system_network_start() == 0) return 1;

    Address server_addr = {};
    u16     server_port = 8000;

    address_from_str8(pax_str8("localhost"), ADDRESS_KIND_IP4, &server_addr);

    if (argc != 1) {
        Format_Options opts = format_options(10, FORMAT_FLAG_NONE);

        for (uptr i = 1; i < argc; i += 1) {
            str8 arg = pax_str8_max(argv[i], 128);

            if (str8_starts_with(arg, CLIENT_ARG_ADDR) != 0) {
                arg = str8_trim_prefix(arg, CLIENT_ARG_ADDR);
                arg = str8_trim_spaces(arg);

                address_from_str8(arg, ADDRESS_KIND_IP4, &server_addr);
            }

            if (str8_starts_with(arg, CLIENT_ARG_PORT) != 0) {
                arg = str8_trim_prefix(arg, CLIENT_ARG_PORT);
                arg = str8_trim_spaces(arg);

                u16_from_str8(arg, opts, &server_port);
            }
        }
    }

    Socket_UDP client = client_udp_start(&arena, ADDRESS_KIND_IP4);

    if (client == 0) return 1;

    buf8 request  = buf8_reserve(&arena, MEMORY_KIB);
    buf8 response = buf8_reserve(&arena, MEMORY_KIB);

    buf8_write_str8_tail(&request, CLIENT_MSG);

    client_udp_write(client, &request, server_port, server_addr);

    u16     port = 0;
    Address addr = {};

    if (client_udp_read(client, &response, &port, &addr) != 0) {
        if (server_port == port && address_is_equal(server_addr, addr) != 0) {
            buf8_normalize(&response);

            printf(INFO " " BLU("'%.*s'") "\n",
                pax_cast(int, response.size), response.memory);
        } else
            printf(ERROR " Indirizzo o porta inaspettati...\n");
    }

    client_udp_stop(client);

    system_network_stop();
}
