# Music Impactor - dodaje pobrane piosenki do aplikacji Muzyka (iOS 6, jailbreak), budowany w Theosie.
#
# SDKVER: wersja SDK iOS, z którą kompilujemy. Skrypt build.sh sam wybiera
# najstarsze dostępne. Ręcznie: make package SDKVER=9.3
SDKVER ?= 6.1

# armv7 działa na iPhone 3GS/4/4S/5 i iPod touch 4/5 (iOS 6.x)
export ARCHS = armv7
export TARGET = iphone:clang:$(SDKVER):6.0

# stary dpkg z iOS 6 nie czyta nowszej kompresji niż gzip
export THEOS_PLATFORM_DEB_COMPRESSION_TYPE = gzip

INSTALL_TARGET_PROCESSES = MusicImpactor

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = MusicImpactor

MusicImpactor_FILES = main.m AppDelegate.m MIStore.m MITheme.m MIViews.m MILang.m MICrash.m MILibrary.m MIDownloads.m MIQueueController.m MIEditController.m MIFilesController.m MIImportedController.m MITextController.m MISettingsController.m mi_core.c mi_lib.c mi_libc.c
MusicImpactor_FRAMEWORKS = UIKit Foundation CoreGraphics QuartzCore AVFoundation
MusicImpactor_LIBRARIES = sqlite3
MusicImpactor_CFLAGS = -fobjc-arc -fno-stack-protector -D_FORTIFY_SOURCE=0 -Wno-deprecated-declarations -Wno-unused-command-line-argument

include $(THEOS_MAKE_PATH)/application.mk

after-install::
	install.exec "uicache"
