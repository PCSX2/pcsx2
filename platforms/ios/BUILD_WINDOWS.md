Windows:
	git clone https://github.com/ARMSX2/ARMSX2.git
	git submodule init
	git submodule update
	
	cd app\src\main\cpp\3rdparty\libadrenotools
	git submodule init
	git submodule update

NPM Packages:
	npm install --legacy-peer-deps

Build: react-native-gradle-plugin
	cd node_modules/@react-native/gradle-plugin
	npm install
	gradlew.bat build

Change: node_modules\@assembless\react-native-material-you\android\build.gradle
android {
    compileSdkVersion 36

    defaultConfig {
        minSdkVersion 34
        targetSdkVersion 34
    }
}

gradlew.bat assembleDebug -PenableRN=true