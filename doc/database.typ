#set heading(numbering: "1.1.1.a")
#outline()
#pagebreak()

#let eg = [_e.g._];
#let ie = [_i.e._];

= Database Design
== Notes
=== On-Disk
Notes are named by their path relative to the notebook directory (#ie a note located at `$ADRUS_DIR/foo/bar/baz` will have the name `/foo/bar/baz`). 

Notes are plain-text files that start with the word `adrus` and a list of attributes, terminated by a new line:
```tcl
adrus attr1 attr2=value...
...
```

Attributes are a key with an (optional) value attached to them; see `db_attr_t` for more info.

=== `db_note_t`
- `name` -- path relative to notebook
- `ctime` -- time of creation
- `mtime` -- time of modification
- `attrs` -- a list of `db_attr_t` that a note has attached to them

=== `db_attr_t` 
- `key` -- name of the attribute
- `val` -- the value attached to the attribute; can be any of the following types:
  - integer
  - string
  - date/time

== Lookup Information 
