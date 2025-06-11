# FolderListModel

This is a copy of the FolderListModel included with Qt Declarative, which has a
fundamental issue resulting in it being unable to handle folders with a #
anywhere in the path. Since Zynthbox does music related things, that is pretty
unacceptable, but as the rest of the model works so well, rather than
reimplement everything, we just clone that, and work around the issue.

This code is license-compatible with libzynthbox (GPL 2.0 or later, which we
match), and while for sure upstreaming a fix would be advantageous in general,
Qt 5.15 is eol, and until we switch to Qt 6, it would be a moot point. So, for
now, this.
