__GIT_HEAD   := $(shell git rev-parse --short HEAD)
__BUILD_DATE := $(shell date +"%d.%m.%Y")
__GIT_TAG    := $(shell git describe --tags --abbrev=0 | cut -dv -f2)

ifeq (,$(shell git status --porcelain))
VERSION_STRING_DATE := "$(__GIT_TAG) ($(__BUILD_DATE)) git: $(__GIT_HEAD)"
else
VERSION_STRING_DATE := "$(__GIT_TAG) ($(__BUILD_DATE)) git: $(__GIT_HEAD),dirty"
endif

export VERSION_STRING_DATE
