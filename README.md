### mod_upload

mod_upload is an apache module for uploading files.

### cgiupload

cgiupload is a small C program that can handle uploading files from a CGI interface.
This makes it possible to add an upload feature to anything that supports CGI .e.g nginx with cgiwrap

e.g.  upload.cgi

    #/bin/bash
    /opt/ops/cgi/utils/cgiupload "/var/archive/" "$CONTENT_TYPE" true
    if [ $? == 0 ] ; then  
        echo Content-Type: text/html
        echo
        echo '<html><body>Upload OK</body></html>'
    fi

Build is a bash script cgi.sh, it builds fine on my raspberry pi, should work on x86 too.

### mod_jsonindex

mod_jsonindex is like mod_index but instead of returning HTML it returns JSON.

Combining these two modules with a bit of static HTML, JQuery and some JavaScript and you have an HTTP fileserver.

The project includes the static and the JavaScript and the Apache conf to set it up.

It does not install out of-the-box you need to be familiar with Apache config and do some copy pasteing.

Build is a bash script. It builds OK on my Fedora 32 bit and 64 bit systems, not tried it on any other distros.  Any volunteers to tidy that up is welcome, as are ports to any other OS.

mod_upload and mod_jsonindex are in no way dependent, you can run one without the other.

You can just run mod_upload and then you have a way to accept files and no way to download them again so some back end script can check for viruses or otherwise sanitize the data prior to publishing it.


Please report bugs if you find them.

mod_jsonindex is based on Core Apache code for mod_index so the same Apache 2 license applies to the code.
The original authors are, Rob McCool and Martin Pool they deserve the credit any bugs are mine.

mod_upload is GPL3.


P.S. If you just need JSON indexes, nginx has this built in.

