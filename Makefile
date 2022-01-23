# Makefile to build the 'fftease' library for Pure Data.
# Needs Makefile.pdlibbuilder as helper makefile for platform-dependent build
# settings and rules.

# library name
lib.name = purrfunk

# sources for the objectclasses
class.sources = \
	overtones.c \
	$(empty)

# extra files
datafiles = \
	martha/kill-old-abs.pd \
	martha/martha~-help.pd \
	martha/martha~.pd \
	martha/oboe.wav \
	martha/osc-bank-voice.pd \
	martha/send-update-abs.pd \
	martha/start-new-abs.pd \
	martha/voice2.wav \
	martha/voice.wav \
	overtones~-help.pd \
	abs/adjust-track.pd \
	$(empty)

# extra dirs
#datadirs = sound \
#	$(empty)

# include the actual build-system
PDLIBBUILDER_DIR=pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

test:
	cd tests; sh runtests.sh
