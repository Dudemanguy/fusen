# fusen
fusen is a simple program for adding arbitrary tags to any file and quickly filtering/searching through them.

## Building
You will need the following dependencies.

* Qt6
* sqlite3
* yaml-cpp
* meson (makedep)

After checking out the source, navigate to the directory, and then simply run.
```
meson setup build -Dprefix=/usr
meson compile -C build
sudo meson install -C build
```
By default, mpv is used for trying to open files.

## Tagging
In the dialog, you can add any number of files or directories to the database. In the list, you can right-click
any number of files and select `Tag` to add or remove tags. In the popup box, tags are comma-delineated which means
commas are not allowed as a possible tag name. Additionally, ` `, `'`, and `"` are all automatically converted
to `_` internally for simplicity. That means that, in practice, `foo bar` and `foo_bar` should be the same.

## Filtering
In the search box at, tags can be searched for matches and update the list of entries. Multiple tags are
comma-delineated. Additionally, prepending the tag with a `-` character performs an exclusion operation instead.
In other words, `-foo` would return all entries that are *not* tagged with `foo`. Checking the "Exact Tag Match" box
will search for strictly only exact matches for tags. Leaving it unchecked will search both the path name for partial
matches as well as tags for partial matches.

## Importing Tags
You can mass import tags for simplicity. fusen accepts files in yaml that are formatted as shown below:
```
filepath1:
    - tag1
    - tag2
    - tag3
filepath2:
    - tag2
    - tag4
```
If the option `Clear Existing Tags on Import` is checked, any existing tags that exist for that particular file will be
cleared before the new tags are applied.

## License
GPLv3
