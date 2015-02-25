FROM ubuntu:14.04
MAINTAINER Bruce Merry <bmerry@gmail.com>

# Add PPA for Oracle Java installer
RUN /bin/echo -e "deb http://ppa.launchpad.net/webupd8team/java/ubuntu trusty main\ndeb-src http://ppa.launchpad.net/webupd8team/java/ubuntu trusty main" > /etc/apt/sources.list.d/webupd8team-java.list && \
    apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys EEA14886 && \
    echo oracle-java8-installer shared/accepted-oracle-license-v1-1 select true | /usr/bin/debconf-set-selections

RUN apt-get -y update && DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends -y install \
    build-essential git-core \
    g++ oracle-java8-installer python3 \
    build-essential libssl-dev libmysqlclient-dev maven \
    xsltproc docbook-xsl w3c-dtd-xhtml fop libxml2-utils \
    mysql-server jetty

# Fix https://bugs.launchpad.net/ubuntu/+source/w3c-dtd-xhtml/+bug/400259
RUN find /usr/share/xml/xhtml /usr/share/xml/entities/xhtml -name catalog.xml -exec sed -i 's!http://globaltranscorp.org/oasis/catalog/xml/tr9401\.dtd!file:////usr/share/xml/schema/xml-core/tr9401.dtd!g' '{}' ';'

# Install abacus
COPY . abacuscm
RUN cd abacuscm && make && make install
