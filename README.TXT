
adapter_mongo.cc

Waitman Gobble <waitman@waitman.net>
San Jose California USA
May 2012

based on adapter_passthru.cc from www.e-cap.org samples, 
      The Measurement Factory
and licensed accordingly.

NO WARRANTIES


A C++ eCAP module that stores content in Mongo DB. This module may be loaded by Squid (or any eCAP compatible cache/proxy). Content is stored in chunks as received/processed. A GUID is generated using the Boost UUID library. The GUID should correspond to a unique request. Each chunk stored has a corresponding index #. So the content can be (re)-assembled by concatenating chunks with the same UUID in the order of the index. 


References
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
MongoDB				http://www.mongodb.org/
Squid				http://www.squid-cache.org/
eCAP support in Squid		http://wiki.squid-cache.org/Features/eCAP
eCAP library			http://www.e-cap.org/
eCAP downloads/examples		http://www.e-cap.org/Downloads
Mongo DB C++ Client Library	http://www.mongodb.org/pages/viewpage.action?pageId=133409
Boost				http://www.boost.org/



Notes using FreeBSD platform (gcc 4.6 w/ Boost)
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

# uname -a
FreeBSD hunny.waitman.net 10.0-CURRENT FreeBSD 10.0-CURRENT #0: Fri Apr 20 20:56:16 PDT 2012     
root@:/usr/obj/usr/src/sys/KEYSHIA  amd64


build and install Squid, edit squid.conf, then verify it will run

need squid 3.2.x
http://www.squid-cache.org/Versions/v3/3.2/

*requires GLIBCXX 3.4.15
env vars -- >
LD_LIBRARY_PATH=/usr/local/lib/gcc46:/usr/local/lib:/usr/lib
CC = gcc46 CXX=g++46

./configure --enable-ecap
make / install / etc

run/test (remember to initial cache store with -z first)
# setenv LD_LIBRARY_PATH /usr/local/lib/gcc46:/usr/local/lib:/usr/lib
# /usr/local/squid/sbin/squid -f /usr/local/squid/etc/squid.conf

# sockstat -4L
should be running on 3128 (and other ports)



Mongo DB C++ Driver

using mongo-cxx-driver-nightly
from 
mongodb-linux-x86_64-2.1.1.tgz
d/l: http://dl.mongodb.org/dl/cxx-driver/


patch SConstruct 
http://groups.google.com/group/mongodb-user/browse_thread/thread/19262557a84771f8?hl=en
or
https://github.com/waitman/mongo

build cxx driver
scons --cc=gcc46 --cxx=g++46



verify that driver is working using simple connect program example
http://www.mongodb.org/pages/viewpage.action?pageId=133415#C%2B%2BTutorial-Connecting

# g++46 -I/usr/local/include/mongo -I/usr/local/include testmongocxx.cpp \
	-L/usr/local/lib \
	-lmongoclient -lboost_thread -lboost_filesystem -lboost_program_options \
	-o trst
./trst
Abort (core dumped)


if error check that mongodb is running
# mongod --fork --logpath=/var/log/mongodb.log --dbpath=/home/mongodb
# ./trst 
connected ok



libecap
http://e-cap.org/Home
using libecap-0.2.0.tar.gz
setenv CC gcc46
setenv CXX g++46
./configure --with-pic --enable-shared=YES 
...

test using
ecap_adapter_sample-0.2.0

# pwd
/usr/home/waitman/downloads/ecap_adapter_sample-0.2.0/src

# g++46 -shared -DHAVE_CONFIG_H -fPIC adapter_passthru.cc -I../src \
-I/usr/local/include/mongo \
-I/usr/local/include -g -O1 -Wall -Wwrite-strings -L/usr/local/lib \
-L/usr/lib -lmongoclient -lboost_thread -lboost_filesystem -lboost_program_options -lecap \
-o pa.so



build adapter_mongo.c

g++46 -shared -fPIC adapter_mongo.cc \
	-I../src -I/usr/local/include/mongo -I/usr/local/include \
	-g -O1 -Wall -Wwrite-strings \
	-L/usr/local/lib/gcc64 L/usr/local/lib -L/usr/lib \
	-lmongoclient -lboost_thread -lboost_filesystem -lboost_program_options -lecap \
	-o adapter_mongo.so

Copy adapter_mongo.so to squid lib path and restart squid...
# cp adapter_mongo.so /usr/local/squid/lib/



squid.conf configuration file

#
cache_effective_user proxy

ecap_enable on
loadable_modules /usr/local/squid/lib/adapter_mongo.so
ecap_service eReqmod reqmod_precache 0 ecap://cudapang.com/ecap/services/mongo
ecap_service eRespmod respmod_precache 0 ecap://cudapang.com/ecap/services/mongo

adaptation_send_client_ip on
adaptation_service_set reqFilter eReqmod
adaptation_service_set respFilter eRespmod

adaptation_access respFilter allow all
adaptation_access reqFilter allow all
#



