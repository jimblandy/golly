Files added to the assets folder are automatically included in Golly.apk.
Note that the following zip archives must be added:

Help.zip      -- compressed copy of golly/gui-common/Help
Patterns.zip  -- compressed copy of golly/Patterns
Rules.zip     -- compressed copy of golly/Rules

These zip files will be unzipped and copied into appropriate
directories in internal storage (see the initPaths method in
MainActivity.java).

The zip files are currently not included in the git repository
(it would be an unnecessary waste of space).
