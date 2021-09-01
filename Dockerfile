# syntax=docker/dockerfile:1
# Note: requires building with BuildKit. See
# https://docs.docker.com/develop/develop-images/build_enhancements/

FROM ubuntu:focal-20210827 as build

RUN apt-get -y update && DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends -y install \
        build-essential \
        g++ openjdk-11-jre-headless openjdk-11-jdk-headless python3 \
        libssl-dev libmysqlclient-dev maven \
        xsltproc docbook-xsl docbook-xml w3c-sgml-lib fop libxml2-utils \
        wget \
        python3-dev \
        python3-pip \
        python3-wheel \
        python3-setuptools \
        zlib1g-dev

ENV JAVA_HOME /usr/lib/jvm/java-11-openjdk-amd64

RUN mkdir -p /install/usr/sbin
RUN wget https://github.com/krallin/tini/releases/download/v0.19.0/tini -O /install/usr/sbin/tini && \
    chmod +x /install/usr/sbin/tini

# Install abacus. Copies are done piecemeal to make the build cache more
# effective.
COPY Makefile Makefile.inc /usr/src/abacuscm/
COPY src /usr/src/abacuscm/src
COPY include /usr/src/abacuscm/include
COPY doc /usr/src/abacuscm/doc
RUN cd /usr/src/abacuscm && make && make DESTDIR=/install install
COPY webapp /usr/src/abacuscm/webapp
RUN --mount=type=cache,target=/root/.m2 cd /usr/src/abacuscm/webapp && mvn
COPY . /usr/src/abacuscm
RUN mkdir -p \
       /install/etc/jetty9/contexts \
       /install/etc/jetty9/start.d \
       /install/var/cache/jetty9/data \
       /install/var/lib/jetty9/webapps \
       /install/usr/bin \
       /install/var/lib/abacuscm
RUN cp /usr/src/abacuscm/docker/webapps/*.xml /install/var/lib/jetty9/webapps/
RUN cp /usr/src/abacuscm/docker/abacuscm-secret-web.xml /install/etc/jetty9/
RUN cp /usr/src/abacuscm/docker/start.d/*.ini /install/etc/jetty9/start.d/
RUN cp /usr/src/abacuscm/docker/run.py /usr/src/abacuscm/docker/inichange.py /install/usr/bin/
RUN cp /usr/src/abacuscm/webapp/target/abacuscm-1.0-SNAPSHOT.war \
       /usr/src/abacuscm/conf/java.policy \
       /usr/src/abacuscm/docker/*.conf \
       /usr/src/abacuscm/db/structure.sql \
    /install/var/lib/abacuscm

# Install cx_Freeze
RUN pip3 install --root /install cx_Freeze==6.7


#######################################################################
# Runtime image, which copies artefacts from the build image

FROM ubuntu:focal-20210827
LABEL org.opencontainers.image.authors="Bruce Merry <bmerry@gmail.com>"

# Ensure we get the documentation we want
COPY docker/dpkg-excludes /etc/dpkg/dpkg.cfg.d/excludes

RUN apt-get -y update && DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends -y install \
        gcc g++ openjdk-11-jre-headless openjdk-11-jdk-headless python3 \
        libpython3.8 python3-distutils python3-setuptools patchelf \
        gcc-doc libstdc++-9-doc openjdk-11-doc python3-doc \
        cppreference-doc-en-html stl-manual \
        libssl1.1 libmysqlclient21 \
        openssl mariadb-server jetty9 supervisor sudo && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

ENV JAVA_HOME /usr/lib/jvm/java-11-openjdk-amd64

# Install from build image
COPY --from=build /install /

# Make mariadb use data from a mount, and set up run-dir for it
RUN sed -i 's!^datadir\s*= /var/lib/mysql!datadir = /data/mysql/db!' /etc/mysql/mariadb.conf.d/50-server.cnf
RUN mkdir /var/run/mysqld && chown mysql:mysql /var/run/mysqld

# Make logging go onto the mount, so that it is preserved
RUN rm -rf /var/log/supervisor /var/log/mysql /var/log/jetty9 && \
    ln -s /data/supervisor/log /var/log/supervisor && \
    ln -s /data/mysql/log /var/log/mysql && \
    ln -s /data/jetty9/log /var/log/jetty9 && \
    mv /usr/share/jetty9/webapps/root /www && \
    ln -s /data/standings /usr/share/jetty9/webapps/standings

# Make language documentation available
RUN DOC_DIR=/usr/share/jetty9/webapps/docs && \
    mkdir -p $DOC_DIR && \
    ln -s /usr/share/cppreference/doc/html/ $DOC_DIR/cppreference && \
    ln -s /usr/share/doc/stl-manual/html/ $DOC_DIR/stl-manual && \
    ln -s /usr/share/doc/python3-doc/html/ $DOC_DIR/python && \
    ln -s /usr/share/doc/openjdk-11-doc/api/ $DOC_DIR/java && \
    ln -s /usr/share/doc/gcc-9-base/libstdc++/ $DOC_DIR/libstdc++ && \
    mkdir -p $DOC_DIR/gcc && ln -s /usr/share/doc/gcc-9-base/*.html $DOC_DIR/gcc
COPY docker/doc/* /usr/share/jetty9/webapps/docs/

# Fix permissions
RUN chown -R jetty:adm /var/lib/jetty9 /var/cache/jetty9

# Create a user for abacus to run as, and users for an interactive tournament
RUN adduser --disabled-password --gecos 'abacus user' abacus && \
    adduser --no-create-home --shell /bin/false --disabled-login --gecos 'tournament user 1' sandbox1 && \
    adduser --no-create-home --shell /bin/false --disabled-login --gecos 'tournament user 2' sandbox2

VOLUME /conf /data /contest /www
EXPOSE 8080 8443 7368
ENTRYPOINT ["/usr/sbin/tini", "--", "/usr/bin/run.py"]
