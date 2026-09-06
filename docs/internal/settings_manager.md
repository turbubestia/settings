# `settings_manager` — Source Documentation & Static Analysis

- **Header:** `src/settings_manager.hpp`
- **Implementation:** `src/settings_manager.cpp` (counterpart found)
- **Namespace:** `turbubestia::settings`
- **Dependencies (contextual):** `src/assert.hpp` (`RUNTIME_ASSERT`), `nlohmann/json.hpp` (vcpkg), C++20 `<ranges>`, `<filesystem>`.

## Purpose & Role

A small, self-contained settings subsystem. It lets callers register typed
*schemas* for named settings, store/validate *values* against those schemas, and
persist the active values to a JSON file (and load them back). Keys are
hierarchical strings of the form `/section/subsection/name`, which map onto nested
JSON objects via `nlohmann::json` flatten/unflatten.

The three public types are:

- `setting_value` — a nullable, typed scalar holder (`bool` / `int` / `double` /
  `std::string`) backed by `std::optional<std::variant<...>>`.
- `setting_schema` — metadata + constraints for one setting key (type, title,
  description, default, enum options, validator).
- `settings_manager` — the registry of schemas and the flat store of active
  values, plus file serialization.

A `nlohmann::adl_serializer<turbubestia::settings::setting_value>` specialization
is defined in `settings_manager.cpp` (inside `namespace nlohmann`) so that a map of
`setting_value` can be serialized/deserialized directly to/from JSON. It dispatches
on the held alternative for `to_json`, and on the JSON tag (`is_boolean`,
`is_number_integer`, `is_number_float`, `is_string`) for `from_json`.

## `setting_value`

A value is either *empty* (no value) or holds exactly one of the four scalar
alternatives.

- **Construction:** default (empty), copy/move, and explicit single-arg
  constructors for `bool`, `int`, `double`, `std::string`.
- **Assignment:** a templated `operator=(const T&)` stores `T` into the variant;
  non-template copy/move assignment are also provided. Because the target is a
  `std::variant`, assigning a type that is not one of the four alternatives is a
  compile error (good — mismatches are caught at compile time).
- **`has_value()` / `operator bool`:** true when a value is present.
- **`type()`:** returns the `std::type_index` of the held alternative via
  `std::visit` + `std::decay_t`; returns `typeid(void)` when empty.
- **Accessors:** `to_bool/to_int/to_double/to_string` call the private
  `get<T>()`, which **throws `std::runtime_error`** if the value is empty or the
  held type does not match `T`.
- **Equality:** friend `operator==` overloads compare a `setting_value` against a
  raw `T` (both orders) and against another `setting_value`. The raw-`T` forms use
  an `if constexpr` branch: when the decayed type is `const char*`/`char*` they
  compare against the `std::string` alternative; otherwise they require
  `type() == typeid(T)` before comparing contents.

### Usage pattern

```cpp
setting_value v = 8;                 // int
int n = v.to_int();                  // throws on type mismatch / empty
bool ok = (v == 8);                  // true
setting_value s = std::string("a/b");
bool lit = (s == "a/b");             // true — const char* branch compares contents
```

## `setting_schema`

Immutable key + type, mutable metadata via fluent setters.

- **Constructor** `setting_schema(key, std::type_index)` — `RUNTIME_ASSERT`s that
  the key passes `detail::is_valid_key_syntax` (non-empty, no spaces, must start
  with `/`, no empty path segments). A bad key **throws**.
- **Fluent setters:** `title`, `description`, `default_value`, `enum_options`,
  `validator`. The templated `default_value(T)` `RUNTIME_ASSERT`s
  `typeid(T) == _type` (throws on mismatch); the `setting_value` overload does
  **not** type-check.
- **`is_valid(const setting_value&)`:** returns false if the value's type differs
  from the schema type; then applies the optional `_validator`; then, for string
  schemas with non-empty `_enum_options`, requires exact membership.
- **`to_display_format()`:** splits the key (minus the leading `/`) on `/` and
  returns each segment passed through `detail::to_display_format` (a
  Title-Case / hyphen-to-space formatter).

## `settings_manager`

Two maps: `_values` (active values) and `_schema_registry` (schemas), both keyed
by the setting key string.

- **`get(key)`** — returns a `setting_value` **by value**:
  1. the active value if present, else
  2. the schema's default value, else
  3. an empty `setting_value`.

  Returning by value removes any dangling-reference hazard from retaining the
  result across mutating calls.
- **`set(key, value)`** — returns `bool`. Rejects (returns false) on: invalid key
  syntax, unregistered key, or a value that fails `schema.is_valid`. On success it
  inserts/updates `_values`. A templated `set<T>(key, T)` forwards to the
  `setting_value` overload.
- **`register_schema(schema)`** — inserts or replaces a schema. On replacement it
  re-validates the currently active value against the *new* schema and erases it
  from `_values` if it no longer validates.
- **`schema(key)` / `schemas()`** — read access to one (as `std::optional`) / all
  schemas.
- **`active_values()`** — read access to the flat value map.

### File serialization

Persistence paths are deliberately restricted. `detail::resolve_persistence_path`
only accepts a *bare filename* (no `/` or `\`, not absolute) and resolves it under
`<base>/.config/dir2md/`, where `<base>` is the test base directory if set, else
the user home (`USERPROFILE` / `HOME`, falling back to the temp dir). The resolved
path must pass `detail::is_within_path` (case-insensitive prefix check against the
config root) or resolution fails. This is a path-traversal guard: absolute paths
and paths containing separators are rejected outright.

- **`save_to_file(path)`** — flattens `_values` into nested JSON via
  `nlohmann::json::unflatten()` (default separator `/`, matching the key syntax),
  creates the parent directory, and writes pretty-printed JSON. It returns `false`
  if path resolution fails, if the parent directory cannot be created, or if the
  output stream cannot be opened; otherwise `true`.
- **`load_from_file(path)`** — reads and parses JSON; on parse failure or a
  non-object root it returns false without touching `_values`. It then flattens the
  object and, for each key that has a registered schema and passes
  `schema.is_valid`, collects it into a temporary map. The temp map is then moved
  into `_values` in one assignment (**replace** semantics): keys present in memory
  but absent from the file are dropped, and unknown/invalid file keys are skipped.

### Test-only API

`set_test_base_directory_path` / `clear_test_base_directory` /
`test_base_directory_path` / `test_base_directory` manipulate a shared static base
directory so unit tests can redirect persistence away from the real home dir.

## Invariants & Assumptions

- **Key syntax** is enforced at schema construction and at `set`; keys are
  `/`-prefixed, space-free, with no empty segments.
- **Type safety** is by `std::type_index` comparison, not by C++ type conversion:
  a value only validates against a schema when its held alternative exactly matches
  the schema's registered type.
- **No thread-safety guarantees** are documented or implemented; all state (both
  maps and the static test base) is unsynchronized.
- **Error handling is mixed:** `get<T>`/`to_*` and `RUNTIME_ASSERT` sites throw
  `std::runtime_error`, while `set`/`save_to_file`/`load_from_file` signal failure
  via `bool`.

## Static Analysis and Security


### Residual risks / assumptions not fully analyzed

- **`nlohmann::json` flatten/unflatten behavior** (default `/` separator, and how
  integer-valued floats are re-typed on round-trip) is a contextual dependency from
  vcpkg. The `adl_serializer::from_json` maps `is_number_integer` → `int` and
  `is_number_float` → `double`, so a `double` schema whose stored value is an
  integral float (e.g., `3.0` serialized as `3`) would deserialize as `int` and then
  fail `schema.is_valid` on load. The save/load round-trip tests in
  `tests/setting_manager_test.cpp` cover the common cases but not this edge.
- **Home-directory resolution** (`USERPROFILE`/`HOME`, temp-dir fallback) and the
  use of a `.config/dir2md` subdirectory on Windows are platform conventions that
  were taken as given, not validated against target deployment expectations.
- **`is_within_path`** relies on `weakly_canonical` + a case-insensitive string
  prefix check with a forced trailing separator on the root; this is sound for the
  simple-filename inputs actually permitted, but its behavior under symlinks or
  unusual filesystem casing was not exercised.
- **`detail::to_display_format`** (now reachable via `setting_schema::to_display_format`)
  is a small state machine with no dedicated unit tests; edge cases such as leading
  hyphens, consecutive hyphens, and non-alphabetic first characters were not
  independently verified.
