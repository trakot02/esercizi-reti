#ifndef HTTP_RESPONSE_CPP
#define HTTP_RESPONSE_CPP

#include "response.hpp"

HTTP_Response_Writer
http_response_writer_init(Arena* arena, uptr length)
{
    HTTP_Response_Writer result = {};

    result.buffer = buf8_reserve(arena, length);

    return result;
}

void
http_response_writer_clear(HTTP_Response_Writer* self)
{
    buf8_clear(&self->buffer);

    self->line = 0;
    self->body = 0;
}

b32
http_response_write(HTTP_Response_Writer* self, Socket_TCP session)
{
    return socket_tcp_write(session, &self->buffer);
}

b32
http_response_write_start(HTTP_Response_Writer* self, str8 version, str8 status, str8 message)
{
    if (self->line != 0) return 0;

    buf8_write_str8_tail(&self->buffer, version);
    buf8_write_str8_tail(&self->buffer, SPACE);
    buf8_write_str8_tail(&self->buffer, status);
    buf8_write_str8_tail(&self->buffer, SPACE);
    buf8_write_str8_tail(&self->buffer, message);
    buf8_write_str8_tail(&self->buffer, CRLF);

    self->line += 1;

    return 1;
}

b32
http_response_write_header(HTTP_Response_Writer* self, str8 key, str8 value)
{
    if (self->body != 0) return 0;

    buf8_write_str8_tail(&self->buffer, key);
    buf8_write_str8_tail(&self->buffer, COLON);
    buf8_write_str8_tail(&self->buffer, SPACE);
    buf8_write_str8_tail(&self->buffer, value);
    buf8_write_str8_tail(&self->buffer, CRLF);

    self->line += 1;

    return 1;
}

uptr
http_response_write_content(HTTP_Response_Writer* self, buf8* content)
{
    if (self->body == 0) {
        buf8_write_str8_tail(&self->buffer, CRLF);

        self->line += 1;
        self->body  = 1;
    }

    return buf8_write_tail(&self->buffer, content);
}

uptr
http_response_write_content_str8(HTTP_Response_Writer* self, str8 content)
{
    if (self->body == 0) {
        buf8_write_str8_tail(&self->buffer, CRLF);

        self->line += 1;
        self->body  = 1;
    }

    return buf8_write_str8_tail(&self->buffer, content);
}

HTTP_Response_Reader
http_response_reader_init(Arena* arena, uptr length)
{
    HTTP_Response_Reader result = {};

    result.buffer = buf8_reserve(arena, length);

    return result;
}

void
http_response_reader_clear(HTTP_Response_Reader* self)
{
    buf8_clear(&self->buffer);

    self->line = 0;
    self->body = 0;
}

b32
http_response_read(HTTP_Response_Reader* self, Socket_TCP session)
{
    return socket_tcp_read(session, &self->buffer);
}

b32
http_response_read_start(HTTP_Response_Reader* self, str8* version, str8* status, str8* message)
{
    buf8_normalize(&self->buffer);

    str8 string = str8_make(self->buffer.memory, self->buffer.size);

    if (self->line != 0 || str8_contains(string, CRLF) == 0)
        return 0;

    str8 line   = str8_split_on(string, CRLF, &string);
    str8 left   = {};
    str8 center = {};
    str8 right  = {};

    buf8_drop_head(&self->buffer, line.length + CRLF.length);

    self->line += 1;

    left   = str8_split_on(line, SPACE, &line);
    center = str8_split_on(line, SPACE, &right);

    if (version != 0) *version = str8_trim_spaces(left);
    if (status  != 0) *status  = str8_trim_spaces(center);
    if (message != 0) *message = str8_trim_spaces(right);

    return 1;
}

b32
http_response_read_header(HTTP_Response_Reader* self, str8* key, str8* value)
{
    buf8_normalize(&self->buffer);

    str8 string = str8_make(self->buffer.memory, self->buffer.size);

    if (self->body != 0 || str8_contains(string, CRLF) == 0)
        return 0;

    str8 line  = str8_split_on(string, CRLF, &string);
    str8 left  = {};
    str8 right = {};

    buf8_drop_head(&self->buffer, line.length + CRLF.length);

    self->line += 1;

    switch (line.length) {
        case 0: { self->body = 1; } break;

        default: {
            left = str8_split_on(line, COLON, &right);

            if (key   != 0) *key   = str8_trim_spaces(left);
            if (value != 0) *value = str8_trim_spaces(right);

            return 1;
        } break;
    }

    return 0;
}

HTTP_Heading
http_response_heading(HTTP_Response_Reader* self, Arena* arena, Socket_TCP session)
{
    HTTP_Heading result =
        hash_map_reserve<str8, str8>(arena, 512, &http_hash_str8);

    while (self->body == 0) {
        if (http_response_read(self, session) == 0) break;

        if (self->line == 0) {
            str8 version = {};
            str8 status  = {};
            str8 message = {};

            http_response_read_start(self, &version, &status, &message);

            hash_map_insert(&result, str8_copy(arena, HTTP_VERSION),
                str8_copy(arena, version));

            hash_map_insert(&result, str8_copy(arena, HTTP_STATUS),
                str8_copy(arena, status));

            hash_map_insert(&result, str8_copy(arena, HTTP_MESSAGE),
                str8_copy(arena, message));
        }

        while (self->buffer.size > 0) {
            str8 key   = {};
            str8 value = {};

            if (http_response_read_header(self, &key, &value) == 0)
                break;

            hash_map_insert(&result, str8_copy(arena, key),
                str8_copy(arena, value));
        }
    }

    return result;
}

buf8
http_response_content(HTTP_Response_Reader* self, Arena* arena, uptr length, Socket_TCP session)
{
    buf8 result = buf8_reserve(arena, length);

    if (result.length != 0) {
        length -= buf8_write_tail(&result, &self->buffer);

        while (length > 0) {
            if (http_response_read(self, session) == 0)
                break;

            length -= buf8_write_tail(&result, &self->buffer);
        }
    }

    return result;
}

#endif // HTTP_RESPONSE_CPP
