# Create the super cache so modules will add themselves to it.
cache(, super)

TEMPLATE = subdirs
SUBDIRS = qtbase qttools qttranslations

load(qt_configure)
