// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_CODEC_HPP
#define ZENGINE_INVENTORY_CODEC_HPP

// THE CAPTURE ADAPTER: one item plus its metadata list, as one self-sufficient byte envelope.
//
// The item is the pure data a maker wanted to keep; the metadata list is what was known about
// it at capture, and its entries may each claim a different schema. Neither the item's nor a
// metadata entry's schema is compiled into this file: a fixed Message field would fix one
// schema, and a list has one declared element type, so heterogeneity is bought with exactly one
// level of indirection -- a combined schema-descriptor closure (the same `zen.SchemaDesc`
// vocabulary `zen/weave/describe.hpp` already uses for a weave's own self-description) plus one
// native-serialized value per root, in the closure's own order.
//
// This is `zengine::message_draft::Library`'s encoding (`message-draft/library.hpp`), stripped
// of the one thing that does not belong here: `Library`'s `OptionalProjection` exists to persist
// an UNFINISHED draft, relaxing every field to optional. An inventory pair is never unfinished --
// `encode_pair` admits the item and every metadata entry against their OWN declared schema before
// anything is written, so a required field left absent is refused here, not silently stored.

#include <zen/gate.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/describe.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::inventory {

namespace detail {
inline loom::Cell byte_cell(std::string_view bytes) {
    return loom::Cell::bytes(loom::Bytes(bytes.begin(), bytes.end()));
}
inline std::string_view byte_view(const loom::Cell& cell) {
    const loom::Bytes& bytes = cell.as_bytes();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
} // namespace detail

/// The envelope's own wire schema. `schemas` is the combined descriptor closure for every root
/// this pair claims -- the item's schema first, then each metadata entry's, in order (the same
/// `accepted`/`referenced` split `zen.AcceptedShapes` already carries). `item` and each entry of
/// `metadata` are native `serialize()` bytes, decodable only once `schemas` has supplied their
/// declared root -- this outer schema's own admission says nothing about what they contain.
inline std::shared_ptr<const loom::Schema> pair_schema() {
    static const auto schema =
        loom::SchemaBuilder("zengine.inventory.Pair", 1)
            .message("schemas", loom::accepted_shapes_schema())
            .field("item", loom::Kind::Bytes)
            .list("metadata", loom::type_of(loom::Kind::Bytes))
            .build();
    return schema;
}

/// One decoded pair: the item and its metadata, each a complete Value admitted against a schema
/// this call recovered from the envelope's own closure -- never from a caller's compiled
/// knowledge of either shape.
struct DecodedPair {
    loom::Value item;
    std::vector<loom::Value> metadata;
};

/// Encode `item` and `metadata` as one self-sufficient envelope.
///
/// Every value must already admit against its OWN claimed schema -- this is complete-data
/// admission, not a draft projection. Throws std::invalid_argument when the item or a metadata
/// entry does not admit, and loom::SchemaConflict when two roots (the item, or two metadata
/// entries, or a metadata entry and the item) disagree under one (name, version): the whole
/// closure is agreed through one throwaway Registry claim before any byte is written, so a
/// caller sees the same refusal `Registry::claim` would give a live consumer of the same
/// vocabulary, not a corrupted envelope discovered only on decode.
inline std::string encode_pair(const loom::Value& item, const std::vector<loom::Value>& metadata) {
    const loom::Admission item_admitted = loom::admit(item, item.schema());
    if (!item_admitted) {
        throw std::invalid_argument("inventory item does not admit against its own schema: " +
                                    item_admitted.first_error().message());
    }
    std::vector<std::shared_ptr<const loom::Schema>> roots;
    roots.push_back(item.schema_ptr());
    loom::Cell::Array metadata_bytes;
    metadata_bytes.reserve(metadata.size());
    for (std::size_t i = 0; i < metadata.size(); ++i) {
        const loom::Admission admitted = loom::admit(metadata[i], metadata[i].schema());
        if (!admitted) {
            throw std::invalid_argument("inventory metadata[" + std::to_string(i) +
                                        "] does not admit against its own schema: " +
                                        admitted.first_error().message());
        }
        roots.push_back(metadata[i].schema_ptr());
        metadata_bytes.push_back(detail::byte_cell(loom::serialize(admitted.value())));
    }
    loom::Registry agreement;
    std::vector<std::shared_ptr<const loom::Schema>> closure;
    for (const std::shared_ptr<const loom::Schema>& root : roots) {
        loom::collect_referenced(*root, closure);
        closure.push_back(root);
    }
    loom::SchemaClaimScope claim = agreement.claim(closure); // throws SchemaConflict on disagreement
    (void)claim;
    loom::Value envelope(pair_schema());
    envelope.set("schemas", loom::Cell::message(loom::encode_accepted_shapes(roots)));
    envelope.set("item", detail::byte_cell(loom::serialize(item_admitted.value())));
    envelope.set("metadata", loom::Cell::list(std::move(metadata_bytes)));
    return loom::serialize(envelope);
}

/// Decode a `bytes` envelope back to its item and metadata.
///
/// Throws std::invalid_argument on anything that is not a genuinely well-formed,
/// self-consistent envelope: malformed bytes, an empty closure, a metadata count that disagrees
/// with the declared roots, or a value that does not admit against the root its own position in
/// the closure names. Throws loom::SchemaConflict if the closure itself disagrees internally.
/// A generic caller needs no compiled knowledge of the item's or any metadata entry's shape:
/// every schema used here comes from `bytes` alone.
inline DecodedPair decode_pair(std::string_view bytes) {
    const loom::Admission envelope = loom::admit(loom::parse(bytes), pair_schema());
    if (!envelope) {
        throw std::invalid_argument("inventory pair does not admit: " +
                                    envelope.first_error().message());
    }
    const loom::Value& vocabulary = *envelope.value().get("schemas")->as_message();
    loom::Registry deps;
    loom::decode_accepted_referenced(vocabulary, deps);
    const std::vector<std::shared_ptr<const loom::Schema>> roots =
        loom::decode_accepted_roots(vocabulary, deps);
    if (roots.empty()) {
        throw std::invalid_argument("inventory pair declares no item schema in its own closure");
    }
    loom::SchemaClaimScope claim = deps.claim(roots); // throws SchemaConflict on disagreement
    (void)claim;
    const loom::Admission item =
        loom::admit(loom::parse(detail::byte_view(*envelope.value().get("item"))), roots.front());
    if (!item) {
        throw std::invalid_argument("inventory pair item does not admit against its declared "
                                    "schema: " +
                                    item.first_error().message());
    }
    const loom::Cell::Array& metadata_bytes = envelope.value().get("metadata")->as_list();
    if (metadata_bytes.size() != roots.size() - 1) {
        throw std::invalid_argument(
            "inventory pair carries " + std::to_string(metadata_bytes.size()) +
            " metadata value(s) for " + std::to_string(roots.size() - 1) +
            " declared metadata root(s)");
    }
    DecodedPair out{item.value(), {}};
    out.metadata.reserve(metadata_bytes.size());
    for (std::size_t i = 0; i < metadata_bytes.size(); ++i) {
        const loom::Admission entry =
            loom::admit(loom::parse(detail::byte_view(metadata_bytes[i])), roots[i + 1]);
        if (!entry) {
            throw std::invalid_argument("inventory pair metadata[" + std::to_string(i) +
                                        "] does not admit against its declared schema: " +
                                        entry.first_error().message());
        }
        out.metadata.push_back(entry.value());
    }
    return out;
}

} // namespace zengine::inventory

#endif // ZENGINE_INVENTORY_CODEC_HPP
