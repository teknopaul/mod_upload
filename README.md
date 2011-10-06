### mod_upload

mod_upload is an apache module for uploading files.

### mod_jsonindex

mod_jsonindex is like mod_index but instead of returning HTML it returns JSON.

Not so FancyIndexing, more like SexyIndexing.

Combining these two modules with a bit of static HTML, JQuery and some JavaScript and you have an HTTP fileserver.

The project includes the static and the JavaScript and the Apache conf to set it up.

It does not install out of-the-box you need to be familiar with Apache config and do some copy pasteing.

The project comes with no license and no guarantees, use at your own risk. I'm not a confident enough C hacker to say that this is safe code.

Having said that I've been running it for a while and it does not crash on me. It uses Apaches APR code which is pretty stable and APR manages the memory so I doubt it has memory leaks.

Build is a bash script, I never grokked make. It builds OK on my Fedora 32 bit and 64 bit systems, not tried it on any other distros.  Any volunteers to tidy that up is welcome, as are ports to any other OS.

mod_upload and mod_jsonindex are in no way dependent, you can run one without the other.

You can just run mod_upload and then you have a way to accept files and no way to download them again so some back end script can check for viruses or otherwise sanitize the data prior to publishing it.


Please report bugs if you find them.

mod_jsonindex is based on Core Apache code for mod_index so the same Apache 2 license applies to the code.
The original authors are, Rob McCool and Martin Pool they deserve the credit any bugs are mine.

FOSS 4 Ever! Forward the evolution

