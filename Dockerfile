FROM ubuntu:14.04
MAINTAINER Bruce Merry <bmerry@gmail.com>

# Add PPA for Oracle Java installer
RUN /bin/echo -e "deb http://ppa.launchpad.net/webupd8team/java/ubuntu trusty main\ndeb-src http://ppa.launchpad.net/webupd8team/java/ubuntu trusty main" > /etc/apt/sources.list.d/webupd8team-java.list && \
    apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys EEA14886 && \
    echo oracle-java8-installer shared/accepted-oracle-license-v1-1 select true | /usr/bin/debconf-set-selections

RUN apt-get -y update && DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends -y install \
    build-essential git-core \
    g++ oracle-java8-installer oracle-java8-set-default python2.7 python3 \
    build-essential libssl-dev libmysqlclient-dev maven \
    xsltproc docbook-xsl docbook-xml w3c-dtd-xhtml fop libxml2-utils \
    openssl mysql-server jetty8 supervisor && \
    apt-get clean

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

# Fix https://bugs.launchpad.net/ubuntu/+source/w3c-dtd-xhtml/+bug/400259
RUN find /usr/share/xml/xhtml /usr/share/xml/entities/xhtml -name catalog.xml -exec sed -i 's!http://globaltranscorp.org/oasis/catalog/xml/tr9401\.dtd!file:////usr/share/xml/schema/xml-core/tr9401.dtd!g' '{}' ';'
# Make Oracle JDK be found
ENV JAVA_HOME /usr/lib/jvm/java-8-oracle

# Install abacus. Copies are done piecemeal to make the build cache more
# effective.
COPY Makefile Makefile.inc /usr/src/abacuscm/
COPY src /usr/src/abacuscm/src
COPY include /usr/src/abacuscm/include
COPY doc /usr/src/abacuscm/doc
# chown/chmod are to make runlimit suid root
RUN cd /usr/src/abacuscm && make && make install
COPY webapp /usr/src/abacuscm/webapp
RUN cd /usr/src/abacuscm/webapp && mvn
COPY . /usr/src/abacuscm
RUN cp /usr/src/abacuscm/docker/abacuscm.xml /etc/jetty8/contexts/abacuscm.xml && \
    cp /usr/src/abacuscm/docker/abacuscm-secret-web.xml /etc/jetty8/abacuscm-secret-web.xml && \
    cp /usr/src/abacuscm/docker/jetty*.xml /etc/jetty8/

# Make mysql use data from a mount
RUN sed -i 's!^datadir\s*= /var/lib/mysql!datadir = /data/mysql!' /etc/mysql/my.cnf

# Make logging go onto the mount, so that it is preserved
RUN rm -rf /var/log/supervisor /var/log/mysql /usr/share/jetty8/logs && \
    ln -s /data/supervisor/log /var/log/supervisor && \
    ln -s /data/mysql/log /var/log/mysql && \
    ln -sf /data/jetty8/log /usr/share/jetty8/logs

# Create a user for abacus to run as
RUN adduser --disabled-password --gecos 'abacus user' abacus

VOLUME /conf
VOLUME /data
VOLUME /contest
EXPOSE 8080
EXPOSE 8443
EXPOSE 7368
ENTRYPOINT ["/usr/src/abacuscm/docker/run.py"]
