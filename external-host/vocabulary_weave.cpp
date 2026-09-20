// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `zengine-guest-vocabulary` -- Workshop's guest vocabulary, as the artifact an external Loom host
// boots (vocabulary_weave.hpp says why a host that asks a Workshop needs it). A `loom-boot.json`
// row names it; the host's operator trusts it; it needs no rule, because it says nothing.

#include "vocabulary_weave.hpp"

#include <zen/kernel/export.hpp>

ZEN_EXPORT_WEAVE(zengine::external_host::GuestVocabulary)
