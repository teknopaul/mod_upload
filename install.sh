#!/bin/bash
#
# Install the modules and their icons
#

if [ "$1" == "" ] ; then
  echo "by default this does nothing since I dont know you apache directory layout"
  echo "to accept redhat defaults: $0 go"
  echo "default are: $0 --conf /etc/httpd/conf.d --res /var/www/res --html /var/www/html --user apache --modules /usr/lib/httpd/modules"
  echo "otherwise: $0 " '--conf [apache conf.d dir] --res [resources dir] --html [html dir] --user [apache user] --modules [apache module sdir]'
  exit 0
fi

# defaults based on RedHat distro
DEFAULT_CONF_D=/etc/httpd/conf.d
DEFAULT_RES=/var/www/res
DEFAULT_HTML=/var/www/html
DEFAULT_MODULES=/usr/lib/httpd/modules
DEFAULT_USER=apache

CONF_D=$DEFAULT_CONF_D
RES=$DEFAULT_RES
HTML=$DEFAULT_HTML
MODULES=$DEFAULT_MODULES
APACHE_USER=$DEFAULT_USER

while [ "$1" != "" ] ;
do
if [ "$1" == "--conf" ] ; then
        CONF_D=$2
fi
if [ "$1" == "--res" ] ; then
        RES=$2
fi
if [ "$1" == "--html" ] ; then
        HTML=$2
fi
if [ "$1" == "--user" ] ; then
        APACHE_USER=$2
fi
if [ "$1" == "--modules" ] ; then
        MODULES=$2
fi
shift
done

echo "Using --conf:$CONF_D --res:$RES --html:$HTML --user:$APACHE_USER --modules:$MODULES"

cd `dirname $0`

if [ ! -d "$CONF_D" ] ; then
  echo "cannot find $CONF_D"
  exit 1
fi

if [ ! -d "$MODULES" ] ; then
  echo "cannot find $MODULES"
  exit 1
fi

grep $APACHE_USER /etc/passwd > /dev/null
if [ $? != 0 ] ; then
  echo "user not found: $APACHE_USER"
  exit 1
fi



mkdir -p $RES $HTML

echo copying files
cp -Ri *.so $MODULES
cp -Ri static/html/* $HTML
cp -Ri static/res/* $RES
cp -Ri conf.d/* $CONF_D

echo Making fies readable by $APACHE_USER
chown -R $APACHE_USER $RES/*
chmod -R u+r $RES/*
chmod u+rx $RES

chown -R $APACHE_USER $HTML/*
chmod -R u+r $HTML/*.html
chmod u+rx $HTML

# root generally owns the config


