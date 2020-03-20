AM_CFLAGS = $(VARNISHAPI_CFLAGS)

# Modules

vmod_LTLIBRARIES = \
	libvmod_dynmetrics.la

libvmod_dynmetrics_la_LDFLAGS = $(VMOD_LDFLAGS)
libvmod_dynmetrics_la_SOURCES = vmod_dynmetrics.c
nodist_libvmod_dynmetrics_la_SOURCES = \
	vcc_dynmetrics_if.c \
	vcc_dynmetrics_if.h

@BUILD_VMOD_DYNMETRICS@

# Test suite

AM_TESTS_ENVIRONMENT = \
	PATH="$(abs_builddir):$(VARNISH_TEST_PATH):$(PATH)" \
	LD_LIBRARY_PATH="$(VARNISH_LIBRARY_PATH)"
TEST_EXTENSIONS = .vtc
VTC_LOG_COMPILER = varnishtest -v
AM_VTC_LOG_FLAGS = \
	-p vcl_path="$(abs_top_srcdir)/vcl:$(VARNISHAPI_VCLDIR)" \
	-p vmod_path="$(abs_builddir)/.libs:$(vmoddir):$(VARNISHAPI_VMODDIR)"

TESTS = \
	vtc/vmod_dynmetrics.vtc

# Documentation

dist_doc_DATA = \
	vmod_dynmetrics.vcc \
	$(TESTS)

dist_man_MANS = \
	vmod_dynmetrics.3


.rst.1:
	$(AM_V_GEN) $(RST2MAN) $< $@
