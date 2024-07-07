`adrus` -- A simple CLI note-taking program
===========================================

`adrus` is designed to be a simple and ergonomic way to create, manage, and query a notebook. 

## Usage 
```sh
# environment variables
export ADRUS_DIR=$HOME/foo/bar       # location of adrus notebook
export LOG_FILTER=trace              # verbosity of log output (from lowest to highest):
                                     #   error, warn, info, debug, trace
export EDITOR=vi                     # text editor of your choice

# NOTE: all paths are relative to the notebook location

# creating/opening a note (opens in your editor)
adrus /path/to/note

# adding/removing tags from a note
adrus /path/to/note +foo -bar

# querying the notebook for posts with/without specified tags
adrus +foo -bar

# listing all notes in the notebook
adrus

# listing notes who's name match a pattern, and match given tags 
adrus ls /path/**/*  +foo -bar

# removing notes who's name match a pattern, and match given tags
adrus rm /path/**/* +foo -bar
```

## Design
### Command Output
All `adrus` commands will log to `stderr`, and write useful output to `stdout`. When a command produces multiple items of output, they will be separated by a newline. This is to make it trivial to use the output of `adrus` commands with other programs, for example to sort them alphabetically:
```sh
adrus +foo -bar | sort
```

### Notes
An `adrus` note is simply a text file, somewhere within the `ADRUS_DIR`, that starts with a line that looks like this:
```
adrus tag1 tag2...
```
It starts with the word `adrus`, and then a space-delimited list of tags until a newline (`\n`) or end-of-file is encountered. The rest of the file is ignored, and contains the body of the note.

In order to avoid complicating the format, all future changes to the note format will still maintain a one-line header, and the syntax of that header will simply grow to accomodate new features.

A note has a "name", which is it's path relative to the notebook. For example, `/foo/bar` would have the path `$ADRUS_DIR/foo/bar`.

### Queries
Queries are written as a sequence of positive or negative tag bounds; for example `+foo -bar` would match against notes that have the `foo` tag, but do not have the `bar` tag. You can match for/against any number of tags.

### Patterns
When using the `rm` or `ls` sub-commands, you have the ability to match against the names of notes. Right now, the pattern makes use of GNU-extended POSIX globs (see: `glob(7)`), however soon there will be an in-house pattern syntax, which will look (roughly) like this:

* `*` -- matches any sequence of characters, for example `/foo*bar` would match against notes who's name starts with `foo`, and ends with `bar`, with anything in-between. 
* `**` -- recursively matches directories, for example `/**/bar` would match any note with the name `bar` in any subdirectory of your notebook

An example of this pattern syntax in use could be:
```sh
adrus rm /lang/**/* +germanic 
```
Which would delete any note under the `/lang` folder that has the `germanic` tag. This would delete e.g. `/lang/north/english`, `/lang/middle/german`, `/lang/middle/yiddish`, etc.


## Ideas
In future, there are a few ideas I would like to implement:

* attributes -- Tags will be able to contain data. Syntactically, this might look like `foo bar=123` 
* externally tagged notes -- A separate "ledger" file will contain tags and metadata for non-text notes. This allows PNG/PDF/etc. files, which cannot contain the `adrus` header while still being valid, to be indexed by `adrus`
* hooks -- the ability to run custom code when certain actions are performed, such as opening, deleting, querying, etc. notes.

## License
`adrus` is licensed under the GNU GPLv3.0 or later. See [license](LICENSE) for more information.
