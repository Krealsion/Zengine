// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_MSGPACK_HPP
#define ZENGINE_NEOVIM_MSGPACK_HPP

// THE WIRE NEOVIM SPEAKS, AND NOTHING MORE OF IT THAN THAT CONVERSATION NEEDS.
//
// Neovim's RPC is msgpack. This is not a general msgpack library: it is the value model, the
// writer and the reader one embedder needs, with its BOUNDS written into the reader rather than
// left to the goodwill of the process on the other end of the pipe. A child process is a
// producer this weave does not control -- a plugin can print megabytes, a bug can emit garbage --
// so every byte that arrives is judged before it is believed:
//
//     a message larger than `Limits::max_message_bytes`   TooLarge, and the session is over
//     nesting deeper than `Limits::max_depth`             Malformed
//     more than `Limits::max_elements` items in one       Malformed
//     a type byte msgpack does not define (0xc1)          Malformed
//
// ⚠ THE READER NEVER RE-PARSES A MESSAGE IT HAS ALREADY WALKED. A redraw batch or a whole exported
// document can arrive in dozens of reads; scanning from the start of the message at every read
// would make receiving one large message quadratic in its size. So the reader keeps a resumable
// SCAN (`Decoder::stack_`) that walks only the new bytes, finds where a message ends without
// building anything, and materializes the value once, from a range already known to be complete
// and within bounds. Nothing is allocated for a message that turns out to be too large.
//
// WHY NOT A DEPENDENCY. The format is small, fixed and fully specified; the part an embedder must
// get right is the bounding, and a third-party reader would still need it written around it.
// One header, pinned by the `neovim` suite over every type byte, both directions.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::neovim::msgpack {

enum class Kind : std::uint8_t { Nil, Bool, Int, UInt, Float, Str, Bin, Array, Map, Ext };

/// ONE DECODED VALUE. Plain members rather than a variant: a Value is built once, read many times,
/// and the reader's questions ("is this an int?", "give me field X") want answers, not visitors.
class Value {
public:
    using Array = std::vector<Value>;
    using Entry = std::pair<Value, Value>;
    using Map = std::vector<Entry>;

    Value() = default;

    static Value nil() { return Value{}; }
    static Value boolean(bool b) {
        Value v;
        v.kind_ = Kind::Bool;
        v.b_ = b;
        return v;
    }
    static Value integer(std::int64_t i) {
        Value v;
        v.kind_ = Kind::Int;
        v.i_ = i;
        return v;
    }
    static Value uinteger(std::uint64_t u) {
        Value v;
        v.kind_ = Kind::UInt;
        v.u_ = u;
        return v;
    }
    static Value floating(double d) {
        Value v;
        v.kind_ = Kind::Float;
        v.f_ = d;
        return v;
    }
    static Value str(std::string s) {
        Value v;
        v.kind_ = Kind::Str;
        v.s_ = std::move(s);
        return v;
    }
    static Value bin(std::string s) {
        Value v;
        v.kind_ = Kind::Bin;
        v.s_ = std::move(s);
        return v;
    }
    static Value array(Array a) {
        Value v;
        v.kind_ = Kind::Array;
        v.a_ = std::move(a);
        return v;
    }
    static Value map(Map m) {
        Value v;
        v.kind_ = Kind::Map;
        v.m_ = std::move(m);
        return v;
    }
    static Value ext(std::int8_t type, std::string data) {
        Value v;
        v.kind_ = Kind::Ext;
        v.ext_ = type;
        v.s_ = std::move(data);
        return v;
    }

    Kind kind() const noexcept { return kind_; }
    bool is_nil() const noexcept { return kind_ == Kind::Nil; }
    bool is_int() const noexcept {
        return kind_ == Kind::Int || (kind_ == Kind::UInt && u_ <= static_cast<std::uint64_t>(INT64_MAX));
    }
    bool is_str() const noexcept { return kind_ == Kind::Str; }
    bool is_array() const noexcept { return kind_ == Kind::Array; }
    bool is_map() const noexcept { return kind_ == Kind::Map; }

    /// The truth value of a Bool, and `fallback` for anything else -- an absent flag in a map a
    /// newer Neovim did not send reads as its documented default.
    bool as_bool(bool fallback = false) const noexcept { return kind_ == Kind::Bool ? b_ : fallback; }

    /// The integer, or `fallback` when this is not an integer that fits an int64.
    std::int64_t as_int(std::int64_t fallback = 0) const noexcept {
        if (kind_ == Kind::Int) {
            return i_;
        }
        if (kind_ == Kind::UInt && u_ <= static_cast<std::uint64_t>(INT64_MAX)) {
            return static_cast<std::int64_t>(u_);
        }
        return fallback;
    }

    /// The unsigned integer, or `fallback` when this is not a non-negative integer.
    std::uint64_t as_uint(std::uint64_t fallback = 0) const noexcept {
        if (kind_ == Kind::UInt) {
            return u_;
        }
        if (kind_ == Kind::Int && i_ >= 0) {
            return static_cast<std::uint64_t>(i_);
        }
        return fallback;
    }

    double as_float(double fallback = 0.0) const noexcept {
        if (kind_ == Kind::Float) {
            return f_;
        }
        if (is_int()) {
            return static_cast<double>(as_int());
        }
        return fallback;
    }

    /// The bytes of a Str or a Bin, and the empty string for anything else.
    const std::string& as_str() const noexcept {
        static const std::string empty;
        return (kind_ == Kind::Str || kind_ == Kind::Bin) ? s_ : empty;
    }

    const Array& as_array() const noexcept {
        static const Array empty;
        return kind_ == Kind::Array ? a_ : empty;
    }

    const Map& as_map() const noexcept {
        static const Map empty;
        return kind_ == Kind::Map ? m_ : empty;
    }

    std::int8_t ext_type() const noexcept { return kind_ == Kind::Ext ? ext_ : 0; }
    const std::string& ext_data() const noexcept {
        static const std::string empty;
        return kind_ == Kind::Ext ? s_ : empty;
    }

    /// A map member by string key, or nullptr. Linear: every map Neovim sends an embedder is a
    /// handful of entries.
    const Value* get(std::string_view key) const noexcept {
        if (kind_ != Kind::Map) {
            return nullptr;
        }
        for (const Entry& e : m_) {
            if (e.first.kind_ == Kind::Str && e.first.s_ == key) {
                return &e.second;
            }
        }
        return nullptr;
    }

    /// An array element, or a shared nil for an index that is not there -- so a reader walking an
    /// event's arguments never indexes past what a producer actually sent.
    const Value& at(std::size_t index) const noexcept {
        static const Value none;
        return (kind_ == Kind::Array && index < a_.size()) ? a_[index] : none;
    }
    std::size_t size() const noexcept {
        return kind_ == Kind::Array ? a_.size() : (kind_ == Kind::Map ? m_.size() : 0);
    }

    /// Neovim's Buffer/Window/Tabpage handles arrive as ext values whose payload is itself a
    /// msgpack integer. Returns the handle, or -1 when this is not such a value.
    std::int64_t handle() const noexcept;

private:
    Kind kind_ = Kind::Nil;
    bool b_ = false;
    std::int8_t ext_ = 0;
    std::int64_t i_ = 0;
    std::uint64_t u_ = 0;
    double f_ = 0.0;
    std::string s_;
    Array a_;
    Map m_;
};

// ---- Writing -------------------------------------------------------------------------------

namespace detail {

inline void put_be(std::string& out, std::uint64_t v, int bytes) {
    for (int i = bytes - 1; i >= 0; --i) {
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFFu));
    }
}

inline std::uint64_t get_be(const unsigned char* p, int bytes) {
    std::uint64_t v = 0;
    for (int i = 0; i < bytes; ++i) {
        v = (v << 8) | p[i];
    }
    return v;
}

} // namespace detail

/// THE WRITER: appends msgpack to a string. Headers are separate calls so a large array (a whole
/// document's lines) is written element by element without first becoming a `Value`.
class Writer {
public:
    explicit Writer(std::string& out) : out_(out) {}

    void nil() { out_.push_back(static_cast<char>(0xC0)); }
    void boolean(bool b) { out_.push_back(static_cast<char>(b ? 0xC3 : 0xC2)); }

    void integer(std::int64_t v) {
        if (v >= 0) {
            uinteger(static_cast<std::uint64_t>(v));
            return;
        }
        if (v >= -32) {
            out_.push_back(static_cast<char>(static_cast<std::uint8_t>(v)));
        } else if (v >= INT8_MIN) {
            out_.push_back(static_cast<char>(0xD0));
            detail::put_be(out_, static_cast<std::uint8_t>(v), 1);
        } else if (v >= INT16_MIN) {
            out_.push_back(static_cast<char>(0xD1));
            detail::put_be(out_, static_cast<std::uint16_t>(v), 2);
        } else if (v >= INT32_MIN) {
            out_.push_back(static_cast<char>(0xD2));
            detail::put_be(out_, static_cast<std::uint32_t>(v), 4);
        } else {
            out_.push_back(static_cast<char>(0xD3));
            detail::put_be(out_, static_cast<std::uint64_t>(v), 8);
        }
    }

    void uinteger(std::uint64_t v) {
        if (v <= 0x7F) {
            out_.push_back(static_cast<char>(v));
        } else if (v <= 0xFF) {
            out_.push_back(static_cast<char>(0xCC));
            detail::put_be(out_, v, 1);
        } else if (v <= 0xFFFF) {
            out_.push_back(static_cast<char>(0xCD));
            detail::put_be(out_, v, 2);
        } else if (v <= 0xFFFFFFFFu) {
            out_.push_back(static_cast<char>(0xCE));
            detail::put_be(out_, v, 4);
        } else {
            out_.push_back(static_cast<char>(0xCF));
            detail::put_be(out_, v, 8);
        }
    }

    void floating(double d) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &d, sizeof bits);
        out_.push_back(static_cast<char>(0xCB));
        detail::put_be(out_, bits, 8);
    }

    void str(std::string_view s) {
        const std::size_t n = s.size();
        if (n <= 31) {
            out_.push_back(static_cast<char>(0xA0 | n));
        } else if (n <= 0xFF) {
            out_.push_back(static_cast<char>(0xD9));
            detail::put_be(out_, n, 1);
        } else if (n <= 0xFFFF) {
            out_.push_back(static_cast<char>(0xDA));
            detail::put_be(out_, n, 2);
        } else {
            out_.push_back(static_cast<char>(0xDB));
            detail::put_be(out_, n, 4);
        }
        out_.append(s.data(), n);
    }

    void bin(std::string_view s) {
        const std::size_t n = s.size();
        if (n <= 0xFF) {
            out_.push_back(static_cast<char>(0xC4));
            detail::put_be(out_, n, 1);
        } else if (n <= 0xFFFF) {
            out_.push_back(static_cast<char>(0xC5));
            detail::put_be(out_, n, 2);
        } else {
            out_.push_back(static_cast<char>(0xC6));
            detail::put_be(out_, n, 4);
        }
        out_.append(s.data(), n);
    }

    void array_header(std::size_t n) {
        if (n <= 15) {
            out_.push_back(static_cast<char>(0x90 | n));
        } else if (n <= 0xFFFF) {
            out_.push_back(static_cast<char>(0xDC));
            detail::put_be(out_, n, 2);
        } else {
            out_.push_back(static_cast<char>(0xDD));
            detail::put_be(out_, n, 4);
        }
    }

    void map_header(std::size_t n) {
        if (n <= 15) {
            out_.push_back(static_cast<char>(0x80 | n));
        } else if (n <= 0xFFFF) {
            out_.push_back(static_cast<char>(0xDE));
            detail::put_be(out_, n, 2);
        } else {
            out_.push_back(static_cast<char>(0xDF));
            detail::put_be(out_, n, 4);
        }
    }

    void ext(std::int8_t type, std::string_view data) {
        const std::size_t n = data.size();
        switch (n) {
        case 1: out_.push_back(static_cast<char>(0xD4)); break;
        case 2: out_.push_back(static_cast<char>(0xD5)); break;
        case 4: out_.push_back(static_cast<char>(0xD6)); break;
        case 8: out_.push_back(static_cast<char>(0xD7)); break;
        case 16: out_.push_back(static_cast<char>(0xD8)); break;
        default:
            if (n <= 0xFF) {
                out_.push_back(static_cast<char>(0xC7));
                detail::put_be(out_, n, 1);
            } else if (n <= 0xFFFF) {
                out_.push_back(static_cast<char>(0xC8));
                detail::put_be(out_, n, 2);
            } else {
                out_.push_back(static_cast<char>(0xC9));
                detail::put_be(out_, n, 4);
            }
        }
        out_.push_back(static_cast<char>(type));
        out_.append(data.data(), n);
    }

    /// A whole `Value`, recursively.
    void value(const Value& v) {
        switch (v.kind()) {
        case Kind::Nil: nil(); break;
        case Kind::Bool: boolean(v.as_bool()); break;
        case Kind::Int: integer(v.as_int()); break;
        case Kind::UInt: uinteger(v.as_uint()); break;
        case Kind::Float: floating(v.as_float()); break;
        case Kind::Str: str(v.as_str()); break;
        case Kind::Bin: bin(v.as_str()); break;
        case Kind::Array:
            array_header(v.as_array().size());
            for (const Value& x : v.as_array()) {
                value(x);
            }
            break;
        case Kind::Map:
            map_header(v.as_map().size());
            for (const Value::Entry& e : v.as_map()) {
                value(e.first);
                value(e.second);
            }
            break;
        case Kind::Ext: ext(v.ext_type(), v.ext_data()); break;
        }
    }

private:
    std::string& out_;
};

inline std::string encode(const Value& v) {
    std::string out;
    Writer(out).value(v);
    return out;
}

// ---- Reading -------------------------------------------------------------------------------

/// THE READER'S BOUNDS. Defaults sized for an embedded editor: a whole transferred document (at
/// most a few MiB) fits; a runaway producer does not.
struct Limits {
    std::size_t max_message_bytes = 64u * 1024u * 1024u;
    std::size_t max_depth = 64;
    std::size_t max_elements = 8u * 1024u * 1024u;
};

namespace detail {

/// What one type byte says: how many header bytes follow it, how its payload length is found,
/// and how many child items it opens. `ok` false for the one undefined byte.
struct Head {
    bool ok = true;
    std::size_t header = 0;        ///< bytes after the type byte that hold a length or a value
    std::size_t fixed_payload = 0; ///< payload bytes known from the type byte alone
    bool length_in_header = false; ///< payload length is the big-endian header value
    std::size_t ext_type_bytes = 0; ///< 1 for ext kinds (the type byte after the length)
    std::size_t fix_children = 0;   ///< child items known from the type byte alone
    bool children_in_header = false;
    std::size_t children_per = 1;   ///< 2 for maps
};

inline Head head_of(unsigned char b) {
    Head h;
    if (b <= 0x7F || b >= 0xE0) {
        return h; // positive/negative fixint
    }
    if (b >= 0xA0 && b <= 0xBF) {
        h.fixed_payload = b & 0x1Fu;
        return h;
    }
    if (b >= 0x90 && b <= 0x9F) {
        h.fix_children = b & 0x0Fu;
        return h;
    }
    if (b >= 0x80 && b <= 0x8F) {
        h.fix_children = b & 0x0Fu;
        h.children_per = 2;
        return h;
    }
    switch (b) {
    case 0xC0: case 0xC2: case 0xC3: return h;
    case 0xC4: h.header = 1; h.length_in_header = true; return h;
    case 0xC5: h.header = 2; h.length_in_header = true; return h;
    case 0xC6: h.header = 4; h.length_in_header = true; return h;
    case 0xC7: h.header = 1; h.length_in_header = true; h.ext_type_bytes = 1; return h;
    case 0xC8: h.header = 2; h.length_in_header = true; h.ext_type_bytes = 1; return h;
    case 0xC9: h.header = 4; h.length_in_header = true; h.ext_type_bytes = 1; return h;
    case 0xCA: h.fixed_payload = 4; return h;
    case 0xCB: h.fixed_payload = 8; return h;
    case 0xCC: h.fixed_payload = 1; return h;
    case 0xCD: h.fixed_payload = 2; return h;
    case 0xCE: h.fixed_payload = 4; return h;
    case 0xCF: h.fixed_payload = 8; return h;
    case 0xD0: h.fixed_payload = 1; return h;
    case 0xD1: h.fixed_payload = 2; return h;
    case 0xD2: h.fixed_payload = 4; return h;
    case 0xD3: h.fixed_payload = 8; return h;
    case 0xD4: h.fixed_payload = 2; return h; // type + 1
    case 0xD5: h.fixed_payload = 3; return h;
    case 0xD6: h.fixed_payload = 5; return h;
    case 0xD7: h.fixed_payload = 9; return h;
    case 0xD8: h.fixed_payload = 17; return h;
    case 0xD9: h.header = 1; h.length_in_header = true; return h;
    case 0xDA: h.header = 2; h.length_in_header = true; return h;
    case 0xDB: h.header = 4; h.length_in_header = true; return h;
    case 0xDC: h.header = 2; h.children_in_header = true; return h;
    case 0xDD: h.header = 4; h.children_in_header = true; return h;
    case 0xDE: h.header = 2; h.children_in_header = true; h.children_per = 2; return h;
    case 0xDF: h.header = 4; h.children_in_header = true; h.children_per = 2; return h;
    default: h.ok = false; return h; // 0xC1: never used
    }
}

/// Materialize one value from a range the scanner already proved complete and in bounds.
inline Value build(const unsigned char* data, std::size_t& pos) {
    const unsigned char b = data[pos++];
    if (b <= 0x7F) {
        return Value::integer(b);
    }
    if (b >= 0xE0) {
        return Value::integer(static_cast<std::int8_t>(b));
    }
    const auto str_of = [&](std::size_t n) {
        std::string s(reinterpret_cast<const char*>(data + pos), n);
        pos += n;
        return s;
    };
    if (b >= 0xA0 && b <= 0xBF) {
        return Value::str(str_of(b & 0x1Fu));
    }
    const auto children = [&](std::size_t n, bool map) {
        if (map) {
            Value::Map m;
            m.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                Value k = build(data, pos);
                Value v = build(data, pos);
                m.emplace_back(std::move(k), std::move(v));
            }
            return Value::map(std::move(m));
        }
        Value::Array a;
        a.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            a.push_back(build(data, pos));
        }
        return Value::array(std::move(a));
    };
    if (b >= 0x90 && b <= 0x9F) {
        return children(b & 0x0Fu, false);
    }
    if (b >= 0x80 && b <= 0x8F) {
        return children(b & 0x0Fu, true);
    }
    const auto be = [&](int n) {
        const std::uint64_t v = get_be(data + pos, n);
        pos += static_cast<std::size_t>(n);
        return v;
    };
    switch (b) {
    case 0xC0: return Value::nil();
    case 0xC2: return Value::boolean(false);
    case 0xC3: return Value::boolean(true);
    case 0xC4: return Value::bin(str_of(static_cast<std::size_t>(be(1))));
    case 0xC5: return Value::bin(str_of(static_cast<std::size_t>(be(2))));
    case 0xC6: return Value::bin(str_of(static_cast<std::size_t>(be(4))));
    case 0xC7:
    case 0xC8:
    case 0xC9: {
        const std::size_t n =
            static_cast<std::size_t>(be(b == 0xC7 ? 1 : (b == 0xC8 ? 2 : 4)));
        const std::int8_t type = static_cast<std::int8_t>(data[pos++]);
        return Value::ext(type, str_of(n));
    }
    case 0xCA: {
        const std::uint32_t bits = static_cast<std::uint32_t>(be(4));
        float f = 0.0f;
        std::memcpy(&f, &bits, sizeof f);
        return Value::floating(static_cast<double>(f));
    }
    case 0xCB: {
        const std::uint64_t bits = be(8);
        double d = 0.0;
        std::memcpy(&d, &bits, sizeof d);
        return Value::floating(d);
    }
    case 0xCC: return Value::uinteger(be(1));
    case 0xCD: return Value::uinteger(be(2));
    case 0xCE: return Value::uinteger(be(4));
    case 0xCF: return Value::uinteger(be(8));
    case 0xD0: return Value::integer(static_cast<std::int8_t>(be(1)));
    case 0xD1: return Value::integer(static_cast<std::int16_t>(be(2)));
    case 0xD2: return Value::integer(static_cast<std::int32_t>(be(4)));
    case 0xD3: return Value::integer(static_cast<std::int64_t>(be(8)));
    case 0xD4:
    case 0xD5:
    case 0xD6:
    case 0xD7:
    case 0xD8: {
        const std::size_t n = b == 0xD4   ? std::size_t{1}
                              : b == 0xD5 ? std::size_t{2}
                              : b == 0xD6 ? std::size_t{4}
                              : b == 0xD7 ? std::size_t{8}
                                          : std::size_t{16};
        const std::int8_t type = static_cast<std::int8_t>(data[pos++]);
        return Value::ext(type, str_of(n));
    }
    case 0xD9: return Value::str(str_of(static_cast<std::size_t>(be(1))));
    case 0xDA: return Value::str(str_of(static_cast<std::size_t>(be(2))));
    case 0xDB: return Value::str(str_of(static_cast<std::size_t>(be(4))));
    case 0xDC: return children(static_cast<std::size_t>(be(2)), false);
    case 0xDD: return children(static_cast<std::size_t>(be(4)), false);
    case 0xDE: return children(static_cast<std::size_t>(be(2)), true);
    case 0xDF: return children(static_cast<std::size_t>(be(4)), true);
    default: return Value::nil(); // unreachable: the scanner refused undefined bytes
    }
}

} // namespace detail

inline std::int64_t Value::handle() const noexcept {
    if (kind_ != Kind::Ext || s_.empty()) {
        return -1;
    }
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s_.data());
    const detail::Head h = detail::head_of(p[0]);
    if (!h.ok || h.fix_children != 0 || h.children_in_header || h.length_in_header) {
        return -1;
    }
    if (s_.size() < 1 + h.fixed_payload) {
        return -1;
    }
    std::size_t pos = 0;
    const Value inner = detail::build(p, pos);
    return inner.is_int() ? inner.as_int() : -1;
}

/// THE READER: bytes in, whole values out, bounded.
class Decoder {
public:
    explicit Decoder(Limits limits = {}) : limits_(limits) {}

    enum class Status : std::uint8_t {
        Value,     ///< `value` holds one complete top-level value
        NeedMore,  ///< no complete value yet; feed more bytes
        Malformed, ///< the stream is not msgpack this reader accepts; `why` says where
        TooLarge,  ///< a message outgrew `Limits::max_message_bytes`
    };
    struct Result {
        Status status = Status::NeedMore;
        Value value;
        std::string why;
    };

    void feed(std::string_view bytes) { buf_.append(bytes.data(), bytes.size()); }

    /// Bytes received and not yet handed out as values.
    std::size_t buffered() const noexcept { return buf_.size() - head_; }

    /// The next complete value, or why there is none. After Malformed or TooLarge the reader is
    /// finished: every later call answers the same, because a byte stream that lost its framing
    /// cannot be trusted to find it again.
    Result next() {
        Result r;
        if (failed_ != Status::Value) {
            r.status = failed_;
            r.why = failure_;
            return r;
        }
        if (!scanning_) {
            if (head_ == buf_.size()) {
                compact();
                return r; // NeedMore
            }
            scanning_ = true;
            scan_ = head_;
            elements_ = 0;
            stack_.assign(1, 1);
        }
        const unsigned char* data = reinterpret_cast<const unsigned char*>(buf_.data());
        while (!stack_.empty()) {
            if (stack_.back() == 0) {
                stack_.pop_back();
                continue;
            }
            if (scan_ >= buf_.size()) {
                return need_more();
            }
            const unsigned char b = data[scan_];
            const detail::Head h = detail::head_of(b);
            if (!h.ok) {
                return fail(Status::Malformed, "undefined msgpack type byte 0xc1 at offset " +
                                                   std::to_string(scan_ - head_));
            }
            if (scan_ + 1 + h.header > buf_.size()) {
                return need_more();
            }
            std::size_t payload = h.fixed_payload;
            std::size_t children = h.fix_children;
            if (h.length_in_header) {
                payload = static_cast<std::size_t>(detail::get_be(data + scan_ + 1,
                                                                  static_cast<int>(h.header))) +
                          h.ext_type_bytes;
            }
            if (h.children_in_header) {
                children = static_cast<std::size_t>(
                    detail::get_be(data + scan_ + 1, static_cast<int>(h.header)));
            }
            const std::size_t end = scan_ + 1 + h.header + payload;
            if (end - head_ > limits_.max_message_bytes) {
                return fail(Status::TooLarge,
                            "a message larger than " + std::to_string(limits_.max_message_bytes) +
                                " bytes");
            }
            if (end > buf_.size()) {
                return need_more();
            }
            // COMMIT this item: it is wholly present (a container's header is its whole self).
            --stack_.back();
            scan_ = end;
            ++elements_;
            if (elements_ > limits_.max_elements) {
                return fail(Status::Malformed, "a message with more than " +
                                                   std::to_string(limits_.max_elements) + " items");
            }
            const std::size_t items = children * h.children_per;
            if (items > 0) {
                if (stack_.size() + 1 > limits_.max_depth) {
                    return fail(Status::Malformed, "nesting deeper than " +
                                                       std::to_string(limits_.max_depth));
                }
                // A declared count no byte could satisfy is refused before it is believed.
                if (items > limits_.max_elements) {
                    return fail(Status::Malformed, "a container declaring " + std::to_string(items) +
                                                       " items");
                }
                stack_.push_back(items);
            }
        }
        // THE MESSAGE IS [head_, scan_) AND IT IS COMPLETE.
        std::size_t pos = head_;
        r.value = detail::build(data, pos);
        r.status = Status::Value;
        head_ = scan_;
        scanning_ = false;
        compact();
        return r;
    }

private:
    Result need_more() {
        Result r;
        if (scan_ - head_ > limits_.max_message_bytes) {
            return fail(Status::TooLarge, "a message larger than " +
                                              std::to_string(limits_.max_message_bytes) + " bytes");
        }
        return r;
    }

    Result fail(Status s, std::string why) {
        failed_ = s;
        failure_ = std::move(why);
        Result r;
        r.status = s;
        r.why = failure_;
        return r;
    }

    void compact() {
        if (head_ > 0 && (head_ == buf_.size() || head_ > (1u << 20))) {
            buf_.erase(0, head_);
            if (scanning_) {
                scan_ -= head_;
            }
            head_ = 0;
        }
    }

    Limits limits_;
    std::string buf_;
    std::size_t head_ = 0;
    bool scanning_ = false;
    std::size_t scan_ = 0;
    std::size_t elements_ = 0;
    std::vector<std::size_t> stack_;
    Status failed_ = Status::Value;
    std::string failure_;
};

} // namespace zengine::neovim::msgpack

#endif // ZENGINE_NEOVIM_MSGPACK_HPP
