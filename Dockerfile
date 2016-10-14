FROM ubuntu:xenial-20160914
MAINTAINER Bruce Merry <bmerry@gmail.com>

RUN apt-get -y update && DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends -y install \
    build-essential git-core sudo \
    g++ openjdk-8-jdk python2.7 python3 \
    gcc-doc libstdc++-5-doc openjdk-8-doc python-doc python3-doc \
    cppreference-doc-en-html stl-manual \
    build-essential libssl-dev libmysqlclient-dev maven \
    python-setuptools python3-setuptools python-pip python3-pip python-dev python3-dev \
    xsltproc docbook-xsl docbook-xml w3c-sgml-lib fop libxml2-utils \
    openssl mysql-server jetty8 supervisor wget && \
    apt-get clean

ENV JAVA_HOME /usr/lib/jvm/java-8-openjdk-amd64

# Allows mvn to use caching
COPY docker/settings.xml /root/.m2/settings.xml

# Install a bunch of mvn dependencies, so that they do not need to be
# re-fetched every time the abacus source changes
RUN for artifact in \
        args4j:args4j:2.0.16 \
        com.google.javascript:closure-compiler:v20130722 \
        com.samaxes.maven:minify-maven-plugin:1.7.1 \
        com.yahoo.platform.yui:yuicompressor:2.4.7 \
        commons-fileupload:commons-fileupload:1.3 \
        commons-io:commons-io:1.3.2 \
        javax.servlet:servlet-api:2.5 \
        org.apache.maven:maven-artifact:2.2.1 \
        org.apache.maven:maven-artifact-manager:2.2.1 \
        org.apache.maven:maven-error-diagnostics:2.2.1 \
        org.apache.maven:maven-monitor:2.2.1 \
        org.apache.maven:maven-monitor-manager:2.2.1 \
        org.apache.maven:maven-plugin-descriptor:2.2.1 \
        org.apache.maven:maven-plugin-parameter-documenter:2.2.1 \
        org.apache.maven:maven-profile:2.2.1 \
        org.apache.maven:maven-project:2.2.1 \
        org.apache.maven:maven-repository-metadata:2.2.1 \
        org.apache.maven:maven-settings:2.2.1 \
        org.apache.maven.plugins:maven-compiler-plugin:3.1 \
        org.apache.maven.plugins:maven-install-plugin:2.3 \
        org.apache.maven.plugins:maven-resources-plugin:2.3 \
        org.apache.maven.plugins:maven-surefire-plugin:2.10 \
        org.apache.maven.plugins:maven-war-plugin:2.3 \
        org.apache.maven.surefire:surefire-junit3:2.10 \
        org.apache.maven.plugins:maven-install-plugin:2.4 \
        org.apache.maven.plugins:maven-war-plugin:2.3 \
        org.codehaus.mojo:xml-maven-plugin:1.0 \
        org.cometd.java:bayeux-api:2.6.0 \
        org.cometd.java:cometd-java-server:2.6.0 \
        org.cometd.javascript:cometd-javascript-jquery:2.6.0:war \
        org.mortbay.jetty:maven-jetty-plugin:6.1.24 \
        org.slf4j:slf4j-jdk14:1.5.6 \
        org.slf4j:jcl-over-slf4j:1.5.6 \
        org.slf4j:slf4j-jdk14:1.7.5 \
        org.sonatype.forge:forge-parent:4 \
        org.sonatype.plexus:plexus-cipher:1.4 \
        org.sonatype.plexus:plexus-sec-dispatcher:1.3 \
    ; do cd ~ && mvn org.apache.maven.plugins:maven-dependency-plugin:2.8:get -Dartifact=$artifact; done

RUN wget https://github.com/krallin/tini/releases/download/v0.10.0/tini -O /sbin/tini && \
    chmod +x /sbin/tini

# Install cx_Freeze. Patches are needed to work around bugs:
# https://bitbucket.org/anthony_tuininga/cx_freeze/issues/32
# https://bitbucket.org/anthony_tuininga/cx_freeze/issues/156
# Can't just use the latest (as of writing, 2016-10-14) because of another bug:
# https://bitbucket.org/anthony_tuininga/cx_freeze/issues/192
COPY docker/cx_Freeze*.patch /tmp/cx_Freeze/
RUN pip download -d /tmp/cx_Freeze cx_Freeze==4.3.4 && \
    tar -C /tmp/cx_Freeze -zxf /tmp/cx_Freeze/cx_Freeze-4.3.4.tar.gz && \
    patch -d /tmp/cx_Freeze/cx_Freeze-4.3.4 -p1 < /tmp/cx_Freeze/cx_Freeze_debian.patch && \
    patch -d /tmp/cx_Freeze/cx_Freeze-4.3.4 -p1 < /tmp/cx_Freeze/cx_Freeze_py3.5.patch && \
    pip install /tmp/cx_Freeze/cx_Freeze-4.3.4 && \
    pip3 install /tmp/cx_Freeze/cx_Freeze-4.3.4 && \
    rm -rf /tmp/cx_Freeze

# Install abacus. Copies are done piecemeal to make the build cache more
# effective.
COPY Makefile Makefile.inc /usr/src/abacuscm/
COPY src /usr/src/abacuscm/src
COPY include /usr/src/abacuscm/include
COPY doc /usr/src/abacuscm/doc
RUN cd /usr/src/abacuscm && make && make install
COPY webapp /usr/src/abacuscm/webapp
RUN cd /usr/src/abacuscm/webapp && mvn
COPY . /usr/src/abacuscm
RUN cp /usr/src/abacuscm/docker/abacuscm.xml /etc/jetty8/contexts/abacuscm.xml && \
    cp /usr/src/abacuscm/docker/root.xml /etc/jetty8/contexts/root.xml && \
    cp /usr/src/abacuscm/docker/abacuscm-secret-web.xml /etc/jetty8/abacuscm-secret-web.xml && \
    cp /usr/src/abacuscm/docker/jetty*.xml /etc/jetty8/

# Make mysql use data from a mount
RUN sed -i 's!^datadir\s*= /var/lib/mysql!datadir = /data/mysql/db!' /etc/mysql/mysql.conf.d/mysqld.cnf

# Make logging go onto the mount, so that it is preserved
RUN rm -rf /var/log/supervisor /var/log/mysql /var/log/jetty8 && \
    ln -s /data/supervisor/log /var/log/supervisor && \
    ln -s /data/mysql/log /var/log/mysql && \
    ln -s /data/jetty8/log /var/log/jetty8 && \
    mv /usr/share/jetty8/webapps/root /www && \
    ln -s /data/standings /usr/share/jetty8/webapps/standings

# Make language documentation available
RUN DOC_DIR=/usr/share/jetty8/webapps/docs && \
    mkdir -p $DOC_DIR && \
    ln -s /usr/share/cppreference/doc/html/ $DOC_DIR/cppreference && \
    ln -s /usr/share/doc/stl-manual/html/ $DOC_DIR/stl-manual && \
    ln -s /usr/share/doc/python-doc/html/ $DOC_DIR/python2 && \
    ln -s /usr/share/doc/python3-doc/html/ $DOC_DIR/python3 && \
    ln -s /usr/share/doc/openjdk-8-doc/api/ $DOC_DIR/java && \
    ln -s /usr/share/doc/gcc-5-base/libstdc++/ $DOC_DIR/libstdc++ && \
    mkdir -p $DOC_DIR/gcc && ln -s /usr/share/doc/gcc-doc/*.html $DOC_DIR/gcc && \
    cp -r /usr/src/abacuscm/docker/doc/* $DOC_DIR/ && \
    rm /etc/jetty8/contexts/javadoc.xml
# Patch the webdefault.xml to allow symlinks for the docs
RUN sed -i '\!<param-name>aliases</param-name>!,+2 s!<param-value>false</param-value>!<param-value>true</param-value>!' /etc/jetty8/webdefault.xml

# Create a user for abacus to run as, and users for an interactive tournament
RUN adduser --disabled-password --gecos 'abacus user' abacus && \
    adduser --no-create-home --shell /bin/false --disabled-login --gecos 'tournament user 1' sandbox1 && \
    adduser --no-create-home --shell /bin/false --disabled-login --gecos 'tournament user 2' sandbox2

VOLUME /conf /data /contest /www
EXPOSE 8080 8443 7368
ENTRYPOINT ["/sbin/tini", "--", "/usr/src/abacuscm/docker/run.py"]
