### mod_upload

mod_upload is an apache module for uploading files.

### mod_jsonindex

mod_jsonindex is like mod_index but instead of returning HTML it returns JSON.

Not so FancyIndexing, more like SexyIndexing.

Combining these two modules with a bit of static HTML, JQuery and some JavaScript and you have an HTTP fileserver.

The project includes the static and the JavaScript and the Apache conf to set it up.

It does not install out of-the-box you need to be familiar with Apache config and do some copy pasteing.

Build is a bash script, I never grokked make. It builds OK on my Fedora 32 bit and 64 bit systems, not tried it on any other distros.  Any volunteers to tidy that up is welcome, as are ports to any other OS.

mod_upload and mod_jsonindex are in no way dependent, you can run one without the other.

You can just run mod_upload and then you have a way to accept files and no way to download them again so some back end script can check for viruses or otherwise sanitize the data prior to publishing it.


Please report bugs if you find them.

mod_jsonindex is based on Core Apache code for mod_index so the same Apache 2 license applies to the code.
The original authors are, Rob McCool and Martin Pool they deserve the credit any bugs are mine.

mod_upload is GPL3, thats right the "viral" one that scares Micro$oft and 0racle, thus you must be open 
with pathces.  I don't want your money honey, I just want your pull request.

