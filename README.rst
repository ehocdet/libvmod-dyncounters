
==================================================
VMOD dyncounters - Varnish dynamic counters module
==================================================

:Author: Emmanuel Hocdet
:Date: 2020-04-17
:Version: 0.1
:Support Varnish Version: 6.2.x 6.3.x 6.4.x

DESCRIPTION
===========

`vmod_dyncounters` enables custom counters in a dynamic manner.
Dynamic counters can persist between VCL loads.
This module is designed to support many dynamic counters: you
must take care of cardinality and VSM memory.

vcl sample:
::

	import dyncounters;

	sub vcl_init {
	    new TEST = dyncounters.head();
	}
	sub vcl_recv {
	    TEST.incr(req.method, "suffix", 1);
	}

varnishstat -1 -f "TEST*":
::

	TEST.GET.suffix              1         0.00
	TEST.HEAD.suffix             2         0.00

vcl sample:
::

	import dyncounters;
	import proxy;

	sub vcl_init {
	    new SSL = dyncounters.head();
	    SSL.doc("req", format=integer, type=counter, level=info, oneliner="requests");
        }
	sub vcl_recv {
	    if (proxy.is_ssl()) {
               SSL.incr(proxy.ssl_version() + ":" + proxy.ssl_cipher() + ":" + req.proto, "req", 1);
	    }
	}

varnishstat -1 -f "SSL*":
::

    SSL.TLSv1.2:ECDHE-ECDSA-AES256-GCM-SHA384:HTTP/1.1.req            7         0.00 requests
    SSL.TLSv1.2:ECDHE-ECDSA-AES128-GCM-SHA256:HTTP/1.1.req            1         0.00 requests
    SSL.TLSv1.3:TLS_AES_128_GCM_SHA256:HTTP/1.1.req                   6         0.00 requests

.. _dyncounters.head():

new xhead = dyncounters.head()
------------------------------

Description
	Create a head of dynamic counters. The object name is the prefix
	of all counter names attached to the head. It appears as such in
	varnishstat, at the same level as MGT, MAIN, VBE, ...
	The head and its counters persist between VCLs as long as a VCL
	has vcl_init the object.

Example
	new DYNC = dyncounters.head();

.. _xhead.doc():

VOID xhead.doc(STRING suffix, ENUM format, ENUM type, ENUM level, STRING oneliner)
----------------------------------------------------------------------------------

::

      VOID xhead.doc(
            STRING suffix,
            ENUM {bitmap, bytes, duration, integer} format=integer,
            ENUM {bitmap, counter, gauge} type=counter,
            ENUM {info, debug, diag} level=info,
            STRING oneliner=""
      )

Description
	Create a documentation for all counters with suffix ``suffix``
	attached to the object head. This documentation is unique and
	persist with object head: any re-declaration of suffix will be
	ignored.

	``suffix``
	Counter suffix.

	``format``
	Counter format.

	``type``
	Counter type.

	``level``
	Counter level.

	``oneliner``
	Counter description.

Example
	DYNC.doc("req", format=integer, type=counter, level=info, oneliner="requests");

.. _xhead.incr():

VOID xhead.incr(STRING radical, STRING suffix, INT d)
-----------------------------------------------------

Description
	Increment value of counter "<head name>.<radical>.<suffix>"
	The counter is created on the fly if it does not exist.
	Documentation for "suffix" is created on the fly if it does
	not exist (with default values).
	Negative values are ignored.

Example
	DYNC.incr(req.method, "req", 1);

.. _xhead.decr():

VOID xhead.decr(STRING radical, STRING suffix, INT d)
-----------------------------------------------------

Description
	Decrement value of counter "<head name>.<radical>.<suffix>"
	The counter is created on the fly if it does not exist.
	Documentation for "suffix" is created on the fly if it does
	not exist (with default values).
	Negative values are ignored.

Example
	DYNC.decr(req.method, "req", 1);

.. _xhead.set():

VOID xhead.set(STRING radical, STRING suffix, INT d)
----------------------------------------------------

Description
	Set value of counter "<head name>.<radical>.<suffix>"
	The counter is created on the fly if it does not exist.
	Documentation for "suffix" is created on the fly if it does
	not exist (with default values).
	Negative values are ignored.

Example
	DYNC.set(req.method, "req", 1);


Compilation
---------------------

For other platforms you would use compilation.

The source tree is based on autotools to configure the building, and
does also have the necessary bits in place to do functional unit tests
using the ``varnishtest`` tool.

Building requires the Varnish header files and uses pkg-config to find
the necessary paths.

Usage::

 ./autogen.sh
 ./configure

If you have installed Varnish to a non-standard directory, call
``autogen.sh`` and ``configure`` with ``PKG_CONFIG_PATH`` pointing to
the appropriate path. For instance, when varnishd configure was called
with ``--prefix=$PREFIX``, use

::

 export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
 export ACLOCAL_PATH=${PREFIX}/share/aclocal

The module will inherit its prefix from Varnish, unless you specify a
different ``--prefix`` when running the ``configure`` script for this
module.

Make targets:

* make - builds the vmod.
* make install - installs your vmod.
* make check - runs the unit tests in ``src/tests/*.vtc``.
* make distcheck - run check and prepare a tarball of the vmod.

If you build a dist tarball, you don't need any of the autotools or
pkg-config. You can build the module simply by running::

 ./configure
 make

Installation directories
------------------------

By default, the vmod ``configure`` script installs the built vmod in the
directory relevant to the prefix. The vmod installation directory can be
overridden by passing the ``vmoddir`` variable to ``make install``.


COMMON PROBLEMS
===============

* configure: error: Need varnish.m4 -- see README.rst

  Check whether ``PKG_CONFIG_PATH`` and ``ACLOCAL_PATH`` were set correctly
  before calling ``autogen.sh`` and ``configure``

* Incompatibilities with different Varnish Cache versions

  Make sure you build this vmod against its correspondent Varnish Cache version.
  For instance, to build against Varnish Cache 4.1, this vmod must be built from
  branch 4.1.

* Require GCC

  This vmod using GCC Atomic builtins.
