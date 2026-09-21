ASSETS ?= $(HOME)/Downloads/SpiderMan_Native_ARM64_Port_Workbench/Assets
WB     ?= $(HOME)/Downloads/SpiderMan_Native_ARM64_Port_Workbench
TEAM   ?= BHV8AWKA75

.PHONY: check test verify regen build sync

check:        ## compile check like CI
	Tools/check_staged.sh

test:         ## host suites against $(ASSETS)
	hosttests/run_host_tests.sh $(ASSETS)

verify:       ## asset tree sanity
	Tools/verify_assets.sh $(ASSETS)

sync:         ## copy sources to the workbench
	cp NativePort/Sources/* $(WB)/NativePort/Sources/

regen: sync   ## regenerate the Xcode project (needed for new files or assets)
	cd $(WB)/build-ios && cmake -G Xcode ../NativePort -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=$(TEAM) -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic >/dev/null && echo REGENERATED

build: sync   ## build for device (no signing)
	cd $(WB)/build-ios && xcodebuild -project SpiderManTotalMayhemNative.xcodeproj -target SpiderManTotalMayhem -sdk iphoneos CODE_SIGNING_ALLOWED=NO build 2>&1 | grep -E 'error:|BUILD'
