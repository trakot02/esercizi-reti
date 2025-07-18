#include "tcp/exports.hpp"
#include "pax/storage/exports.hpp"

#include <stdio.h>

enum Command_Type
{
    COMMAND_EXIT,
    COMMAND_FILE_CONTENT,
    COMMAND_FILE_SIZE,
};

static const str8 SERVER_DATA_PATH = pax_str8("./data/http_server");

static const str8 SERVER_ARG_PORT = pax_str8("--port=");

void
file_server_on_content(Socket_TCP session, Arena* arena, str8 name, buf8 response);

void
file_server_on_size(Socket_TCP session, Arena* arena, str8 name, buf8 response);

int
main(int argc, const char* argv[])
{
    Arena arena = system_reserve(16);

    if (system_network_start() == 0) return 1;

    u16 server_port = 8000;

    if (argc != 1) {
        Format_Options opts = format_options(10, FORMAT_FLAG_NONE);

        for (uptr i = 1; i < argc; i += 1) {
            str8 arg = pax_str8_max(argv[i], 128);

            if (str8_starts_with(arg, SERVER_ARG_PORT) != 0) {
                arg = str8_trim_prefix(arg, SERVER_ARG_PORT);
                arg = str8_trim_spaces(arg);

                u16_from_str8(arg, opts, &server_port);
            }
        }
    }

    Socket_TCP server = server_tcp_start(&arena,
        server_port, address_any(ADDRESS_KIND_IP4));

    if (server == 0) return 1;

    Socket_TCP session = session_tcp_open(&arena, server);

    if (session == 0) return 1;

    buf8 request  = buf8_reserve(&arena, MEMORY_KIB);
    buf8 response = buf8_reserve(&arena, MEMORY_KIB);

    server_tcp_stop(server);

    uptr offset = arena_offset(&arena);
    b32  loop   = 1;

    do {
        if (session_tcp_read(session, &request) == 0)
            break;

        u8 type = 0;

        buf8_read_mem8_head(&request, &type, 1);

        switch (type) {
            case COMMAND_FILE_CONTENT: {
                str8 name = buf8_read_str8_head(&request,
                    &arena, request.size);

                file_server_on_content(session, &arena, name, response);
            } break;

            case COMMAND_FILE_SIZE: {
                str8 name = buf8_read_str8_head(&request,
                    &arena, request.size);

                file_server_on_size(session, &arena, name, response);
            } break;

            default: { loop = 0; } break;
        }

        arena_rewind(&arena, offset);
    } while (loop != 0);

    session_tcp_close(session);

    system_network_stop();
}

void
file_server_on_content(Socket_TCP session, Arena* arena, str8 name, buf8 response)
{
    printf(INFO " Requested content of file " BLU("'%.*s'") "\n",
        pax_cast(int, name.length), name.memory);

    File_Props props = {};

    file_props(&props, arena, SERVER_DATA_PATH, name);

    uptr size = file_size(&props);
    File file = file_open(arena, SERVER_DATA_PATH, name, FILE_PERM_READ);

    for (uptr i = size; file != 0 && i > 0;) {
        if (file_read(file, &response) == 0)
            break;

        i -= response.size;

        session_tcp_write(session, &response);
    }

    file_close(file);
}

void
file_server_on_size(Socket_TCP session, Arena* arena, str8 name, buf8 response)
{
    printf(INFO " Requested size of file " BLU("'%.*s'") "\n",
        pax_cast(int, name.length), name.memory);

    File_Props     props = {};
    Format_Options opts  = format_options(10, FORMAT_FLAG_NONE);

    file_props(&props, arena, SERVER_DATA_PATH, name);

    u32 size = file_size(&props);

    buf8_write_mem8_tail(&response, pax_cast(u8*, &size),
        pax_size_of(u32));

    session_tcp_write(session, &response);
}
