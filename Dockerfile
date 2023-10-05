FROM ubuntu:bionic

RUN apt update
RUN apt install -y debian-archive-keyring curl gnupg apt-transport-https automake curl libtool make pkg-config python3-docutils autoconf-archive git nano
RUN curl -s -L https://packagecloud.io/varnishcache/varnish63/gpgkey | apt-key add -
RUN . /etc/os-release && \
tee /etc/apt/sources.list.d/varnishcache_varnish63.list > /dev/null <<-EOF
    deb https://packagecloud.io/varnishcache/varnish63/$ID/ $VERSION_CODENAME main
EOF
RUN tee /etc/apt/preferences.d/varnishcache > /dev/null <<EOF
Package: varnish varnish-*
Pin: release o=packagecloud.io/varnishcache/*
Pin-Priority: 1000
EOF
RUN apt-get update
RUN apt-get install -y varnish varnish-dev
RUN git clone https://github.com/varnish/toolbox.git; \
    cd toolbox; \
    git checkout $TOOLBOX_COMMIT; \
    cp install-vmod/install-vmod /usr/local/bin/;
CMD []
