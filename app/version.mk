DEVICE_NAME = JCA-123

# Version is taken from the last git tag "v<major>.<minor>.<patch>".
# Non-numeric tag, wrong field count, or no tag at all -> 0.0.0.
VERSION_TRIPLE := $(shell \
	t=$$(git describe --tags --abbrev=0 2>/dev/null); \
	t=$${t#v}; t=$${t#V}; \
	if echo "$$t" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$$'; then \
		echo "$$t" | tr '.' ' '; \
	else \
		echo "0 0 0"; \
	fi)

SW_VERSION_MAJOR ?= $(word 1,$(VERSION_TRIPLE))
SW_VERSION_MINOR ?= $(word 2,$(VERSION_TRIPLE))
SW_VERSION_PATCH ?= $(word 3,$(VERSION_TRIPLE))

SW_VERSION_NAME = $(SW_VERSION_MAJOR).$(SW_VERSION_MINOR).$(SW_VERSION_PATCH)

# Git build info appended to the version:
#   exactly on the last tag, clean -> ""        -> v<ver>
#   commits after the last tag     -> "-<hash>" -> v<ver>-<hash>
#   uncommitted changes            -> append "-dirty"
# Empty (plain v<ver>) when there is no tag or no git.
# NOTE: keep these shells free of a lone ')' -- make's $(shell) paren matching
# mis-parses e.g. a 'case ... )' pattern.
GIT_VERSION_SUFFIX := $(shell \
	d=$$(git describe --tags --long --dirty 2>/dev/null); \
	[ -n "$$d" ] || exit 0; \
	dirty=; \
	if [ "$${d%-dirty}" != "$$d" ]; then dirty=-dirty; d=$${d%-dirty}; fi; \
	t=$${d%-g*}; cnt=$${t##*-}; \
	if [ "$$cnt" = "0" ]; then printf '%s' "$$dirty"; \
	else printf '%s' "-$${d##*-g}$$dirty"; fi)

SW_VERSION_FULL = v$(SW_VERSION_NAME)$(GIT_VERSION_SUFFIX)

VERSION_DEFS = \
-DSW_VERSION_MAJOR=$(SW_VERSION_MAJOR) \
-DSW_VERSION_MINOR=$(SW_VERSION_MINOR) \
-DSW_VERSION_PATCH=$(SW_VERSION_PATCH) \
-DSW_VERSION_NAME=$(SW_VERSION_NAME) \
-DSW_VERSION_FULL='"'$(SW_VERSION_FULL)'"'
